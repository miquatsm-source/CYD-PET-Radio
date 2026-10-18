"""Render all CYD PET Radio pets into an animated GIF for the README.

The drawing commands mirror src/fat_cat.cpp and src/genre_animal.cpp.
Requires Pillow. Usage: python scripts/render_pets.py
"""
from pathlib import Path

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


def draw_animal(f, index, bob, pose):
    x, y = 48, 137 + bob
    fur, shadow = FUR, SHADOW
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
    elif index == 4:  # Chameleon
        f.drawLine(11, 180, 84, 180, 0x7BEF)
        f.fillEllipse(x, y, 24, 13, 0x07FF)
        f.fillCircle(x + 18, y - 3, 12, 0x07FF)
        f.drawCircle(15, y + 2, 14, 0xA81F)
        f.drawCircle(15, y + 2, 8, 0xA81F)
        f.fillCircle(x + 22, y - 7, 3, WHITE)
        f.fillCircle(x + 23, y - 7, 1, BLACK)
        f.fillTriangle(18, y - 5, 11, y - 14, 25, y - 8, 0x07FF)
        f.fillTriangle(60, y - 5, 67, y - 14, 73, y - 7, 0x07FF)
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
    elif index == 8:  # Sloth
        f.drawLine(x - 14, y - 12, x - 17, y + 28, shadow)
        f.drawLine(x + 13, y - 12, x + 18, y + 25, shadow)
        f.fillRoundRect(x - 23, y - 7, 46, 49, 20, 0x84A9)
        f.fillCircle(x, y - 29, 23, 0x84A9)
        f.fillCircle(x - 8, y - 30, 7, 0x4208)
        f.fillCircle(x + 8, y - 30, 7, 0x4208)
        f.fillCircle(x - 8, y - 30, 3, WHITE)
        f.fillCircle(x + 8, y - 30, 3, WHITE)
        f.drawLine(x - 6, y - 18, x + 6, y - 18, 0x4208)
        f.drawLine(x, y - 7, x - 7, y + 17, 0x4208)
        f.drawLine(x - 7, y + 17, x + 7, y + 17, 0x4208)
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


def render(index, pose):
    genre, animal, name = PETS[index]
    f = Canvas()
    f.drawFastHLine(0, 193, WIDTH, 0x3186)
    f.fillCircle(15, 44, 1, YELLOW)
    f.fillCircle(77, 105, 1, YELLOW)
    f.text(name, 48, 18, CYAN)
    if index == 0:
        draw_cat(f, pose)
    else:
        draw_animal(f, index, BOUNCE[pose], pose)
    f.text(genre, 48, 209, CYAN, small=True)
    f.text(f"{animal} {name}", 48, 226, WHITE, small=True)
    return f.image


def main():
    frames = []
    for index in range(len(PETS)):
        for t in range(FRAMES_PER_PET):
            frames.append(render(index, t % 12))
    out = Path(__file__).resolve().parent.parent / "docs" / "pets.gif"
    out.parent.mkdir(exist_ok=True)
    frames[0].save(out, save_all=True, append_images=frames[1:],
                   duration=FRAME_MS, loop=0, optimize=True)
    print(f"{out} ({out.stat().st_size // 1024} KiB)")


if __name__ == "__main__":
    main()
