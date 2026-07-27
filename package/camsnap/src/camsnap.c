/* camsnap — lightweight live-view + calibration daemon for the SMT1019.
 *
 * Owns the rkisp mainpath (/dev/video11): streams NV12 continuously and
 * publishes
 *   /dev/shm/cam.jpg        — JPEG of the latest frame (~10 fps), for the
 *                             an application live view (e.g. over HTTP)
 *   /dev/shm/cam_stats.json — live mean Y/U/V + derived linear-ish RGB of a
 *                             center patch (25% box), computed from the RAW
 *                             NV12 before JPEG — the white-balance
 *                             calibration instrument.
 * Files are written to a temp name and rename()d so readers never see a
 * partial file.
 *
 * The rkaiq 3A daemon must be running for auto exposure/AWB; camsnap only
 * consumes frames. On startup it sets the sensor's vertical blanking to
 * >=1000us (the RK3576 CIF-online minimum) in case 3A isn't up yet — see
 * package/rkaiq/README.md.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <linux/videodev2.h>
#include <jpeglib.h>

#define VIDEO_DEV "/dev/video11"
#define SENSOR_SUBDEV "/dev/v4l-subdev3"
#define WIDTH 1280
#define HEIGHT 720
#define NBUF 4
#define JPEG_QUALITY 80
#define JPEG_PERIOD_MS 100         /* ~10 fps live view */
#define JPG_PATH "/dev/shm/cam.jpg"
#define STATS_PATH "/dev/shm/cam_stats.json"

static volatile sig_atomic_t stop;
static void on_sig(int s) { (void)s; stop = 1; }

static long now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

/* Best-effort: raise vblank above the CIF-online 1000us minimum (the gc5035
 * mode default is 933us). The 3A engine manages this itself once running. */
static void set_vblank(void) {
  int fd = open(SENSOR_SUBDEV, O_RDWR);
  struct v4l2_control c = { .id = V4L2_CID_VBLANK, .value = 120 };
  if (fd < 0) return;
  ioctl(fd, VIDIOC_S_CTRL, &c);
  close(fd);
}

static int write_atomic(const char *path, const unsigned char *data, size_t len) {
  char tmp[128];
  int fd;
  snprintf(tmp, sizeof(tmp), "%s.tmp", path);
  fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) return -1;
  if (write(fd, data, len) != (ssize_t)len) { close(fd); return -1; }
  close(fd);
  return rename(tmp, path);
}

/* NV12 -> JPEG via libjpeg raw YCbCr input (deinterleave UV once). */
static unsigned char *jpg_buf;
static unsigned long jpg_len;
static unsigned char *uplane, *vplane;

static int encode_jpeg(const unsigned char *nv12) {
  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  JSAMPROW y_rows[16], u_rows[8], v_rows[8];
  JSAMPARRAY planes[3] = { y_rows, u_rows, v_rows };
  const unsigned char *y = nv12, *uv = nv12 + WIDTH * HEIGHT;
  unsigned int i;

  for (i = 0; i < (WIDTH * HEIGHT) / 4; i++) {
    uplane[i] = uv[2 * i];
    vplane[i] = uv[2 * i + 1];
  }

  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);
  if (jpg_buf) { free(jpg_buf); jpg_buf = NULL; }
  jpg_len = 0;
  jpeg_mem_dest(&cinfo, &jpg_buf, &jpg_len);
  cinfo.image_width = WIDTH;
  cinfo.image_height = HEIGHT;
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_YCbCr;
  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, JPEG_QUALITY, TRUE);
  cinfo.raw_data_in = TRUE;
  cinfo.comp_info[0].h_samp_factor = 2;
  cinfo.comp_info[0].v_samp_factor = 2;
  cinfo.comp_info[1].h_samp_factor = 1;
  cinfo.comp_info[1].v_samp_factor = 1;
  cinfo.comp_info[2].h_samp_factor = 1;
  cinfo.comp_info[2].v_samp_factor = 1;
  jpeg_start_compress(&cinfo, TRUE);

  while (cinfo.next_scanline < HEIGHT) {
    unsigned int j, line = cinfo.next_scanline;
    for (j = 0; j < 16; j++)
      y_rows[j] = (JSAMPROW)(y + (line + j) * WIDTH);
    for (j = 0; j < 8; j++) {
      u_rows[j] = uplane + ((line / 2) + j) * (WIDTH / 2);
      v_rows[j] = vplane + ((line / 2) + j) * (WIDTH / 2);
    }
    jpeg_write_raw_data(&cinfo, planes, 16);
  }
  jpeg_finish_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);
  return 0;
}

