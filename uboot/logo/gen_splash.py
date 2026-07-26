#!/usr/bin/env python3
"""Generate the SMT1019 boot splash: rainbow light-ring + house on navy.

Output: 800x1280 8-bit palettized BMP (matches the vendor U-Boot's
expectations, same container as the previous placeholder).
"""
import colorsys
import math
import random

from PIL import Image, ImageDraw, ImageFilter

# Compose in LANDSCAPE (how the panel is mounted/viewed), then rotate 90°
# clockwise into the portrait-native 800x1280 canvas the panel scans out —
# same correction the touch calibration matrix applies.
W, H = 1280, 800
SS = 2  # supersample factor
w, h = W * SS, H * SS
cx, cy = w // 2, h // 2

random.seed(1019)

# --- background: vertical gradient navy ---
bg = Image.new("RGB", (w, h))
top = (13, 17, 27)
bot = (19, 26, 42)
px = bg.load()
for y in range(h):
    t = y / (h - 1)
    r = int(top[0] + (bot[0] - top[0]) * t)
    g = int(top[1] + (bot[1] - top[1]) * t)
    b = int(top[2] + (bot[2] - top[2]) * t)
    for x in range(w):
        px[x, y] = (r, g, b)

# --- soft radial glow behind the ring ---
glow = Image.new("L", (w, h), 0)
gd = ImageDraw.Draw(glow)
gd.ellipse([cx - 620, cy - 620, cx + 620, cy + 620], fill=46)
glow = glow.filter(ImageFilter.GaussianBlur(260))
glow_tint = Image.new("RGB", (w, h), (46, 60, 100))
bg = Image.composite(Image.blend(bg, glow_tint, 0.5), bg, glow)

img = bg.convert("RGBA")

# --- rainbow ring (the RGB light ring) ---
R = 430          # ring radius (supersampled px)
ring_w = 56      # main stroke width


def hue_color(deg, sat=0.82, val=1.0, alpha=255):
    rr, gg, bb = colorsys.hsv_to_rgb(((deg + 300) % 360) / 360.0, sat, val)
    return (int(rr * 255), int(gg * 255), int(bb * 255), alpha)


def draw_ring(radius, width, alpha, sat=0.82, val=1.0):
    layer = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    box = [cx - radius, cy - radius, cx + radius, cy + radius]
    step = 2
    for a in range(0, 360, step):
        d.arc(box, start=a - 0.6, end=a + step + 0.6,
              fill=hue_color(a, sat, val, alpha), width=width)
    return layer


# outer glow passes, then the crisp ring
img = Image.alpha_composite(img, draw_ring(R, ring_w + 120, 26, sat=0.9).filter(ImageFilter.GaussianBlur(40)))
img = Image.alpha_composite(img, draw_ring(R, ring_w + 44, 60, sat=0.9).filter(ImageFilter.GaussianBlur(14)))
img = Image.alpha_composite(img, draw_ring(R, ring_w, 255))

# --- little house in the center ---
house = Image.new("RGBA", (w, h), (0, 0, 0, 0))
hd = ImageDraw.Draw(house)
body_w, body_h = 300, 230
roof_h = 170
overhang = 46
bx0 = cx - body_w // 2
bx1 = cx + body_w // 2
by1 = cy + 190
by0 = by1 - body_h
white = (238, 242, 250, 255)
stroke = 30

# rounded body outline
hd.rounded_rectangle([bx0, by0, bx1, by1], radius=26, outline=white, width=stroke)
# roof (open gable)
hd.line([bx0 - overhang, by0 + 10, cx, by0 - roof_h], fill=white, width=stroke)
hd.line([cx, by0 - roof_h, bx1 + overhang, by0 + 10], fill=white, width=stroke)
# round the roof joints
for (jx, jy) in [(bx0 - overhang, by0 + 10), (bx1 + overhang, by0 + 10), (cx, by0 - roof_h)]:
    hd.ellipse([jx - stroke // 2, jy - stroke // 2, jx + stroke // 2, jy + stroke // 2], fill=white)

# warm glowing window
win = 92
wx0, wy0 = cx - win // 2, (by0 + by1) // 2 - win // 2 + 8
warm = (255, 205, 100, 255)
wg = Image.new("RGBA", (w, h), (0, 0, 0, 0))
wgd = ImageDraw.Draw(wg)
wgd.rounded_rectangle([wx0 - 34, wy0 - 34, wx0 + win + 34, wy0 + win + 34], radius=40, fill=(255, 200, 90, 90))
wg = wg.filter(ImageFilter.GaussianBlur(30))
house = Image.alpha_composite(house, wg)
hd = ImageDraw.Draw(house)
hd.rounded_rectangle([wx0, wy0, wx0 + win, wy0 + win], radius=16, fill=warm)

img = Image.alpha_composite(img, house)

# --- sparkles ---
sp = Image.new("RGBA", (w, h), (0, 0, 0, 0))
sd = ImageDraw.Draw(sp)
for _ in range(26):
    while True:
        x = random.randint(60, w - 60)
        y = random.randint(60, h - 60)
        if math.hypot(x - cx, y - cy) > R + 150:
            break
    s = random.choice([5, 7, 9, 12])
    a = random.randint(70, 170)
    c = (215, 225, 245, a)
    # 4-point star
    sd.line([x - s, y, x + s, y], fill=c, width=3)
    sd.line([x, y - s, x, y + s], fill=c, width=3)
img = Image.alpha_composite(img, sp.filter(ImageFilter.GaussianBlur(1)))

# --- downscale, rotate into portrait-native, quantize to 8-bit BMP ---
out = img.convert("RGB").resize((W, H), Image.LANCZOS)
portrait = out.rotate(-90, expand=True)  # 90° CW -> 800x1280 panel-native
assert portrait.size == (800, 1280), portrait.size
pal = portrait.quantize(colors=256, method=Image.MEDIANCUT, dither=Image.FLOYDSTEINBERG)
pal.save("splash.bmp", format="BMP")
out.save("splash_preview.png", format="PNG")  # preview as the viewer sees it
print("wrote splash.bmp (800x1280 portrait-native) + splash_preview.png (landscape view)")
