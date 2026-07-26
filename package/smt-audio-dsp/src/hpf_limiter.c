/* smt_hpf_limiter — LADSPA voicing + protection plugin for the SMT1019's 2 W
 * built-in speaker (wall-mounted smart panel: voice/intercom, notifications,
 * occasional music).
 *
 * Per-channel signal chain:
 *
 *   4th-order Butterworth high-pass         (woofer excursion protection)
 *     -> low-shelf   "warmth"   (~250 Hz)   (restore body just above the HPF)
 *     -> peaking     "presence" (~2.5 kHz)  (speech intelligibility across a room)
 *     -> high-shelf  "air"      (~8 kHz)    (crispness for clicks/chimes)
 *     -> makeup gain                        (loudness)
 *     -> tanh soft-limiter (ceiling)        (loudness without hard clipping)
 *
 * Every stage is a biquad (RBJ cookbook) so it is cheap and stable. All bands
 * are exposed as LADSPA control ports, so the whole voicing is tunable from
 * /etc/asound.conf (or a ~/.asoundrc override) with no recompile. Any EQ band
 * at 0 dB is an exact unity no-op, so a flat config == pure HPF + limiter.
 *
 * It is a mono (1-in/1-out) plugin; the ALSA `type ladspa` host instantiates
 * one per channel.
 */

#include <math.h>
#include <stdlib.h>

#include "ladspa.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Control ports, in the order asound.conf `controls [ ... ]` must list them:
 *   [ cutoff_Hz  warmth_Hz warmth_dB  presence_Hz presence_Q presence_dB
 *     air_Hz air_dB  makeup_dB  ceiling ] */
enum {
  P_CUTOFF = 0,
  P_WARMTH_HZ,
  P_WARMTH_DB,
  P_PRESENCE_HZ,
  P_PRESENCE_Q,
  P_PRESENCE_DB,
  P_AIR_HZ,
  P_AIR_DB,
  P_MAKEUP,
  P_CEILING,
  P_INPUT,
  P_OUTPUT,
  PORT_COUNT
};

/* Biquad sections in series. Two HPF sections make the 4th-order Butterworth. */
enum { B_HP1 = 0, B_HP2, B_WARMTH, B_PRESENCE, B_AIR, NBIQ };

typedef struct {
  double b0, b1, b2, a1, a2; /* coeffs, normalized by a0 */
  double x1, x2, y1, y2;     /* direct-form-I state */
} Biquad;

typedef struct {
  LADSPA_Data *port[PORT_COUNT];
  double sr;
  Biquad bq[NBIQ];
  /* cached control values so coeffs are only recomputed when something moves */
  double c_cut, c_whz, c_wdb, c_phz, c_pq, c_pdb, c_ahz, c_adb;
  int primed;
} Plugin;

/* ---- RBJ cookbook biquads (all store a1,a2 = denominator/a0, subtracted in
 * the difference equation: y = b0 x + b1 x1 + b2 x2 - a1 y1 - a2 y2) ---- */

static void bq_highpass(Biquad *b, double sr, double fc, double q) {
  double w0 = 2.0 * M_PI * fc / sr, cw = cos(w0), sw = sin(w0);
  double alpha = sw / (2.0 * q), a0 = 1.0 + alpha;
  b->b0 = ((1.0 + cw) * 0.5) / a0;
  b->b1 = (-(1.0 + cw)) / a0;
  b->b2 = ((1.0 + cw) * 0.5) / a0;
  b->a1 = (-2.0 * cw) / a0;
  b->a2 = (1.0 - alpha) / a0;
}

static void bq_peaking(Biquad *b, double sr, double fc, double q, double db) {
  double A = pow(10.0, db / 40.0);
  double w0 = 2.0 * M_PI * fc / sr, cw = cos(w0), sw = sin(w0);
  double alpha = sw / (2.0 * q), a0 = 1.0 + alpha / A;
  b->b0 = (1.0 + alpha * A) / a0;
  b->b1 = (-2.0 * cw) / a0;
  b->b2 = (1.0 - alpha * A) / a0;
  b->a1 = (-2.0 * cw) / a0;
  b->a2 = (1.0 - alpha / A) / a0;
}

static void bq_lowshelf(Biquad *b, double sr, double fc, double q, double db) {
  double A = pow(10.0, db / 40.0);
  double w0 = 2.0 * M_PI * fc / sr, cw = cos(w0), sw = sin(w0);
  double alpha = sw / (2.0 * q), tsa = 2.0 * sqrt(A) * alpha;
  double a0 = (A + 1.0) + (A - 1.0) * cw + tsa;
  b->b0 = (A * ((A + 1.0) - (A - 1.0) * cw + tsa)) / a0;
  b->b1 = (2.0 * A * ((A - 1.0) - (A + 1.0) * cw)) / a0;
  b->b2 = (A * ((A + 1.0) - (A - 1.0) * cw - tsa)) / a0;
  b->a1 = (-2.0 * ((A - 1.0) + (A + 1.0) * cw)) / a0;
  b->a2 = ((A + 1.0) + (A - 1.0) * cw - tsa) / a0;
}