/* Mean Y/U/V of the center 25% box + BT.601 RGB, written as JSON. */
static void write_stats(const unsigned char *nv12, unsigned int seq) {
  const unsigned char *yp = nv12, *uv = nv12 + WIDTH * HEIGHT;
  unsigned long sy = 0, su = 0, sv = 0;
  unsigned int x0 = WIDTH * 3 / 8, x1 = WIDTH * 5 / 8;
  unsigned int y0 = HEIGHT * 3 / 8, y1 = HEIGHT * 5 / 8;
  unsigned int x, yy, n = 0;
  char json[512];
  double my, mu, mv, r, g, b;

  for (yy = y0; yy < y1; yy++)
    for (x = x0; x < x1; x++) { sy += yp[yy * WIDTH + x]; n++; }
  my = (double)sy / n;
  n = 0;
  for (yy = y0 / 2; yy < y1 / 2; yy++)
    for (x = x0 / 2; x < x1 / 2; x++) {
      su += uv[yy * WIDTH + 2 * x];
      sv += uv[yy * WIDTH + 2 * x + 1];
      n++;
    }
  mu = (double)su / n;
  mv = (double)sv / n;
  r = my + 1.402 * (mv - 128);
  g = my - 0.344136 * (mu - 128) - 0.714136 * (mv - 128);
  b = my + 1.772 * (mu - 128);
  snprintf(json, sizeof(json),
           "{\"seq\":%u,\"y\":%.1f,\"u\":%.1f,\"v\":%.1f,"
           "\"r\":%.1f,\"g\":%.1f,\"b\":%.1f,"
           "\"r_g\":%.3f,\"b_g\":%.3f}\n",
           seq, my, mu, mv, r, g, b,
           g > 1 ? r / g : 0, g > 1 ? b / g : 0);
  write_atomic(STATS_PATH, (unsigned char *)json, strlen(json));
}

int main(void) {
  int fd, i;
  struct v4l2_format fmt = {0};
  struct v4l2_requestbuffers req = {0};
  struct { void *start[VIDEO_MAX_PLANES]; size_t len[VIDEO_MAX_PLANES]; } bufs[NBUF];
  long last_jpg = 0;

  signal(SIGTERM, on_sig);
  signal(SIGINT, on_sig);

  uplane = malloc((WIDTH * HEIGHT) / 4);
  vplane = malloc((WIDTH * HEIGHT) / 4);
  if (!uplane || !vplane) return 1;

  set_vblank();

  fd = open(VIDEO_DEV, O_RDWR);
  if (fd < 0) { perror("open video"); return 1; }

  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  fmt.fmt.pix_mp.width = WIDTH;
  fmt.fmt.pix_mp.height = HEIGHT;
  fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12;
  fmt.fmt.pix_mp.num_planes = 1;
  if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) { perror("s_fmt"); return 1; }

  /* vblank again after set_fmt: the sensor driver resets it on mode set */
  set_vblank();

  req.count = NBUF;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
  req.memory = V4L2_MEMORY_MMAP;
  if (ioctl(fd, VIDIOC_REQBUFS, &req) < 0) { perror("reqbufs"); return 1; }

  for (i = 0; i < NBUF; i++) {
    struct v4l2_buffer b = {0};
    struct v4l2_plane planes[VIDEO_MAX_PLANES] = {{0}};
    unsigned int p;
    b.type = req.type;
    b.memory = V4L2_MEMORY_MMAP;
    b.index = i;
    b.length = VIDEO_MAX_PLANES;
    b.m.planes = planes;
    if (ioctl(fd, VIDIOC_QUERYBUF, &b) < 0) { perror("querybuf"); return 1; }
    for (p = 0; p < b.length && planes[p].length; p++) {
      bufs[i].len[p] = planes[p].length;
      bufs[i].start[p] = mmap(NULL, planes[p].length, PROT_READ | PROT_WRITE,
                              MAP_SHARED, fd, planes[p].m.mem_offset);
      if (bufs[i].start[p] == MAP_FAILED) { perror("mmap"); return 1; }
    }
    if (ioctl(fd, VIDIOC_QBUF, &b) < 0) { perror("qbuf"); return 1; }
  }

  {
    enum v4l2_buf_type t = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (ioctl(fd, VIDIOC_STREAMON, &t) < 0) { perror("streamon"); return 1; }
  }

  while (!stop) {
    struct v4l2_buffer b = {0};
    struct v4l2_plane planes[VIDEO_MAX_PLANES] = {{0}};
    b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    b.memory = V4L2_MEMORY_MMAP;
    b.length = VIDEO_MAX_PLANES;
    b.m.planes = planes;
    if (ioctl(fd, VIDIOC_DQBUF, &b) < 0) {
      if (errno == EINTR) continue;
      perror("dqbuf");
      break;                       /* supervisor restarts us */
    }
    if (b.index < NBUF) {
      const unsigned char *frame = bufs[b.index].start[0];
      long t = now_ms();
      if (t - last_jpg >= JPEG_PERIOD_MS) {
        last_jpg = t;
        write_stats(frame, b.sequence);
        if (encode_jpeg(frame) == 0 && jpg_len > 0)
          write_atomic(JPG_PATH, jpg_buf, jpg_len);
      }
    }
    if (ioctl(fd, VIDIOC_QBUF, &b) < 0) { perror("requeue"); break; }
  }

  {
    enum v4l2_buf_type t = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    ioctl(fd, VIDIOC_STREAMOFF, &t);
  }
  close(fd);
  return 0;
}
