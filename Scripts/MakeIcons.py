# MakeIcons.py - Draws the item icons of the character and merchant screens
# (Assets/UI/icon_*.png): small silhouettes of weapons, armour, potions, ammunition, and keys, in a
# few flat colors with a dark outline. Made for LegendsBelow and released as CC0. Plain Python
# (no packages); deterministic.
#
#   python Scripts/MakeIcons.py [output directory, default Assets/UI]

import math
import struct
import sys
import zlib
from pathlib import Path

SIZE = 32
SUPER = 4            # samples per pixel along each axis (anti-aliasing)
STEEL = (196, 204, 214)
DARK_STEEL = (120, 128, 140)
WOOD = (150, 98, 54)
GOLD = (232, 186, 70)
LEATHER = (140, 86, 50)
RED = (206, 52, 48)
GLASS = (210, 230, 240)
OUTLINE = (24, 20, 18)


class Canvas:
    def __init__(self):
        n = SIZE * SUPER
        self.n = n
        self.color = [[None] * n for _ in range(n)]

    def fill(self, inside, color):
        """Paint every sample whose center (in icon pixels) is inside the shape"""
        n = self.n
        for y in range(n):
            py = (y + 0.5) / SUPER
            row = self.color[y]
            for x in range(n):
                if inside((x + 0.5) / SUPER, py):
                    row[x] = color

    def polygon(self, points, color):
        def inside(x, y):
            hit = False
            for i in range(len(points)):
                x0, y0 = points[i]
                x1, y1 = points[(i + 1) % len(points)]
                if (y0 > y) != (y1 > y) and x < x0 + (y - y0) * (x1 - x0) / (y1 - y0):
                    hit = not hit
            return hit
        self.fill(inside, color)

    def line(self, x0, y0, x1, y1, width, color):
        dx, dy = x1 - x0, y1 - y0
        length2 = dx * dx + dy * dy or 1e-9
        def inside(x, y):
            t = max(0.0, min(1.0, ((x - x0) * dx + (y - y0) * dy) / length2))
            cx, cy = x0 + t * dx, y0 + t * dy
            return (x - cx) ** 2 + (y - cy) ** 2 <= (width / 2) ** 2
        self.fill(inside, color)

    def circle(self, cx, cy, r, color):
        self.fill(lambda x, y: (x - cx) ** 2 + (y - cy) ** 2 <= r * r, color)

    def ellipse(self, cx, cy, rx, ry, color):
        self.fill(lambda x, y: ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1.0, color)

    def rect(self, x0, y0, x1, y1, color):
        self.fill(lambda x, y: x0 <= x <= x1 and y0 <= y <= y1, color)

    def arc(self, cx, cy, r, a0, a1, width, color):
        def inside(x, y):
            d = math.hypot(x - cx, y - cy)
            a = math.atan2(y - cy, x - cx)
            return abs(d - r) <= width / 2 and a0 <= a <= a1
        self.fill(inside, color)

    def pixels(self):
        """Down to icon pixels (averaging the samples), with a dark outline around the shape"""
        n, s = self.n, SUPER
        cover = [[0.0] * SIZE for _ in range(SIZE)]
        rgb = [[(0, 0, 0)] * SIZE for _ in range(SIZE)]
        for y in range(SIZE):
            for x in range(SIZE):
                r = g = b = count = 0
                for sy in range(s):
                    for sx in range(s):
                        c = self.color[y * s + sy][x * s + sx]
                        if c:
                            r += c[0]; g += c[1]; b += c[2]; count += 1
                cover[y][x] = count / (s * s)
                if count:
                    rgb[y][x] = (r // count, g // count, b // count)
        out = []
        for y in range(SIZE):
            row = []
            for x in range(SIZE):
                a = cover[y][x]
                if a > 0:
                    row.append((*rgb[y][x], int(255 * min(1.0, a * 1.15))))
                    continue
                near = max((cover[yy][xx] for yy in range(max(0, y - 1), min(SIZE, y + 2)) for xx in range(max(0, x - 1), min(SIZE, x + 2))), default=0)
                row.append((*OUTLINE, int(230 * min(1.0, near * 2.0))) if near > 0 else (0, 0, 0, 0))
            out.append(row)
        return out


def write_png(path, pixels):
    raw = b''.join(b'\x00' + b''.join(struct.pack('BBBB', *p) for p in row) for row in pixels)
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xFFFFFFFF)
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', SIZE, SIZE, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')
    path.write_bytes(png)


def sword(c):
    c.polygon([(7, 25), (22, 6), (26, 6), (26, 10), (10, 28)], STEEL)
    c.line(8, 21, 14, 27, 3, GOLD)
    c.line(5, 29, 9, 25, 3, LEATHER)
    c.circle(4.5, 29.5, 1.8, GOLD)


def axe(c):
    c.line(8, 28, 22, 6, 3, WOOD)
    c.polygon([(16, 6), (26, 3), (29, 12), (22, 17), (18, 14)], STEEL)
    c.line(21, 9, 25, 15, 1.2, DARK_STEEL)


def dagger(c):
    c.polygon([(12, 22), (24, 7), (26, 9), (15, 25)], STEEL)
    c.line(9, 19, 17, 27, 2.6, GOLD)
    c.line(7, 28, 11, 24, 3, LEATHER)