static void bq_highshelf(Biquad *b, double sr, double fc, double q, double db) {
  double A = pow(10.0, db / 40.0);
  double w0 = 2.0 * M_PI * fc / sr, cw = cos(w0), sw = sin(w0);
  double alpha = sw / (2.0 * q), tsa = 2.0 * sqrt(A) * alpha;
  double a0 = (A + 1.0) - (A - 1.0) * cw + tsa;
  b->b0 = (A * ((A + 1.0) + (A - 1.0) * cw + tsa)) / a0;
  b->b1 = (-2.0 * A * ((A - 1.0) + (A + 1.0) * cw)) / a0;
  b->b2 = (A * ((A + 1.0) + (A - 1.0) * cw - tsa)) / a0;
  b->a1 = (2.0 * ((A - 1.0) - (A + 1.0) * cw)) / a0;
  b->a2 = ((A + 1.0) - (A - 1.0) * cw - tsa) / a0;
}

/* Shelves use a Butterworth-ish slope (Q ~ 0.707); the 4th-order HP uses the
 * two standard Butterworth section Qs. */
#define SHELF_Q 0.70710678

static double val(const Plugin *p, int port, double dflt) {
  const LADSPA_Data *d = p->port[port];
  return d ? (double)*d : dflt;
}

static void recompute(Plugin *p) {
  static const double hpq[2] = {0.54119610, 1.30656296}; /* 4th-order Butterworth */
  double nyq = p->sr * 0.45;
  double cut = val(p, P_CUTOFF, 180.0);
  double whz = val(p, P_WARMTH_HZ, 250.0), wdb = val(p, P_WARMTH_DB, 0.0);
  double phz = val(p, P_PRESENCE_HZ, 2500.0), pq = val(p, P_PRESENCE_Q, 1.0);
  double pdb = val(p, P_PRESENCE_DB, 0.0);
  double ahz = val(p, P_AIR_HZ, 8000.0), adb = val(p, P_AIR_DB, 0.0);

  if (cut < 20.0) cut = 20.0;
  if (cut > nyq) cut = nyq;
  if (whz > nyq) whz = nyq;
  if (phz > nyq) phz = nyq;
  if (ahz > nyq) ahz = nyq;
  if (pq < 0.1) pq = 0.1;

  bq_highpass(&p->bq[B_HP1], p->sr, cut, hpq[0]);
  bq_highpass(&p->bq[B_HP2], p->sr, cut, hpq[1]);
  bq_lowshelf(&p->bq[B_WARMTH], p->sr, whz, SHELF_Q, wdb);
  bq_peaking(&p->bq[B_PRESENCE], p->sr, phz, pq, pdb);
  bq_highshelf(&p->bq[B_AIR], p->sr, ahz, SHELF_Q, adb);

  p->c_cut = cut; p->c_whz = whz; p->c_wdb = wdb;
  p->c_phz = phz; p->c_pq = pq; p->c_pdb = pdb;
  p->c_ahz = ahz; p->c_adb = adb;
  p->primed = 1;
}

static int controls_moved(const Plugin *p) {
  return !p->primed ||
         val(p, P_CUTOFF, 180.0) != p->c_cut ||
         val(p, P_WARMTH_HZ, 250.0) != p->c_whz ||
         val(p, P_WARMTH_DB, 0.0) != p->c_wdb ||
         val(p, P_PRESENCE_HZ, 2500.0) != p->c_phz ||
         val(p, P_PRESENCE_Q, 1.0) != p->c_pq ||
         val(p, P_PRESENCE_DB, 0.0) != p->c_pdb ||
         val(p, P_AIR_HZ, 8000.0) != p->c_ahz ||
         val(p, P_AIR_DB, 0.0) != p->c_adb;
}

static LADSPA_Handle instantiate(const LADSPA_Descriptor *d, unsigned long sr) {
  Plugin *p = calloc(1, sizeof(Plugin));
  (void)d;
  if (!p) return NULL;
  p->sr = (double)sr;
  return p;
}

static void connect_port(LADSPA_Handle inst, unsigned long port, LADSPA_Data *data) {
  Plugin *p = (Plugin *)inst;
  if (port < PORT_COUNT) p->port[port] = data;
}

static void activate(LADSPA_Handle inst) {
  Plugin *p = (Plugin *)inst;
  int i;
  for (i = 0; i < NBIQ; i++) {
    p->bq[i].x1 = p->bq[i].x2 = p->bq[i].y1 = p->bq[i].y2 = 0.0;
  }
  p->primed = 0;
}

