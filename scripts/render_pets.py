"""Render all CYD PET Radio pets into an animated GIF for the README.

The drawing commands mirror src/fat_cat.cpp and src/genre_animal.cpp.
Requires Pillow. Usage: python scripts/render_pets.py
"""
from pathlib import Path
import math

from PIL import Image, ImageDraw, ImageFont

SCALE = 3
WIDTH, HEIGHT = 96, 240
FRAME_MS = 100
FRAMES_PER_PET = 24


def rgb(v):
    return (((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63,
            (v & 31) * 255 // 31)


YELLOW, WHITE, BLACK, CYAN = 0xFFE0, 0xFFFF, 0x0000, 0x07FF
PANEL, FUR, SHADOW, BELLY, EAR, EYE = (
    0x0841, 0xAD55, 0x6B4D, 0xFFDF, 0xF249, 0x07E0)

BOUNCE = [0, -2, -5, -8, -10, -8, -5, -2, 0, 2, 1, 0]
ARMS = [-7, -12, -17, -21, -17, -12, -7, -12, -17, -21, -17, -12]
KICKS = [-4, -2, 2, 6, 2, -2, -4, 2, 6, 2, -2, -4]

# (genre, animal, name); the cat is the default pet shown without a genre.
PETS = [
    ("No genre (default)", "Cat", "Claudia"),
    ("Pop", "Peacock", "Poldi"),
    ("Rock", "Lion", "Leopold"),
    ("Hip-Hop", "Kangaroo", "Hoppe"),
    ("Techno", "Chameleon", "Egon"),
    ("Classical", "Swan", "Ludwig"),
    ("Jazz", "Cat", "Moritz"),
    ("Heavy Metal", "Rhino", "Siegfried"),
    ("Reggae", "Sloth", "Willi"),
    ("Country", "Horse", "Blitz"),
    ("German Pop", "Budgie", "Hansi"),
]


class Canvas:
    def __init__(self):
        self.image = Image.new("RGB", (WIDTH * SCALE, HEIGHT * SCALE),
                               rgb(PANEL))
        self.d = ImageDraw.Draw(self.image)
        self.font = ImageFont.truetype("arial.ttf", 11 * SCALE)
        self.small = ImageFont.truetype("arial.ttf", 9 * SCALE)

    @staticmethod
    def p(*a):
        return [v * SCALE for v in a]

    def fillRect(self, x, y, w, h, c):
        self.d.rectangle(self.p(x, y, x + w, y + h), fill=rgb(c))

    def drawFastHLine(self, x, y, w, c):
        self.fillRect(x, y, w, 1, c)

    def drawFastVLine(self, x, y, h, c):
        self.fillRect(x, y, 1, h, c)

    def thickLine(self, x0, y0, x1, y1, w, c):
        self.d.line(self.p(x0, y0, x1, y1), fill=rgb(c), width=int(w * SCALE))
        self.fillCircle(x0, y0, w // 2, c)
        self.fillCircle(x1, y1, w // 2, c)

    def drawLine(self, x0, y0, x1, y1, c):
        self.d.line(self.p(x0, y0, x1, y1), fill=rgb(c), width=SCALE)

    def fillCircle(self, x, y, r, c):
        self.d.ellipse(self.p(x - r, y - r, x + r + 1, y + r + 1), fill=rgb(c))

    def drawCircle(self, x, y, r, c):
        self.d.ellipse(self.p(x - r, y - r, x + r + 1, y + r + 1),
                       outline=rgb(c), width=SCALE)

    def fillEllipse(self, x, y, rx, ry, c):
        self.d.ellipse(self.p(x - rx, y - ry, x + rx + 1, y + ry + 1),
                       fill=rgb(c))

    def fillTriangle(self, x0, y0, x1, y1, x2, y2, c):
        self.d.polygon(self.p(x0, y0, x1, y1, x2, y2), fill=rgb(c))

    def fillRoundRect(self, x, y, w, h, r, c):
        self.d.rounded_rectangle(self.p(x, y, x + w, y + h),
                                 radius=r * SCALE, fill=rgb(c))

    def text(self, s, x, y, c, small=False):
        self.d.text((x * SCALE, y * SCALE), s, fill=rgb(c), anchor="mm",
                    font=self.small if small else self.font)


def draw_cat(f, pose):
    bob = BOUNCE[pose]
    step = KICKS[pose]
    arm = ARMS[pose]
    hx, hy, by = 48, 108 + bob, 132 + bob
    f.drawLine(68, by + 24, 81, by + 17 + bob // 2, SHADOW)
    f.drawLine(81, by + 17 + bob // 2, 86, by + 10 + bob // 2, SHADOW)
    f.fillCircle(86, by + 10 + bob // 2, 3, FUR)
    f.fillTriangle(hx - 14, hy - 11, hx - 17, hy - 27, hx - 5, hy - 19, SHADOW)
    f.fillTriangle(hx + 14, hy - 11, hx + 17, hy - 27, hx + 5, hy - 19, SHADOW)
    f.fillTriangle(hx - 13, hy - 15, hx - 14, hy - 23, hx - 8, hy - 18, EAR)
    f.fillTriangle(hx + 13, hy - 15, hx + 14, hy - 23, hx + 8, hy - 18, EAR)
    f.fillRoundRect(25, by, 46, 47, 20, SHADOW)
    f.fillRoundRect(28, by - 2, 40, 44, 18, FUR)
    f.fillEllipse(48, by + 23, 14, 17, BELLY)
    f.drawLine(32, by + 10, 24, by + arm, SHADOW)
    f.drawLine(64, by + 10, 72, by + ARMS[(pose + 6) % 12], SHADOW)
    f.fillCircle(24, by + arm, 4, BELLY)
    f.fillCircle(72, by + ARMS[(pose + 6) % 12], 4, BELLY)
    f.fillCircle(hx, hy, 20, FUR)
    f.fillCircle(41, hy + 1, 2, EYE)
    f.fillCircle(55, hy + 1, 2, EYE)
    f.fillCircle(41, hy + 1, 1, BLACK)
    f.fillCircle(55, hy + 1, 1, BLACK)
    f.fillTriangle(45, hy + 7, 51, hy + 7, 48, hy + 10, EAR)
    f.drawLine(48, hy + 10, 45, hy + 13, BLACK)
    f.drawLine(48, hy + 10, 51, hy + 13, BLACK)
    for a, b in ((9, 8), (12, 13)):
        f.drawLine(35, hy + a, 28, hy + b, BELLY)
        f.drawLine(61, hy + a, 68, hy + b, BELLY)
    f.drawLine(37, by + 40, 34 + step, 181 + bob, SHADOW)
    f.drawLine(59, by + 40, 62 - step, 181 - bob, SHADOW)
    f.fillRoundRect(28 + step, 178 + bob, 13, 7, 3, BELLY)
    f.fillRoundRect(55 - step, 178 - bob, 13, 7, 3, BELLY)
    f.text("*", 13, 73 + bob, YELLOW)
    f.text("*", 82, 56 - bob, YELLOW)


def c565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def claws(f, x, y, direction):
    top = y if direction > 0 else y - 7
    for off in (-3, 0, 3):
        f.fillRect(x + off, top, 1, 6, c565(240, 235, 215))


SWAY = [0, 1, 2, 3, 2, 1, 0, -1, -2, -3, -2, -1]
SWAY_BOB = [0, -1, -2, -3, -2, -1, 0, 1, 2, 3, 2, 1]
WAVE = [0, 2, 4, 6, 4, 2, 0, -2, -4, -6, -4, -2]


def sloth_face(f, cx, cy, pose):
    mask, dark = c565(60, 40, 30), c565(30, 20, 20)
    f.fillEllipse(cx - 8, cy - 2, 6, 5, mask)
    f.fillEllipse(cx + 8, cy - 2, 6, 5, mask)
    if pose >= 10:
        f.drawFastHLine(cx - 11, cy - 2, 7, c565(232, 216, 172))
        f.drawFastHLine(cx + 5, cy - 2, 7, c565(232, 216, 172))
    else:
        f.fillCircle(cx - 8, cy - 2, 2, WHITE)
        f.fillCircle(cx + 8, cy - 2, 2, WHITE)
        f.fillCircle(cx - 8, cy - 2, 1, BLACK)
        f.fillCircle(cx + 8, cy - 2, 1, BLACK)
    f.fillEllipse(cx, cy + 5, 3, 2, dark)
    f.drawLine(cx - 5, cy + 8, cx - 2, cy + 10, dark)
    f.drawLine(cx - 2, cy + 10, cx + 2, cy + 10, dark)
    f.drawLine(cx + 2, cy + 10, cx + 5, cy + 8, dark)


def draw_willi_hanging(f, swing, bob, pose):
    branch, leaf = c565(110, 70, 35), c565(60, 170, 70)
    fur, shade = c565(150, 135, 85), c565(105, 90, 55)
    cx, by, hy = 48 + swing, 130 + bob, 98 + bob
    f.fillRoundRect(2, 62, 92, 7, 3, branch)
    for lx, ly in ((10, 58), (84, 58), (48, 57)):
        f.fillEllipse(lx, ly, 8, 3, leaf)
    f.thickLine(cx - 12, by - 14, 28, 66, 5, shade)
    f.thickLine(cx + 12, by - 14, 68, 66, 5, shade)
    claws(f, 28, 62, 1)
    claws(f, 68, 62, 1)
    f.thickLine(cx - 8, by + 20, 38 - swing, 172, 5, shade)
    f.thickLine(cx + 8, by + 20, 58 - swing, 172, 5, shade)
    claws(f, 38 - swing, 170, 1)
    claws(f, 58 - swing, 170, 1)
    f.fillEllipse(cx, by, 22, 29, fur)
    f.fillEllipse(cx, by + 6, 13, 18, c565(190, 170, 120))
    f.fillEllipse(cx, hy, 20, 17, fur)
    f.fillEllipse(cx, hy + 4, 16, 13, c565(232, 216, 172))
    sloth_face(f, cx, hy + 1, pose)


def draw_willi_sitting(f, bob, pose):
    fur, shade = c565(150, 135, 85), c565(105, 90, 55)
    by, hy = 152 + bob, 102 + bob
    foot = 181 + int(bob / 3)
    hx_, hy_ = 14 + WAVE[pose], 116 + bob
    f.fillEllipse(48, by, 27, 29, fur)
    f.fillEllipse(48, by + 6, 16, 19, c565(190, 170, 120))
    f.fillEllipse(30, foot, 10, 6, shade)
    f.fillEllipse(66, foot, 10, 6, shade)
    claws(f, 24, foot, 1)
    claws(f, 60, foot, 1)
    f.thickLine(26, by - 12, hx_, hy_, 6, shade)
    f.fillCircle(hx_, hy_ - 2, 4, shade)
    claws(f, hx_, hy_ - 8, -1)
    f.thickLine(70, by - 12, 76, by + 12, 6, shade)
    f.fillCircle(76, by + 14, 4, shade)
    f.fillEllipse(48, hy, 22, 20, fur)
    f.fillEllipse(48, hy + 3, 17, 14, c565(232, 216, 172))
    f.fillEllipse(48, hy - 18, 23, 14, c565(30, 150, 60))
    f.fillEllipse(48, hy - 20, 21, 11, c565(250, 210, 40))
    f.fillEllipse(48, hy - 22, 19, 8, c565(220, 40, 40))
    sloth_face(f, 48, hy, pose)


def draw_chameleon(f, bob, pose, neon):
    shift_y = 26
    green, dark = c565(30, 215, 150), c565(20, 150, 105)
    light = c565(170, 255, 215)
    mag, yel, blu = c565(225, 60, 225), c565(255, 230, 60), c565(60, 140, 255)
    cyc = [mag, yel, blu]
    navy = c565(40, 40, 60)
    foot_y = 153 + shift_y
    bx, by = 44, 128 + shift_y + bob
    fs = 2 if pose % 6 < 3 else -2
    hx, hy = 72, 122 + shift_y + bob
    f.fillRoundRect(4, 152 + shift_y, 88, 7, 3, c565(110, 70, 35))
    f.fillEllipse(12, 150 + shift_y, 8, 3, c565(60, 170, 70))
    f.fillEllipse(80, 150 + shift_y, 8, 3, c565(60, 170, 70))
    curl = 0.3 + math.sin(pose * 0.5236) * 0.25
    t = 0.0
    while t < 13.2:
        r = 13.0 - t * 0.85
        f.fillCircle(round(22 + r * math.cos(curl + t)),
                     round(by + 6 + r * math.sin(curl + t)), 2, dark)
        t += 0.2
    f.thickLine(56, by + 12, 60 - fs, foot_y, 3, dark)
    f.thickLine(28, by + 12, 24 - fs, foot_y, 3, dark)
    f.fillEllipse(bx, by, 26, 17, green)
    if neon:
        for col in range(-26, 27):
            off = col + 26 + pose * 2
            if off % 6 >= 3:
                continue
            half = round(17 * math.sqrt(1 - (col / 26) ** 2))
            f.drawFastVLine(bx + col, by - half, 2 * half + 1,
                            cyc[(off // 6) % 3])
    else:
        sh = pose // 4
        f.fillEllipse(bx, by + 9, 21, 7, light)
        for dx_, dy_, r, k in ((32, -6, 4, 0), (44, -10, 3, 1), (54, -4, 4, 2),
                               (38, 4, 3, 1), (50, 5, 3, 0)):
            f.fillCircle(dx_, by + dy_, r, cyc[(sh + k) % 3])
    for i in range(7):
        x = bx - 22 + i * 7.3
        top = round(by - 17 * math.sqrt(1 - ((x - bx) / 26) ** 2))
        f.fillTriangle(round(x) - 3, top + 1, round(x), top - 6,
                       round(x) + 3, top + 1, dark)
    f.thickLine(62, by + 14, 66 + fs, foot_y, 4, green)
    f.thickLine(32, by + 14, 28 + fs, foot_y, 4, green)
    for foot in (66 + fs, 28 + fs):
        f.drawLine(foot, foot_y, foot + 4, foot_y - 2, green)
        f.drawLine(foot, foot_y, foot - 4, foot_y - 2, green)
    f.fillTriangle(hx - 12, hy - 6, hx - 16, hy - 22, hx + 2, hy - 10, dark)
    f.fillEllipse(hx, hy, 14, 11, green)
    f.fillEllipse(hx + 10, hy + 3, 7, 5, green)
    f.drawLine(hx + 17, hy + 6, hx + 2, hy + 8, c565(10, 70, 50))
    f.fillCircle(hx + 5, hy - 3, 7, c565(15, 110, 80))
    f.fillCircle(hx + 5, hy - 3, 5, WHITE)
    f.fillCircle(hx + (5, 6, 7, 6)[pose // 3], hy - 3, 3, BLACK)
    f.fillCircle(hx + 15, hy + 1, 1, c565(10, 70, 50))
    if neon:
        for ang in range(190, 346, 8):
            r = math.radians(ang)
            f.fillCircle(round(hx + 15 * math.cos(r)),
                         round(hy - 2 + 17 * math.sin(r)), 2, navy)
        f.fillCircle(hx - 6, hy + 2, 6, navy)
        f.fillCircle(hx - 6, hy + 2, 4 if pose % 6 < 3 else 3, mag)


def fill_quad(f, p0, p1, p2, p3, c):
    f.fillTriangle(*p0, *p1, *p2, c)
    f.fillTriangle(*p0, *p2, *p3, c)


def thick_arc(f, cx, cy, rx, ry, a0, a1, w, c):
    for a in range(a0, a1 + 1, 4):
        r = math.radians(a)
        f.fillCircle(cx + round(rx * math.cos(r)), cy + round(ry * math.sin(r)),
                     w // 2, c)


def tdiv(a, b):
    return int(a / b)


def draw_kangaroo(f, b, pose, boombox):
    fur, dark = c565(205, 150, 95), c565(160, 110, 65)
    light, pink = c565(238, 208, 165), c565(230, 150, 150)
    feet = 176 + tdiv(b, 2)
    f.thickLine(58, 152 + b, 86, feet, 7, dark)
    for fx in (36, 60):
        f.fillEllipse(fx, 156 + b, 11, 16, fur)
        f.fillRoundRect(fx - 12, feet, 24, 8, 4, dark)
    f.fillEllipse(48, 135 + b, 19, 29, fur)
    f.fillEllipse(48, 142 + b, 12, 19, light)
    for sx in (-1, 1):
        f.fillTriangle(48 + sx * 12, 92 + b, 48 + sx * 15, 64 + b,
                       48 + sx * 5, 86 + b, fur)
        f.fillTriangle(48 + sx * 11, 88 + b, 48 + sx * 14, 70 + b,
                       48 + sx * 7, 86 + b, pink)
    if not boombox:
        grey = c565(90, 90, 110)
        wave = tdiv(WAVE[pose], 2)
        f.fillEllipse(48, 99 + b, 16, 17, grey)
        f.fillEllipse(48, 99 + b, 12, 14, fur)
        f.fillEllipse(48, 138 + b, 21, 27, grey)
        f.fillRoundRect(36, 146 + b, 24, 11, 4, c565(70, 70, 90))
        f.drawLine(42, 121 + b, 40, 140 + b, c565(210, 210, 230))
        f.drawLine(54, 121 + b, 56, 140 + b, c565(210, 210, 230))
        f.thickLine(29, 124 + b, 22, 142 + b, 6, grey)
        f.fillCircle(22, 143 + b, 3, fur)
        f.thickLine(67, 124 + b, 76 + wave, 104 + b, 6, grey)
        f.fillCircle(77 + wave, 101 + b, 3, fur)
        f.thickLine(76 + wave, 99 + b, 74 + wave, 92 + b, 2, fur)
        f.thickLine(78 + wave, 99 + b, 80 + wave, 92 + b, 2, fur)
    f.fillEllipse(48, 107 + b, 7, 6, light)
    f.fillCircle(48, 105 + b, 2, BLACK)
    f.fillCircle(43, 96 + b, 2, BLACK)
    f.fillCircle(53, 96 + b, 2, BLACK)
    if boombox:
        phones = c565(40, 40, 50)
        beat = pose % 6 < 3
        cup = c565(0, 200, 255) if beat else c565(0, 110, 190)
        thick_arc(f, 48, 98 + b, 16, 17, 180, 360, 3, phones)
        f.fillCircle(32, 102 + b, 5, phones)
        f.fillCircle(64, 102 + b, 5, phones)
        f.fillCircle(32, 102 + b, 2, cup)
        f.fillCircle(64, 102 + b, 2, cup)
        f.thickLine(31, 120 + b, 22, 136 + b, 5, fur)
        f.fillRoundRect(54, 118 + b, 34, 17, 3, c565(60, 60, 70))
        thick_arc(f, 71, 118 + b, 7, 6, 180, 360, 2, c565(150, 150, 160))
        for sx in (63, 79):
            f.fillCircle(sx, 127 + b, 6 if beat else 5, c565(30, 30, 30))
            f.fillCircle(sx, 127 + b, 3 if beat else 2, c565(0, 220, 120))
        f.thickLine(65, 123 + b, 70, 130 + b, 5, fur)


def draw_rhino(f, b, pose, devil):
    strum = [0, 4, 8, 4]
    grey, dark = c565(150, 150, 160), c565(110, 110, 120)
    light, horn = c565(190, 190, 200), c565(240, 235, 215)
    leather, stud = c565(25, 25, 30), c565(220, 220, 230)
    red = c565(200, 40, 40)
    feet = 176 + tdiv(b, 2)
    f.thickLine(62, 160 + b, 88, feet, 5, dark)
    for fx in (36, 60):
        f.fillEllipse(fx, 160 + b, 11, 17, dark)
        f.fillRoundRect(fx - 12, feet, 24, 8, 4, dark)
    f.fillEllipse(48, 135 + b, 24, 29, grey)
    f.fillEllipse(48, 142 + b, 14, 18, light)
    f.fillRoundRect(26, 118 + b, 44, 14, 6, leather)
    for sx in range(30, 68, 8):
        f.fillCircle(sx, 125 + b, 1, stud)
    for sx in (-1, 1):
        f.fillTriangle(48 + sx * 14, 90 + b, 48 + sx * 20, 74 + b,
                       48 + sx * 6, 84 + b, grey)
    f.fillEllipse(48, 100 + b, 16, 15, light)
    f.fillTriangle(44, 92 + b, 48, 66 + b, 52, 92 + b, horn)
    f.fillTriangle(46, 88 + b, 48, 78 + b, 50, 88 + b, c565(215, 205, 180))
    f.fillCircle(40, 98 + b, 2, BLACK)
    f.fillCircle(56, 98 + b, 2, BLACK)
    f.drawLine(36, 94 + b, 44, 96 + b, BLACK)
    f.drawLine(60, 94 + b, 52, 96 + b, BLACK)
    f.fillCircle(43, 109 + b, 1, dark)
    f.fillCircle(53, 109 + b, 1, dark)
    f.drawFastHLine(42, 114 + b, 12, c565(60, 20, 20))
    if not devil:
        hand = strum[pose % 4]
        fill_quad(f, (14, 132 + b), (30, 150 + b), (26, 156 + b),
                  (10, 138 + b), red)
        fill_quad(f, (50, 150 + b), (86, 114 + b), (92, 120 + b),
                  (58, 160 + b), red)
        fill_quad(f, (30, 154 + b), (64, 156 + b), (80, 128 + b),
                  (86, 134 + b), c565(60, 60, 60))
        f.thickLine(80, 116 + b, 72, 100 + b, 4, c565(70, 50, 30))
        fill_quad(f, (26, 150 + b), (50, 142 + b), (68, 158 + b),
                  (44, 164 + b), red)
        f.thickLine(32, 124 + b, 40, 148 + b, 6, grey)
        f.thickLine(64, 124 + b, 78, 118 + b + hand, 6, grey)
        f.fillCircle(32, 148 + b, 2, stud)
        f.fillCircle(40, 146 + b, 2, stud)
    else:
        lift = tdiv(WAVE[pose], 2)
        hair = c565(20, 20, 25)
        f.fillTriangle(22, 90 + b, 16, 130 + b, 28, 130 + b, hair)
        f.fillTriangle(74, 90 + b, 80, 130 + b, 68, 130 + b, hair)
        f.fillEllipse(48, 86 + b, 16, 6, hair)
        f.fillTriangle(36, 90 + b, 40, 94 + b, 36, 96 + b, red)
        f.drawFastHLine(44, 76 + b, 8, red)
        f.thickLine(31, 126 + b, 20, 108 + b + lift, 6, grey)
        f.fillCircle(20, 106 + b + lift, 4, grey)
        f.fillTriangle(17, 103 + b + lift, 15, 94 + b + lift, 20,
                       101 + b + lift, grey)
        f.fillTriangle(22, 101 + b + lift, 25, 93 + b + lift, 23,
                       104 + b + lift, grey)
        f.thickLine(65, 126 + b, 70, 144 + b, 6, grey)


def draw_moritz(f, b, pose):
    fur, belly = c565(45, 45, 55), c565(230, 230, 235)
    pink, gold = c565(235, 150, 160), c565(240, 190, 40)
    whisker = c565(225, 225, 225)
    hy = 84 + b
    tap = 0 if pose % 4 < 2 else 3
    f.thickLine(72, 168, 88, 150 + b, 5, fur)
    f.thickLine(88, 150 + b, 82, 138 + b - tdiv(WAVE[pose], 2), 5, fur)
    f.fillEllipse(48, 150 + b, 24, 30, fur)
    f.fillEllipse(48, 156 + b, 14, 21, belly)
    f.fillEllipse(36, 177, 8, 5, fur)
    f.fillEllipse(60, 177, 8, 5, fur)
    for sx in (-1, 1):
        f.fillTriangle(48 + sx * 24, hy + 6, 48 + sx * 24, hy - 24,
                       48 + sx * 8, hy - 14, fur)
        f.fillTriangle(48 + sx * 20, hy + 1, 48 + sx * 21, hy - 17,
                       48 + sx * 11, hy - 10, pink)
    f.fillEllipse(48, hy, 25, 22, fur)
    f.fillEllipse(48, hy + 8, 11, 8, belly)
    f.fillTriangle(45, hy + 3, 51, hy + 3, 48, hy + 7, c565(240, 110, 130))
    for sx in (-1, 1):
        f.drawLine(48 + sx * 12, hy + 6, 48 + sx * 30, hy + 1, whisker)
        f.drawLine(48 + sx * 12, hy + 8, 48 + sx * 30, hy + 11, whisker)
    eye = c565(90, 220, 90)
    for sx in (-10, 10):
        if pose >= 10:
            f.drawFastHLine(48 + sx - 4, hy - 2, 9, eye)
        else:
            f.fillEllipse(48 + sx, hy - 2, 4, 5, eye)
            f.fillEllipse(48 + sx, hy - 2, 1, 4, BLACK)
    f.fillEllipse(40, hy - 16, 20, 8, c565(150, 30, 50))
    f.fillCircle(38, hy - 24, 2, c565(150, 30, 50))
    f.thickLine(58, hy + 28, 58, hy + 66, 4, gold)
    thick_arc(f, 66, hy + 66, 8, 8, 90, 270, 4, gold)
    f.thickLine(66, hy + 74, 76, hy + 74, 4, gold)
    f.thickLine(76, hy + 74, 80, hy + 56, 4, gold)
    fill_quad(f, (74, hy + 54), (86, hy + 54), (82, hy + 62), (80, hy + 58),
              gold)
    f.thickLine(56, hy + 22, 54, hy + 14, 2, gold)
    f.thickLine(36, 132 + b, 50, 124 + b + tap, 5, fur)


def draw_blitz(f, b, pose):
    o = -5
    coat, dark = c565(150, 95, 55), c565(95, 55, 30)
    light, red = c565(190, 135, 85), c565(210, 50, 50)
    hat = c565(120, 75, 40)
    lift = 6 if pose % 6 < 3 else 0
    f.thickLine(18 + o + tdiv(WAVE[pose], 2), 124 + b, 8 + o, 156, 4, dark)
    f.thickLine(20 + o, 128 + b, 12 + o, 160, 3, dark)
    for lx in (24, 42):
        f.thickLine(lx + o, 140 + b, lx - 1 + o, 176, 6, dark)
        f.fillRoundRect(lx - 5 + o, 175, 9, 6, 2, BLACK)
    f.fillEllipse(40 + o, 130 + b, 28, 18, coat)
    f.thickLine(30 + o, 140 + b, 31 + o, 176, 6, coat)
    f.fillRoundRect(26 + o, 175, 9, 6, 2, BLACK)
    f.thickLine(62 + o, 140 + b, 63 + o + lift // 2, 176 - lift, 6, coat)
    f.fillRoundRect(58 + o + lift // 2, 175 - lift, 9, 6, 2, BLACK)
    fill_quad(f, (58 + o, 120 + b), (66 + o, 80 + b), (84 + o, 84 + b),
              (74 + o, 130 + b), coat)
    f.fillEllipse(84 + o, 90 + b, 9, 17, coat)
    f.fillEllipse(91 + o, 102 + b, 5, 5, light)
    f.fillCircle(92 + o, 102 + b, 1, BLACK)
    f.fillCircle(83 + o, 85 + b, 2, BLACK)
    for i in range(5):
        f.thickLine(62 + i * 2 + o, 80 + i * 8 + b, 56 + i * 2 + o,
                    86 + i * 8 + b, 3, dark)
    f.fillTriangle(72 + o, 78 + b, 74 + o, 68 + b, 80 + o, 77 + b, coat)
    fill_quad(f, (60 + o, 100 + b), (76 + o, 100 + b), (72 + o, 108 + b),
              (64 + o, 108 + b), red)
    f.fillTriangle(64 + o, 106 + b, 72 + o, 106 + b, 68 + o, 114 + b, red)
    f.fillCircle(46 + o, 128 + b, 3, c565(240, 190, 40))
    f.fillEllipse(80 + o, 78 + b, 17, 4, hat)
    f.fillRoundRect(72 + o, 64 + b, 16, 14, 4, hat)
    f.fillRoundRect(72 + o, 73 + b, 16, 3, 1, red)


def draw_lion(f, b, pose, king):
    coat, light = c565(235, 185, 80), c565(250, 225, 160)
    mane, mane_d = c565(215, 130, 30), c565(170, 95, 20)
    nose, gold = c565(60, 30, 30), c565(240, 190, 40)
    tail = tdiv(WAVE[pose], 3)
    f.thickLine(66, 168, 86, 176, 4, coat)
    f.thickLine(86, 176, 90, 160 + tail, 4, coat)
    f.fillCircle(91, 157 + tail, 5, mane)
    f.fillEllipse(48, 148 + b, 22, 30, coat)
    f.fillEllipse(48, 154 + b, 13, 21, light)
    f.fillEllipse(36, 176, 11, 6, coat)
    f.fillEllipse(60, 176, 11, 6, coat)
    f.fillCircle(48, 100 + b, 30, mane_d)
    f.fillCircle(48, 100 + b, 26, mane)
    for sx in (-1, 1):
        f.fillCircle(48 + sx * 17, 76 + b, 6, coat)
        f.fillCircle(48 + sx * 17, 76 + b, 3, c565(240, 150, 150))
    f.fillCircle(48, 102 + b, 19, coat)
    f.fillEllipse(48, 110 + b, 10, 7, light)
    f.fillTriangle(45, 104 + b, 51, 104 + b, 48, 108 + b, nose)
    f.drawLine(48, 108 + b, 48, 112 + b, nose)
    for sx in (-7, 7):
        if pose >= 10:
            f.drawFastHLine(48 + sx - 3, 99 + b, 7, BLACK)
        else:
            f.fillCircle(48 + sx, 99 + b, 2, BLACK)
    if not king:
        wave = tdiv(WAVE[pose], 2)
        vest, stand = c565(25, 25, 30), c565(150, 150, 160)
        f.fillEllipse(48, 114 + b, 4, 3 if pose % 4 < 2 else 1,
                      c565(90, 40, 30))
        f.fillRoundRect(28, 124 + b, 40, 24, 8, vest)
        for sx in (34, 42, 50, 58):
            f.fillCircle(sx, 130 + b, 1, c565(220, 220, 230))
        f.thickLine(22, 176, 22, 118, 2, stand)
        f.thickLine(22, 118, 34, 118, 2, stand)
        f.fillEllipse(36, 117, 5, 4, c565(60, 60, 70))
        f.thickLine(33, 128 + b, 38, 118, 5, coat)
        f.thickLine(62, 128 + b, 78 + wave, 112 + b, 5, coat)
        f.fillCircle(80 + wave, 109 + b, 3, coat)
        f.thickLine(80 + wave, 106 + b, 78 + wave, 98 + b, 2, coat)
        f.thickLine(82 + wave, 106 + b, 86 + wave, 99 + b, 2, coat)
    else:
        f.fillTriangle(33, 76 + b, 36, 60 + b, 42, 70 + b, gold)
        f.fillTriangle(42, 70 + b, 48, 56 + b, 54, 70 + b, gold)
        f.fillTriangle(54, 70 + b, 60, 60 + b, 63, 76 + b, gold)
        f.fillRoundRect(33, 74 + b, 30, 4, 1, gold)
        f.fillCircle(48, 71 + b, 2, 0xF800 if pose % 6 < 3 else WHITE)
        cape = c565(150, 30, 50)
        f.fillTriangle(26, 122 + b, 70, 122 + b, 82, 176, cape)
        f.fillTriangle(26, 122 + b, 82, 176, 14, 176, cape)
        fill_quad(f, (40, 122 + b), (56, 122 + b), (60, 150 + b),
                  (36, 150 + b), c565(245, 240, 235))
        for sx in (40, 48, 56):
            f.fillCircle(sx, 130 + b, 1, c565(30, 30, 30))
        f.thickLine(62, 130 + b, 82, 150 + b, 5, coat)
        f.thickLine(86, 118 + b, 86, 150 + b, 3, gold)
        f.fillCircle(86, 114 + b, 5, gold)
        f.fillCircle(86, 114 + b, 2, 0xF800)


def draw_swan(f, b, pose, violin):
    white, shade = c565(245, 245, 250), c565(200, 205, 225)
    beak, suit = c565(250, 150, 30), c565(30, 30, 40)
    ripple = tdiv(WAVE[pose], 3)
    hx, hy = 70, 76 + tdiv(b, 2)
    for i in range(3):
        f.thickLine(8 + i * 6 + ripple, 172 + i * 6, 86 - i * 8 + ripple,
                    172 + i * 6, 2, c565(70, 110, 190))
    f.fillTriangle(26, 152, 10, 132, 36, 142, white)
    f.fillEllipse(46, 152, 28, 16, white)
    f.fillEllipse(40, 154, 18, 9, shade)
    for i in range(41):
        t = i / 40
        u = 1 - t
        x = u**3 * 60 + 3 * u * u * t * 88 + 3 * u * t * t * 40 + t**3 * hx
        y = u**3 * 140 + 3 * u * u * t * 128 + 3 * u * t * t * 100 + t**3 * hy
        f.fillCircle(round(x), round(y), round(4 - 1.5 * t), white)
    f.fillCircle(hx, hy, 8, white)
    f.fillTriangle(hx + 6, hy - 3, hx + 22, hy + 2, hx + 6, hy + 7, beak)
    f.drawLine(hx + 6, hy - 3, hx + 8, hy, BLACK)
    f.fillCircle(hx + 1, hy - 2, 2, BLACK)
    if not violin:
        f.fillEllipse(hx, hy - 9, 12, 3, suit)
        f.fillRoundRect(hx - 7, hy - 21, 14, 12, 3, suit)
        f.fillRoundRect(hx - 7, hy - 14, 14, 3, 1, 0xF800)
        fill_quad(f, (46, 138), (52, 134), (58, 138), (52, 142),
                  c565(200, 40, 50))
        f.thickLine(36, 146, 60, 150, 6, suit)
        f.thickLine(50, 144, 76, 124, 5, white)
        f.thickLine(76, 124, 88 + WAVE[pose], 100, 2, c565(240, 230, 200))
        f.fillCircle(76, 124, 3, white)
    else:
        wig, bow = c565(232, 232, 238), WAVE[pose]
        f.fillEllipse(hx, hy - 9, 13, 6, wig)
        for sx in (-10, 10):
            f.fillCircle(hx + sx, hy, 4, wig)
            f.fillCircle(hx + sx, hy + 7, 4, wig)
        f.fillRoundRect(14, 132, 34, 22, 8, c565(180, 100, 40))
        f.fillCircle(31, 143, 4, c565(60, 30, 10))
        f.thickLine(48, 138, 74, 118, 3, c565(110, 60, 25))
        f.thickLine(8 + bow, 120 + tdiv(bow, 2), 64 + bow,
                    152 + tdiv(bow, 2), 2, c565(190, 180, 160))
        f.thickLine(56, 146, 30, 138, 5, white)


def draw_peacock(f, b, pose, rock):
    blue, dark = c565(20, 160, 200), c565(10, 110, 160)
    green, gold = c565(40, 200, 110), c565(240, 190, 40)
    count = 7 if rock else 9
    opening = (pose if pose <= 6 else 12 - pose) / 6
    spread = (30 if rock else 40) + opening * (45 if rock else 65)
    length = (56 if rock else 50) + (0 if pose % 6 < 3 else 3)
    for i in range(count):
        a = math.radians(-90 - spread / 2 + spread * i / (count - 1))
        fx = 48 + round(length * math.cos(a))
        fy = 150 + b + round(length * math.sin(a))
        f.thickLine(48, 150 + b, fx, fy, 2, dark)
        f.fillEllipse(fx, fy, 6, 8, green)
        f.fillEllipse(fx, fy, 4, 6, blue)
        f.fillCircle(fx, fy, 2, WHITE if (i + pose // 3) % 3 == 0 else gold)
    f.thickLine(42, 170, 40, 184, 2, gold)
    f.thickLine(54, 170, 56, 184, 2, gold)
    f.fillEllipse(48, 150 + b, 14, 24, blue)
    f.fillEllipse(48, 154 + b, 8, 17, c565(170, 235, 245))
    f.thickLine(50, 128 + b, 52, 112 + b, 6, blue)
    f.fillCircle(52, 106 + b, 9, blue)
    f.fillTriangle(58, 104 + b, 68, 109 + b, 58, 112 + b, gold)
    f.fillCircle(55, 103 + b, 2, BLACK)
    for dx in (-3, 0, 3):
        f.thickLine(52 + dx, 98 + b, 52 + dx * 2, 88 + b, 1, dark)
        f.fillCircle(52 + dx * 2, 87 + b, 2, green)
    if rock:
        wave = tdiv(WAVE[pose], 3)
        pink = c565(230, 40, 130)
        f.fillRoundRect(44, 92 + b, 18, 7, 2, BLACK)
        f.drawLine(62, 95 + b, 66, 96 + b, BLACK)
        f.fillTriangle(44, 128 + b, 56, 128 + b, 50, 136 + b, pink)
        f.fillTriangle(50, 128 + b, 44, 124 + b, 44, 132 + b, pink)
        f.thickLine(36, 140 + b, 22, 148 + b, 4, blue)
        f.thickLine(22, 148 + b, 18 + wave, 120 + b, 2, c565(90, 90, 100))
        f.fillCircle(18 + wave, 116 + b, 5, c565(110, 110, 120))


def draw_budgie(f, b, pose, glitter):
    blue, dark = c565(30, 180, 230), c565(10, 110, 160)
    yellow, orange = c565(250, 230, 90), c565(240, 150, 40)
    gold = c565(240, 190, 40)
    wave = tdiv(WAVE[pose], 3)
    f.thickLine(10, 177, 86, 177, 3, c565(120, 80, 40))
    f.thickLine(44, 170, 44, 176, 2, orange)
    f.thickLine(56, 170, 56, 176, 2, orange)
    f.fillTriangle(36, 150 + b, 24, 188, 50, 168 + b, dark)
    f.fillEllipse(50, 140 + b, 20, 30, blue)
    f.fillEllipse(52, 150 + b, 12, 20, c565(120, 215, 240))
    f.fillEllipse(40, 138 + b, 8, 20, c565(20, 130, 190))
    f.fillEllipse(52, 92 + b, 17, 17, yellow)
    f.fillEllipse(54, 102 + b, 13, 8, yellow)
    f.fillTriangle(62, 92 + b, 76, 96 + b, 62, 102 + b, orange)
    f.fillCircle(56, 88 + b, 4, WHITE)
    if pose >= 10:
        f.drawFastHLine(52, 88 + b, 9, yellow)
    else:
        f.fillCircle(57, 88 + b, 2, BLACK)
    for dx in (0, 6, 12):
        f.fillCircle(40 + dx, 104 + b, 1, c565(20, 20, 60))
    f.fillTriangle(38, 82 + b, 42, 70 + b, 48, 80 + b, blue)
    if glitter:
        f.fillRoundRect(34, 118 + b, 32, 40, 8, c565(150, 30, 160))
        f.fillCircle(40, 126 + b, 1, gold)
        f.fillCircle(54, 134 + b, 1, c565(255, 120, 220))
        f.fillCircle(44, 146 + b, 1, c565(120, 240, 255))
        f.fillCircle(58, 152 + b, 1, gold)
        f.fillTriangle(46, 118 + b, 50, 128 + b, 54, 118 + b, WHITE)
        f.thickLine(62, 124 + b, 78, 110 + b + wave, 5, blue)
        f.thickLine(80, 112 + b + wave, 83, 98 + b + wave, 2,
                    c565(130, 130, 140))
        f.fillCircle(84, 95 + b + wave, 4, c565(110, 110, 120))
        f.thickLine(38, 126 + b, 26, 136 + b, 5, blue)
        if pose % 4 < 2:
            f.drawLine(88, 88 + b, 94, 84 + b, c565(255, 255, 120))
            f.drawLine(89, 92 + b, 95, 92 + b, c565(255, 255, 120))
    else:
        hat, green = c565(110, 80, 50), c565(60, 120, 60)
        f.fillEllipse(52, 76 + b, 14, 5, hat)
        f.fillRoundRect(42, 62 + b, 20, 15, 4, hat)
        f.fillRoundRect(42, 70 + b, 20, 3, 1, c565(210, 40, 40))
        f.drawLine(60, 64 + b, 66, 52 + b, c565(60, 160, 60))
        f.drawLine(62, 64 + b, 70, 56 + b, c565(220, 60, 60))
        f.fillRoundRect(32, 124 + b, 36, 32, 6, green)
        f.fillRoundRect(30, 120 + b, 40, 7, 3, c565(130, 80, 40))
        f.drawLine(40, 124 + b, 40, 138 + b, c565(200, 200, 200))
        f.drawLine(60, 124 + b, 60, 138 + b, c565(200, 200, 200))
        f.thickLine(38, 130 + b, 20, 138 + b, 5, blue)
        f.thickLine(62, 130 + b, 82, 118 + b + wave, 5, blue)
        f.fillCircle(84, 114 + b + wave, 3, blue)
        f.fillEllipse(20, 134 + b, 5, 8, c565(230, 200, 120))


def draw_animal(f, index, bob, pose, dance="bounce"):
    x, y = 48, 137 + bob
    fur, shadow = FUR, SHADOW
    if index == 1:
        draw_peacock(f, bob, pose, dance == "sway")
        return
    if index == 10:
        draw_budgie(f, bob, pose, dance == "sway")
        return
    if index == 2:
        draw_lion(f, bob, pose, dance == "sway")
        return
    if index == 5:
        draw_swan(f, bob, pose, dance == "sway")
        return
    if index == 3:
        draw_kangaroo(f, bob, pose, dance == "sway")
        return
    if index == 6:
        draw_moritz(f, bob, pose)
        return
    if index == 7:
        draw_rhino(f, bob, pose, dance == "sway")
        return
    if index == 9:
        draw_blitz(f, bob, pose)
        return
    if index == 4:
        draw_chameleon(f, bob, pose, dance == "sway")
        return
    if index == 8:
        if dance == "sway":
            draw_willi_hanging(f, SWAY[pose] * 2, bob, pose)
        else:
            draw_willi_sitting(f, bob, pose)
        return
    if index == 1:  # Peacock
        for feather in range(7):
            fx = 17 + feather * 10
            fy = 105 - abs(3 - feather) * 8 + bob
            f.drawLine(x, y + 4, fx, fy, 0x05BF)
            f.fillCircle(fx, fy, 4, 0x07E0)
            f.fillCircle(fx, fy, 2, YELLOW)
        f.fillEllipse(x, y, 13, 23, 0x05BF)
        f.fillCircle(x, y - 25, 10, 0x05BF)
        f.fillCircle(x, y - 27, 2, WHITE)
        f.fillTriangle(x + 9, y - 26, x + 18, y - 22, x + 9, y - 20, YELLOW)
        f.drawLine(x - 5, y + 21, x - 7, 183 + bob, YELLOW)
        f.drawLine(x + 5, y + 21, x + 7, 183 - bob, YELLOW)
    elif index == 2:  # Lion
        f.fillCircle(x, y - 20, 29, 0xFD20)
        f.fillCircle(x, y - 20, 20, 0xFBE0)
        f.fillCircle(x - 12, y - 38, 8, fur)
        f.fillCircle(x + 12, y - 38, 8, fur)
        f.fillCircle(x - 7, y - 23, 2, BLACK)
        f.fillCircle(x + 7, y - 23, 2, BLACK)
        f.fillEllipse(x, y - 12, 9, 6, WHITE)
        f.fillCircle(x, y - 14, 2, shadow)
        f.fillRoundRect(x - 22, y + 2, 44, 39, 17, fur)
        f.fillRoundRect(x - 14, y + 9, 28, 29, 12, 0xFFDF)
    elif index == 3:  # Kangaroo
        f.drawLine(x + 10, y + 12, 82, y + 4, shadow)
        f.drawLine(82, y + 4, 87, y + 14, shadow)
        f.fillEllipse(x, y, 19, 29, 0xC2A5)
        f.fillEllipse(x, y + 10, 11, 12, 0xFBE0)
        f.fillRoundRect(x - 20, y - 53, 12, 36, 5, fur)
        f.fillRoundRect(x + 8, y - 53, 12, 36, 5, fur)
        f.fillCircle(x - 8, y - 34, 12, fur)
        f.fillCircle(x + 8, y - 34, 12, fur)
        f.fillCircle(x - 4, y - 36, 2, BLACK)
        f.fillCircle(x + 4, y - 36, 2, BLACK)
        f.fillRoundRect(x - 15 + bob, 179, 19, 8, 4, 0xFFDF)
        f.fillRoundRect(x + 1 - bob, 179, 19, 8, 4, 0xFFDF)
    elif index == 5:  # Swan
        f.fillEllipse(x, y + 13, 28, 13, WHITE)
        f.fillEllipse(x + 15, y - 23, 7, 22, WHITE)
        f.fillCircle(x + 17, y - 42, 8, WHITE)
        f.fillTriangle(x + 23, y - 43, x + 34, y - 39, x + 24, y - 36, YELLOW)
        f.drawLine(x - 12, y + 26, x - 15, 183 + bob, YELLOW)
        f.drawLine(x + 5, y + 25, x + 3, 183 - bob, YELLOW)
        f.drawLine(x + 2, y - 7, x + 20, y - 1, 0xBDF7)
    elif index == 6:  # Jazz cat
        hy = 78 + bob
        f.fillCircle(x, y + 7, 32, shadow)
        f.fillRoundRect(15, y, 66, 56, 25, shadow)
        f.fillRoundRect(18, y - 2, 60, 54, 23, fur)
        f.fillEllipse(x, y + 25, 18, 19, 0xFFDF)
        f.fillTriangle(28, hy + 3, 25, hy - 23, 41, hy - 11, fur)
        f.fillTriangle(68, hy + 3, 71, hy - 23, 55, hy - 11, fur)
        f.fillCircle(x, hy, 26, fur)
        f.fillCircle(38, hy + 1, 3, 0x07E0)
        f.fillCircle(58, hy + 1, 3, 0x07E0)
        f.fillTriangle(45, hy + 9, 51, hy + 9, 48, hy + 13, 0xF249)
        f.drawLine(34, y + 9, 17, y - 7, shadow)
        f.drawLine(62, y + 9, 79, y - 7, shadow)
        f.drawLine(35, y + 53, 30 + bob, 183, shadow)
        f.drawLine(61, y + 53, 66 - bob, 183, shadow)
    elif index == 7:  # Rhino
        f.fillEllipse(x - 5, y, 28, 22, 0xA534)
        f.fillEllipse(x + 12, y - 6, 17, 16, 0xBDF7)
        f.fillTriangle(x + 12, y - 15, x + 23, y - 29, x + 21, y - 10, WHITE)
        f.fillTriangle(x + 16, y - 11, x + 34, y - 17, x + 22, y - 5, 0xFFDF)
        f.fillTriangle(x - 16, y - 12, x - 23, y - 25, x - 7, y - 17, 0xA534)
        f.fillCircle(x + 16, y - 8, 2, BLACK)
        f.fillRoundRect(x - 24, y + 12, 10, 27, 4, 0xA534)
        f.fillRoundRect(x - 5, y + 13, 10, 26, 4, 0xA534)
        f.fillRoundRect(x + 12, y + 7, 10, 29, 4, 0xA534)
    elif index == 9:  # Horse
        f.fillEllipse(x - 3, y + 3, 25, 17, 0x9A65)
        f.fillTriangle(x - 6, y - 1, x + 4, y - 36, x + 23, y - 17, 0x9A65)
        f.fillEllipse(x + 17, y - 25, 11, 18, 0x9A65)
        f.fillTriangle(x + 12, y - 37, x + 10, y - 51, x + 18, y - 38, shadow)
        f.fillTriangle(x + 20, y - 37, x + 27, y - 49, x + 26, y - 34, shadow)
        f.fillCircle(x + 20, y - 26, 2, BLACK)
        f.drawLine(x - 17, y + 13, x - 19, 183 + bob, shadow)
        f.drawLine(x - 3, y + 15, x - 2, 183 - bob, shadow)
        f.drawLine(x + 11, y + 10, x + 13, 183 + bob, shadow)
        f.drawLine(x + 24, y + 5, x + 25, 183 - bob, shadow)
    elif index == 10:  # Budgie
        f.fillEllipse(x, y, 21, 27, 0x07FF)
        f.fillEllipse(x - 3, y + 3, 11, 20, 0x05BF)
        f.fillCircle(x + 10, y - 21, 17, 0x07FF)
        f.fillCircle(x + 5, y - 24, 2, BLACK)
        f.fillTriangle(x + 23, y - 22, x + 35, y - 17, x + 23, y - 13, 0xFD20)
        f.fillTriangle(x + 3, y - 37, x - 1, y - 49, x + 10, y - 37, 0x05BF)
        f.fillTriangle(x + 14, y - 36, x + 24, y - 45, x + 21, y - 32, 0x05BF)
        f.drawLine(x - 6, y + 25, x - 8, 183 + bob, YELLOW)
        f.drawLine(x + 7, y + 24, x + 9, 183 - bob, YELLOW)
    if pose % 6 == 0:
        f.fillCircle(14, 90, 2, YELLOW)
        f.fillCircle(82, 108, 2, CYAN)


def render(index, pose, dance="bounce"):
    genre, animal, name = PETS[index]
    f = Canvas()
    f.drawFastHLine(0, 193, WIDTH, 0x3186)
    f.fillCircle(15, 44, 1, YELLOW)
    f.fillCircle(77, 105, 1, YELLOW)
    f.text(name, 48, 18, CYAN)
    if index == 0:
        draw_cat(f, pose)
    else:
        bob = SWAY_BOB[pose] if dance == "sway" else BOUNCE[pose]
        draw_animal(f, index, bob, pose, dance)
    f.text(genre, 48, 209, CYAN, small=True)
    f.text(f"{animal} {name}", 48, 226, WHITE, small=True)
    return f.image


# Pets with two looks are shown twice; the radio picks the look by dance.
TWO_LOOKS = {1: ("bounce", "sway"), 10: ("bounce", "sway"), 2: ("bounce", "sway"), 3: ("bounce", "sway"),
             5: ("bounce", "sway"), 4: ("bounce", "sway"),
             7: ("bounce", "sway"), 8: ("sway", "bounce")}
SHOWS = []
for _i in range(len(PETS)):
    for _d in TWO_LOOKS.get(_i, ("bounce",)):
        SHOWS.append((_i, _d))


def main():
    frames = []
    for index, dance in SHOWS:
        for t in range(FRAMES_PER_PET):
            frames.append(render(index, t % 12, dance))
    out = Path(__file__).resolve().parent.parent / "docs" / "pets.gif"
    out.parent.mkdir(exist_ok=True)
    frames[0].save(out, save_all=True, append_images=frames[1:],
                   duration=FRAME_MS, loop=0, optimize=True)
    print(f"{out} ({out.stat().st_size // 1024} KiB)")


if __name__ == "__main__":
    main()