def mace(c):
    c.line(9, 28, 19, 12, 3, WOOD)
    c.circle(21, 9, 6, DARK_STEEL)
    for a in range(0, 360, 60):
        r = math.radians(a)
        c.circle(21 + math.cos(r) * 6.5, 9 + math.sin(r) * 6.5, 1.7, STEEL)


def staff(c):
    c.line(7, 29, 23, 7, 3, WOOD)
    c.circle(24.5, 6, 4, (110, 170, 240))
    c.circle(23.5, 5, 1.5, (220, 240, 255))


def wand(c):
    c.line(8, 26, 22, 10, 2.6, WOOD)
    c.circle(23, 9, 3, (200, 120, 230))
    c.line(10, 24, 13, 21, 3.2, GOLD)


def bow(c):
    c.arc(10, 16, 13, -1.2, 1.2, 2.6, WOOD)
    c.line(14.7, 3.9, 14.7, 28.1, 0.9, (230, 230, 220))
    c.line(6, 16, 22, 16, 1.4, DARK_STEEL)
    c.polygon([(22, 13.5), (27, 16), (22, 18.5)], STEEL)


def crossbow(c):
    c.line(9, 25, 22, 12, 3.2, WOOD)
    c.arc(18, 17, 11, -2.5, -0.7, 2.4, DARK_STEEL)
    c.line(9.5, 9, 26, 25, 0.9, (230, 230, 220))
    c.rect(6, 25, 10, 29, LEATHER)


def shield(c):
    c.polygon([(6, 5), (26, 5), (26, 15), (16, 29), (6, 15)], WOOD)
    c.polygon([(8, 7), (24, 7), (24, 14.5), (16, 26), (8, 14.5)], (176, 124, 70))
    c.line(16, 6, 16, 27, 2, DARK_STEEL)
    c.line(7, 13, 25, 13, 2, DARK_STEEL)


def helmet(c):
    c.ellipse(16, 16, 11, 11, STEEL)
    c.rect(4, 16, 28, 27, (0, 0, 0)) if False else None
    c.rect(5, 17, 27, 24, STEEL)
    c.rect(9, 18, 23, 21, OUTLINE)
    c.rect(15, 5, 17, 28, DARK_STEEL)


def chest(c):
    c.polygon([(9, 4), (23, 4), (29, 10), (25, 13), (25, 28), (7, 28), (7, 13), (3, 10)], LEATHER)
    c.polygon([(12, 4), (20, 4), (16, 10)], (90, 56, 34))
    c.line(7, 20, 25, 20, 2, DARK_STEEL)


def gloves(c):
    c.polygon([(8, 28), (8, 14), (10, 7), (12, 14), (13, 6), (15, 14), (16, 6), (18, 14), (19, 8), (21, 15), (25, 13), (23, 21), (21, 28)], LEATHER)
    c.rect(7, 24, 22, 29, (110, 68, 40))


def boots(c):
    c.polygon([(9, 4), (18, 4), (18, 20), (27, 23), (27, 28), (9, 28)], LEATHER)
    c.rect(8, 4, 19, 8, (110, 68, 40))
    c.rect(9, 25, 27, 28, (90, 56, 34))


def potion(c):
    c.circle(16, 20, 9, GLASS)
    c.circle(16, 21, 7.5, RED)
    c.rect(13, 5, 19, 12, GLASS)
    c.rect(12, 3, 20, 6, WOOD)
    c.circle(13, 18, 1.6, (255, 200, 200))


def arrows(c):
    for i, x in enumerate((9, 15, 21)):
        c.line(x, 28, x + 4, 7, 1.6, WOOD)
        c.polygon([(x + 2.3, 7.5), (x + 4.5, 2), (x + 6, 8)], STEEL)
        c.line(x - 0.5, 27, x + 0.8, 22, 2.6, (230, 230, 220))


def bolts(c):
    for x in (10, 18):
        c.line(x, 28, x + 3, 9, 2.2, WOOD)
        c.polygon([(x + 1, 9.5), (x + 3.5, 3), (x + 5.5, 10)], DARK_STEEL)
        c.line(x - 0.5, 27, x + 0.5, 23, 3, (200, 60, 50))


def key(c):
    c.circle(10, 10, 6, GOLD)
    c.circle(10, 10, 2.6, OUTLINE)
    c.line(14, 14, 27, 27, 3, GOLD)
    c.line(21, 21, 25, 17, 2.4, GOLD)
    c.line(24, 24, 28, 20, 2.4, GOLD)


ICONS = {
    'sword': sword, 'axe': axe, 'dagger': dagger, 'mace': mace, 'staff': staff, 'wand': wand, 'bow': bow,
    'crossbow': crossbow, 'shield': shield, 'helmet': helmet, 'chest': chest, 'gloves': gloves, 'boots': boots,
    'potion': potion, 'arrows': arrows, 'bolts': bolts, 'key': key,
}

if __name__ == '__main__':
    directory = Path(sys.argv[1] if len(sys.argv) > 1 else 'Assets/UI')
    for name, draw in ICONS.items():
        canvas = Canvas()
        draw(canvas)
        write_png(directory / f'icon_{name}.png', canvas.pixels())
        print(f'icon_{name}.png')