static void run(LADSPA_Handle inst, unsigned long n) {
  Plugin *p = (Plugin *)inst;
  const LADSPA_Data *in = p->port[P_INPUT];
  LADSPA_Data *out = p->port[P_OUTPUT];
  double g = pow(10.0, val(p, P_MAKEUP, 0.0) / 20.0);
  double ceil = val(p, P_CEILING, 1.0);
  double inv_ceil;
  unsigned long i;
  int s;

  if (ceil < 0.05) ceil = 0.05;
  if (ceil > 1.0) ceil = 1.0;
  inv_ceil = 1.0 / ceil;

  if (controls_moved(p)) recompute(p);

  for (i = 0; i < n; i++) {
    double v = (double)in[i];

    for (s = 0; s < NBIQ; s++) {
      Biquad *b = &p->bq[s];
      double y = b->b0 * v + b->b1 * b->x1 + b->b2 * b->x2 - b->a1 * b->y1 - b->a2 * b->y2;
      b->x2 = b->x1; b->x1 = v;
      b->y2 = b->y1; b->y1 = y;
      v = y;
    }

    v *= g;
    v = ceil * tanh(v * inv_ceil);
    out[i] = (LADSPA_Data)v;
  }
}

static void cleanup(LADSPA_Handle inst) { free(inst); }

/* ---- descriptor ---- */

static const char *const port_names[PORT_COUNT] = {
    "Cutoff (Hz)",   "Warmth Freq (Hz)",  "Warmth (dB)",
    "Presence Freq (Hz)", "Presence Q",   "Presence (dB)",
    "Air Freq (Hz)", "Air (dB)",          "Makeup (dB)",
    "Ceiling",       "Input",             "Output"};

#define CTRL (LADSPA_PORT_INPUT | LADSPA_PORT_CONTROL)
static const LADSPA_PortDescriptor port_descriptors[PORT_COUNT] = {
    CTRL, CTRL, CTRL, CTRL, CTRL, CTRL, CTRL, CTRL, CTRL, CTRL,
    LADSPA_PORT_INPUT | LADSPA_PORT_AUDIO,
    LADSPA_PORT_OUTPUT | LADSPA_PORT_AUDIO};
#undef CTRL

#define HZ(lo, hi, def) {LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | \
                         LADSPA_HINT_LOGARITHMIC | (def), (float)0, (float)0}
static const LADSPA_PortRangeHint port_hints[PORT_COUNT] = {
    {LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_LOGARITHMIC | LADSPA_HINT_DEFAULT_LOW, 20.0f, 2000.0f},
    {LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_LOGARITHMIC | LADSPA_HINT_DEFAULT_MIDDLE, 100.0f, 600.0f},
    {LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_DEFAULT_0, -12.0f, 12.0f},
    {LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_LOGARITHMIC | LADSPA_HINT_DEFAULT_MIDDLE, 800.0f, 6000.0f},
    {LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_DEFAULT_1, 0.3f, 4.0f},
    {LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_DEFAULT_0, -12.0f, 12.0f},
    {LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_LOGARITHMIC | LADSPA_HINT_DEFAULT_MIDDLE, 3000.0f, 14000.0f},
    {LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_DEFAULT_0, -12.0f, 12.0f},
    {LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_DEFAULT_0, 0.0f, 24.0f},
    {LADSPA_HINT_BOUNDED_BELOW | LADSPA_HINT_BOUNDED_ABOVE | LADSPA_HINT_DEFAULT_MAXIMUM, 0.05f, 1.0f},
    {0, 0.0f, 0.0f},
    {0, 0.0f, 0.0f}};
#undef HZ

static const LADSPA_Descriptor descriptor = {
    .UniqueID = 0x534D5431UL, /* "SMT1" */
    .Label = "smt_hpf_limiter",
    .Properties = LADSPA_PROPERTY_HARD_RT_CAPABLE,
    .Name = "SMT Voicing (HPF + 3-band EQ + Soft Limiter)",
    .Maker = "SMT1019 hwtest",
    .Copyright = "MIT",
    .PortCount = PORT_COUNT,
    .PortDescriptors = port_descriptors,
    .PortNames = port_names,
    .PortRangeHints = port_hints,
    .ImplementationData = NULL,
    .instantiate = instantiate,
    .connect_port = connect_port,
    .activate = activate,
    .run = run,
    .run_adding = NULL,
    .set_run_adding_gain = NULL,
    .deactivate = NULL,
    .cleanup = cleanup};

const LADSPA_Descriptor *ladspa_descriptor(unsigned long index) {
  return index == 0 ? &descriptor : NULL;
}
