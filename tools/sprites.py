#!/usr/bin/env python3
"""TamaPoke sprite workshop.

The 32x32 sprites are BUILT from primitives (ellipses with automatic
shading on the lower-right edge, automatic silhouette outline) and
pixel-level details (eyes, mouth, spots). The script validates, renders a
PNG contact sheet for visual review, and emits the C and JS arrays:

  python3 tools/sprites.py        # validate + render tools/sheet.png
  python3 tools/sprites.py emit   # also regenerate species.h and emitted_sprites.js

Pillow is only needed to render the sheet (--no-sheet skips it).
"""
#region Imports
import argparse
import json
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dex_data import DEX, TYPE_ACCENTS
from tamapoke_common import REPO_ROOT, TOOLS_DIR, hex_to_rgb565, render_contact_sheet, write_generated_text
#endregion

#region Palette
PALETTE = {
    'k': '#1b1b25',  # outline / eyes
    'w': '#ffffff',  # eye highlight
    'y': '#f8d990',  # cream (belly, shell plate)
    'Y': '#e0b860',  # cream shadow
    'o': '#f5863d',  # orange
    'O': '#d65f28',  # orange shadow
    'r': '#e8503a',  # red (flame, charmeleon)
    'R': '#b53224',  # red shadow
    'f': '#ffd95e',  # flame yellow
    't': '#8fd6b4',  # mint (bulbasaur)
    'T': '#5fae8c',  # mint shadow
    'g': '#58b868',  # bulb green
    'G': '#3c8a4c',  # bulb green shadow
    'd': '#3f7e62',  # bulbasaur spots
    'p': '#f08aa4',  # flower pink
    'P': '#c75f80',  # pink shadow
    'b': '#7cc4ea',  # blue (squirtle)
    'B': '#4f93c4',  # blue shadow
    'N': '#3a6fa0',  # blastoise blue
    'M': '#2a5278',  # blastoise blue shadow
    'c': '#b07a45',  # shell brown
    'C': '#7e5530',  # brown shadow
    'l': '#9aa9e0',  # lavender (wartortle)
    'L': '#6f7cb8',  # lavender shadow
    's': '#aab0bc',  # cannon gray
    'S': '#787e8c',  # gray shadow
    'h': '#fff4d6',  # egg highlight cream
    'e': '#c99a48',  # egg core shadow
    'q': '#8fdc9a',  # egg spot highlight green
}

W = H = 32
EGG_SIZE = 80  # the egg is drawn at its own size (zoom 2 instead of the creatures' 5)
#endregion

#region Sprite canvas

class G:
    def __init__(self, size=W):
        self.size = size
        self.g = [['.'] * size for _ in range(size)]

    def disk(self, cx, cy, rx, ry, ch, shade=None):
        for y in range(self.size):
            for x in range(self.size):
                dx, dy = (x - cx) / rx, (y - cy) / ry
                n = dx * dx + dy * dy
                if n <= 1.0:
                    c = ch
                    if shade and n >= 0.45 and (dx + dy) > 0.42:
                        c = shade
                    self.g[y][x] = c

    def rect(self, x0, y0, x1, y1, ch):
        for y in range(max(0, y0), min(self.size, y1 + 1)):
            for x in range(max(0, x0), min(self.size, x1 + 1)):
                self.g[y][x] = ch

    def px(self, x, y, ch):
        if 0 <= x < self.size and 0 <= y < self.size:
            self.g[y][x] = ch

    def erase(self, cx, cy, rx, ry):
        for y in range(self.size):
            for x in range(self.size):
                dx, dy = (x - cx) / rx, (y - cy) / ry
                if dx * dx + dy * dy <= 1.0:
                    self.g[y][x] = '.'

    def outline(self):
        out = [row[:] for row in self.g]
        for y in range(self.size):
            for x in range(self.size):
                if self.g[y][x] == '.':
                    continue
                for nx, ny in ((x-1, y), (x+1, y), (x, y-1), (x, y+1)):
                    if not (0 <= nx < self.size and 0 <= ny < self.size) or self.g[ny][nx] == '.':
                        out[y][x] = 'k'
                        break
        self.g = out

    def eye(self, x, y):
        # 3x4 eye with highlight
        self.rect(x, y, x + 2, y + 3, 'k')
        self.px(x, y + 1, 'w')
        self.px(x + 1, y, 'w')

    def smile(self, cx, row):
        self.px(cx - 3, row, 'k')
        self.rect(cx - 2, row + 1, cx + 2, row + 1, 'k')
        self.px(cx + 3, row, 'k')

    def toes(self, x, y):
        self.px(x, y, 'k')

    def rows(self):
        return [''.join(r) for r in self.g]

#endregion

#region Fire line

def charmander():
    g = G()
    # thick tail rising on the right, with flame
    g.rect(21, 23, 25, 26, 'o')
    g.rect(24, 19, 27, 24, 'O')
    g.rect(26, 15, 28, 20, 'O')
    g.disk(27.5, 12, 3.4, 4.2, 'r', 'R')
    g.px(27, 7, 'r'); g.px(28, 8, 'r')
    g.disk(27.2, 13.2, 1.6, 2.2, 'f')
    # big head + body
    g.disk(14, 9.5, 8.2, 7.4, 'o', 'O')
    g.disk(14, 22, 7.0, 6.2, 'o', 'O')
    # arms
    g.rect(5, 18, 7, 22, 'o')
    g.rect(21, 18, 23, 20, 'o')
    # legs
    g.rect(7, 27, 11, 30, 'o')
    g.rect(17, 27, 21, 30, 'o')
    g.outline()
    # cream belly
    g.disk(14, 22.5, 4.6, 4.2, 'y', 'Y')
    # toes
    g.toes(9, 30); g.toes(19, 30)
    g.eye(9, 6); g.eye(17, 6)
    g.smile(13, 12)
    return g.rows()


def charmeleon():
    g = G()
    # long tail with big flame
    g.rect(21, 22, 25, 25, 'r')
    g.rect(24, 17, 27, 23, 'R')
    g.disk(27, 12.5, 3.8, 4.6, 'r', 'R')
    g.px(26, 6, 'r'); g.px(27, 7, 'r'); g.px(28, 6, 'r')
    g.disk(26.8, 13.8, 1.8, 2.4, 'f')
    # backward horn
    g.px(19, 2, 'r'); g.px(20, 1, 'r'); g.px(21, 0, 'r')
    g.rect(18, 3, 20, 4, 'r'); g.px(20, 2, 'r'); g.px(21, 1, 'r')
    # head + slender body
    g.disk(13, 9.5, 7.8, 7.0, 'r', 'R')
    g.disk(13, 22, 6.4, 6.2, 'r', 'R')
    # clawed arms
    g.rect(4, 17, 6, 22, 'r')
    g.rect(20, 17, 22, 20, 'r')
    # legs
    g.rect(6, 27, 10, 30, 'r')
    g.rect(16, 27, 20, 30, 'r')
    g.outline()
    g.disk(13, 22.5, 4.2, 4.2, 'y', 'Y')
    g.toes(8, 30); g.toes(18, 30)
    g.eye(8, 6); g.eye(16, 6)
    g.smile(12, 12)
    return g.rows()


def charizard():
    g = G()
    # wings: greenish membrane with a tall peak (behind)
    for i, (x0, x1) in enumerate([(1, 3), (1, 4), (1, 5), (2, 6), (2, 7), (2, 7),
                                  (3, 8), (3, 8), (3, 9), (4, 9), (4, 9), (5, 9)]):
        g.rect(x0, 4 + i, x1, 4 + i, 'T')
    for i, (x0, x1) in enumerate([(28, 30), (27, 30), (26, 30), (25, 29), (24, 29), (24, 29),
                                  (23, 28), (23, 28), (22, 28), (22, 27), (22, 27), (22, 26)]):
        g.rect(x0, 4 + i, x1, 4 + i, 'T')
    g.px(1, 3, 'T'); g.px(30, 3, 'T')
    # tail with flame
    g.rect(22, 25, 26, 27, 'o')
    g.rect(25, 21, 28, 26, 'O')
    g.disk(28, 18, 3.0, 3.8, 'r', 'R')
    g.px(28, 13, 'r'); g.px(27, 14, 'r')
    g.disk(27.8, 19.2, 1.4, 2.0, 'f')
    # bulky body + head
    g.disk(15, 23, 8.4, 7.0, 'o', 'O')
    g.disk(15, 9, 7.0, 6.2, 'o', 'O')
    # horns
    g.rect(10, 1, 11, 3, 'o'); g.rect(19, 1, 20, 3, 'o')
    # wide legs
    g.rect(7, 28, 12, 31, 'o')
    g.rect(18, 28, 23, 31, 'o')
    g.outline()
    g.disk(15, 23.5, 5.2, 4.8, 'y', 'Y')
    g.toes(9, 31); g.toes(20, 31)
    g.eye(10, 6); g.eye(18, 6)
    g.smile(14, 12)
    return g.rows()


#endregion

#region Grass line

def bulbasaur():
    g = G()
    # bulb with sprout
    g.disk(21.5, 8.5, 7.2, 6.4, 'g', 'G')
    g.rect(20, 1, 22, 2, 'G')
    # wide body (quadruped)
    g.disk(14, 22, 9.6, 6.4, 't', 'T')
    # head
    g.disk(10, 13, 7.8, 6.2, 't', 'T')
    # ears
    g.px(3, 4, 't'); g.rect(3, 5, 5, 8, 't'); g.px(4, 4, 't')
    g.px(13, 3, 't'); g.rect(12, 4, 14, 7, 't'); g.px(14, 3, 't')
    # legs
    g.rect(4, 26, 7, 30, 't')
    g.rect(10, 26, 13, 30, 't')
    g.rect(16, 26, 19, 30, 't')
    g.rect(21, 25, 24, 29, 't')
    g.outline()
    # leaves hugging the base of the bulb
    g.px(15, 13, 'G'); g.px(16, 14, 'G'); g.px(17, 14, 'G')
    g.px(26, 13, 'G'); g.px(25, 14, 'G')
    # spots
    g.rect(7, 7, 8, 7, 'd'); g.rect(13, 17, 14, 18, 'd')
    g.rect(5, 21, 6, 22, 'd'); g.rect(17, 22, 18, 23, 'd')
    g.toes(5, 30); g.toes(11, 30); g.toes(17, 30)
    g.eye(6, 10); g.eye(13, 10)
    g.smile(9, 16)
    return g.rows()


def ivysaur():
    g = G()
    # bulb with pink bud and leaves
    g.disk(21.5, 9.5, 7.0, 5.8, 'g', 'G')
    g.disk(21.5, 3.2, 3.6, 3.0, 'p', 'P')
    g.px(21, 0, 'p'); g.px(22, 0, 'p')
    # side leaves
    g.rect(15, 6, 17, 7, 'g'); g.px(14, 5, 'g'); g.px(15, 5, 'g')
    g.rect(28, 6, 30, 7, 'g'); g.px(30, 5, 'g')
    g.disk(14, 22, 9.6, 6.4, 't', 'T')
    g.disk(10, 13, 7.8, 6.2, 't', 'T')
    g.px(3, 4, 't'); g.rect(3, 5, 5, 8, 't'); g.px(4, 4, 't')
    g.px(13, 3, 't'); g.rect(12, 4, 14, 7, 't'); g.px(14, 3, 't')
    g.rect(4, 26, 7, 30, 't')
    g.rect(10, 26, 13, 30, 't')
    g.rect(16, 26, 19, 30, 't')
    g.rect(21, 25, 24, 29, 't')
    g.outline()
    # bud petal division
    g.px(21, 1, 'P'); g.px(21, 2, 'P')
    g.px(15, 13, 'G'); g.px(16, 14, 'G')
    g.rect(7, 7, 8, 7, 'd'); g.rect(13, 17, 14, 18, 'd')
    g.rect(5, 21, 6, 22, 'd')
    g.toes(5, 30); g.toes(11, 30); g.toes(17, 30)
    g.eye(6, 10); g.eye(13, 10)
    g.smile(9, 16)
    return g.rows()


def venusaur():
    g = G()
    # open flower: pink petals + yellow center over leaves
    g.disk(21, 5.5, 7.0, 4.2, 'p', 'P')
    g.disk(21, 6.5, 2.8, 2.0, 'f')
    # petal separations
    g.px(17, 2, 'P'); g.px(17, 3, 'P'); g.px(25, 2, 'P'); g.px(25, 3, 'P')
    # crown of leaves
    g.disk(21, 11, 9.0, 3.2, 'g', 'G')
    # bulky body
    g.disk(15, 23, 11.0, 7.0, 't', 'T')
    g.disk(9, 13.5, 7.6, 6.0, 't', 'T')
    g.px(2, 5, 't'); g.rect(2, 6, 4, 9, 't'); g.px(3, 5, 't')
    g.px(12, 4, 't'); g.rect(11, 5, 13, 8, 't'); g.px(13, 4, 't')
    # trunk legs
    g.rect(3, 26, 7, 30, 't')
    g.rect(10, 26, 14, 30, 't')
    g.rect(17, 26, 21, 30, 't')
    g.rect(23, 25, 27, 29, 't')
    g.outline()
    g.rect(6, 7, 7, 7, 'd'); g.rect(12, 17, 13, 18, 'd')
    g.rect(5, 22, 6, 23, 'd'); g.rect(18, 22, 19, 23, 'd')
    g.toes(5, 30); g.toes(12, 30); g.toes(19, 30)
    g.eye(5, 10); g.eye(12, 10)
    g.smile(8, 16)
    return g.rows()


#endregion

#region Water line

def squirtle():
    g = G()
    # spiral tail (ring with a hole)
    g.disk(23.5, 20, 4.6, 5.2, 'b', 'B')
    g.erase(24.7, 19.2, 1.6, 1.9)
    # big head + body
    g.disk(13, 9, 8.4, 7.6, 'b', 'B')
    g.disk(13, 22.5, 7.0, 5.8, 'b', 'B')
    # arms
    g.rect(4, 17, 6, 22, 'b')
    g.rect(20, 17, 22, 19, 'b')
    # legs
    g.rect(6, 27, 10, 30, 'b')
    g.rect(16, 27, 20, 30, 'b')
    g.outline()
    # shell: brown ring + cream plate
    g.disk(13, 22.5, 5.2, 4.4, 'c', 'C')
    g.disk(13, 22.5, 3.6, 3.0, 'y', 'Y')
    g.toes(8, 30); g.toes(18, 30)
    g.eye(8, 5); g.eye(16, 5)
    g.smile(12, 11)
    return g.rows()


def wartortle():
    g = G()
    # fluffy cloud-like tail
    g.disk(23.5, 18, 5.4, 7.0, 'l', 'L')
    # waves on the tail
    g.px(22, 14, 'L'); g.px(23, 15, 'L'); g.px(24, 16, 'L')
    g.px(21, 19, 'L'); g.px(22, 20, 'L')
    # head + body
    g.disk(12, 9.5, 7.8, 7.0, 'l', 'L')
    g.disk(12, 22.5, 6.6, 5.6, 'l', 'L')
    # wavy ears
    g.px(4, 0, 'l'); g.rect(3, 1, 5, 5, 'l'); g.px(2, 2, 'l')
    g.px(18, 0, 'l'); g.rect(17, 1, 19, 5, 'l'); g.px(20, 2, 'l')
    # arms
    g.rect(3, 17, 5, 21, 'l')
    g.rect(19, 17, 21, 19, 'l')
    # legs
    g.rect(5, 27, 9, 30, 'l')
    g.rect(15, 27, 19, 30, 'l')
    g.outline()
    # inner ears
    g.px(4, 2, 'L'); g.px(4, 3, 'L'); g.px(18, 2, 'L'); g.px(18, 3, 'L')
    g.disk(12, 22.5, 4.8, 4.2, 'c', 'C')
    g.disk(12, 22.5, 3.2, 2.8, 'y', 'Y')
    g.toes(7, 30); g.toes(17, 30)
    g.eye(7, 6); g.eye(15, 6)
    g.smile(11, 12)
    return g.rows()


def blastoise():
    g = G()
    # angled cannons over the shoulders
    g.rect(3, 4, 6, 7, 'S')
    g.rect(4, 7, 7, 18, 's')
    g.px(4, 5, 'k'); g.px(5, 5, 'k')
    g.rect(25, 4, 28, 7, 'S')
    g.rect(24, 7, 27, 18, 's')
    g.px(26, 5, 'k'); g.px(27, 5, 'k')
    # head + bulky body
    g.disk(15.5, 8.5, 7.0, 6.2, 'N', 'M')
    g.disk(15.5, 22, 9.4, 7.2, 'N', 'M')
    # brawny arms
    g.rect(5, 19, 8, 25, 'N')
    g.rect(23, 19, 26, 25, 'N')
    # wide legs
    g.rect(7, 28, 12, 31, 'N')
    g.rect(19, 28, 24, 31, 'N')
    g.outline()
    g.disk(15.5, 22.5, 6.0, 5.0, 'c', 'C')
    g.disk(15.5, 22.5, 4.2, 3.6, 'y', 'Y')
    g.toes(9, 31); g.toes(21, 31)
    g.eye(11, 6); g.eye(19, 6)
    g.smile(15, 12)
    return g.rows()


#endregion

#region Generic sprites

#region Egg
# 80x80 egg, narrower at the top, lit from the upper left: banded cream
# shading with a checkerboard dither between bands, green spots shaded to
# follow the curve, and a two-pixel outline. The crack paths drawn over it
# live in main_screen.cpp (CRACK1 / CRACK2_*) in the same 80x80 coordinates.

EGG_CENTER_X, EGG_CENTER_Y = 39.5, 42.0
EGG_RADIUS_X, EGG_RADIUS_Y = 28.0, 34.0
EGG_SPOTS = [
    # (center x, center y, radius x, radius y)
    (29.0, 27.0, 7.0, 5.5),
    (51.5, 44.0, 7.6, 6.5),
    (32.0, 58.0, 6.0, 4.5),
    (16.5, 44.0, 3.4, 4.2),
    (47.5, 22.0, 3.3, 2.6),
    (57.5, 62.0, 3.3, 3.6),
]


def egg_surface_normal(column, row):
    """(horizontal, vertical, depth) of the egg's surface at a pixel, or None outside it."""
    vertical = (row + 0.5 - EGG_CENTER_Y) / EGG_RADIUS_Y
    if abs(vertical) >= 1.0:
        return None
    # taper: narrower toward the top
    width_factor = 0.86 + 0.14 * vertical
    horizontal = (column + 0.5 - EGG_CENTER_X) / (EGG_RADIUS_X * width_factor)
    radial = horizontal * horizontal + vertical * vertical
    if radial > 1.0:
        return None
    return horizontal, vertical, math.sqrt(max(0.0, 1.0 - radial))


def egg_light_level(surface_normal):
    """Lambert light from the upper left: 1 faces the light, below 0 faces away."""
    light_x, light_y, light_z = -0.55, -0.6, 0.58
    light_length = math.sqrt(light_x ** 2 + light_y ** 2 + light_z ** 2)
    return (surface_normal[0] * light_x + surface_normal[1] * light_y + surface_normal[2] * light_z) / light_length


def egg_spot_offset(column, row):
    """(offset x, offset y) inside the spot covering a pixel (1 = its rim), or None."""
    for center_x, center_y, radius_x, radius_y in EGG_SPOTS:
        offset_x = (column + 0.5 - center_x) / radius_x
        offset_y = (row + 0.5 - center_y) / radius_y
        # slightly irregular edge
        wobble = 1.0 + 0.12 * math.sin(3.0 * math.atan2(offset_y, offset_x) + center_x)
        if offset_x * offset_x + offset_y * offset_y <= wobble:
            return offset_x, offset_y
    return None


def egg():
    egg_grid = [['.'] * EGG_SIZE for _ in range(EGG_SIZE)]

    #region Shell shading and spots
    for row in range(EGG_SIZE):
        for column in range(EGG_SIZE):
            surface_normal = egg_surface_normal(column, row)
            if surface_normal is None:
                continue
            light_level = egg_light_level(surface_normal)
            # checkerboard dither softens the band edges
            dithered_light_level = light_level + (0.035 if (column + row) % 2 == 0 else -0.035)
            if dithered_light_level > 0.965:
                shade = 'w'
            elif dithered_light_level > 0.80:
                shade = 'h'
            elif dithered_light_level > 0.33:
                shade = 'y'
            elif dithered_light_level > -0.02:
                shade = 'Y'
            else:
                shade = 'e'
            spot_offset = egg_spot_offset(column, row)
            if spot_offset is not None:
                if light_level > 0.90:
                    shade = 'q'
                elif light_level > 0.25:
                    shade = 'g'
                else:
                    shade = 'G'
                # inner rim of the spot facing away from the light
                if spot_offset[0] + spot_offset[1] > 0.95:
                    shade = 'G'
            egg_grid[row][column] = shade
    #endregion

    #region Two-pixel outline
    for outline_pass in range(2):
        # pass 0 rings the transparent edge, pass 1 rings the first outline
        boundary_characters = '.' if outline_pass == 0 else '.k'
        outlined_grid = [grid_row[:] for grid_row in egg_grid]
        for row in range(EGG_SIZE):
            for column in range(EGG_SIZE):
                if egg_grid[row][column] in '.k':
                    continue
                for neighbor_column, neighbor_row in ((column - 1, row), (column + 1, row),
                                                      (column, row - 1), (column, row + 1)):
                    if (not (0 <= neighbor_column < EGG_SIZE and 0 <= neighbor_row < EGG_SIZE)
                            or egg_grid[neighbor_row][neighbor_column] in boundary_characters):
                        outlined_grid[row][column] = 'k'
                        break
        egg_grid = outlined_grid
    #endregion

    return [''.join(grid_row) for grid_row in egg_grid]

#endregion


def poop():
    # Hand-drawn 3-tier swirl with a curled tip, cream highlights and a
    # dark-brown shade on the right/bottom of each tier. Pinned here as a
    # literal (like the heart) so species.h stays identical by construction.
    return [
        "................................",
        "................................",
        "................................",
        "................................",
        "................................",
        "................................",
        "................................",
        "................................",
        "................................",
        "................................",
        "................................",
        ".................kk.............",
        "................kcCk............",
        "............kkkkcCCk............",
        "...........kcyccccCCk...........",
        "...........kycccccCCk...........",
        "...........kCCCCCCCCk...........",
        ".........kkcccccccccCkk.........",
        "........kccycccccccccCCk........",
        "........kcyccccccccccCCk........",
        "........kCCCCCCCCCCCCCCk........",
        "......kkcccccccccccccccCkk......",
        ".....kccccccccccccccccccCCk.....",
        ".....kccycccccccccccccccCCk.....",
        ".....kcyccccccccccccccccCCk.....",
        ".....kccccccccccccccccccCCk.....",
        ".....kccccccccccccccccccCCk.....",
        "......kCCCCCCCCCCCCCCCCCCk......",
        "........kkkkkkkkkkkkkkkk........",
        "................................",
        "................................",
        "................................",
    ]


def heart():
    # Hand-drawn and edited directly in species.h (commit 7c2b0f9,
    # "epic animations, heart and biomes"), without coming back here: the
    # generator kept a different procedural version and species.h could no
    # longer be regenerated identically. The heart that actually ships is pinned here.
    return [
        "................................",
        "................................",
        "................................",
        "................................",
        "................................",
        "................................",
        "........krrrrk....krrrrk........",
        ".......krrrrrrk..krrrrrrk.......",
        "......krrrrrrrrrrrrrrrrrrk......",
        ".....krrrrrrrrrrrrrrrrrrrrk.....",
        ".....krrwwrrrrrrrrrrrrrrRrk.....",
        ".....krwwrrrrrrrrrrrrrrRRrk.....",
        "......krrrrrrrrrrrrrrrrrrk......",
        ".......krrrrrrrrrrrrrrrrk.......",
        "........krrrrrrrrrrrrrrk........",
        ".........krrrrrrrrrrrrk.........",
        "..........krrrrrrrrrrk..........",
        "...........krrrrrrrrk...........",
        "............krrrrrrk............",
        ".............krrrrk.............",
        "..............krrk..............",
        "...............kk...............",
        "................................",
        "................................",
        "................................",
        "................................",
        "................................",
        "................................",
        "................................",
        "................................",
        "................................",
        "................................",
    ]


#endregion

#region Button icons

# The icons are hand-drawn pixel art (approved in the CYD screen mockup), so
# they are pinned as literal rows like heart(), not built procedurally.

def icon_food():
    # food button: fork and spoon
    return [
        "................",
        "................",
        "..k.k.k...kkkk..",
        ".kwkskSk.kwsssk.",
        ".kskskSk.kwssSk.",
        ".kskskSk.ksssSk.",
        ".kssssSk.ksssSk.",
        "..kssSk...kSSk..",
        "...ksk....kssk..",
        "...ksk....ksSk..",
        "...ksk....ksSk..",
        "..kssSk..ksssSk.",
        "..kssSk..ksssSk.",
        "..kssSk..ksssSk.",
        "...kkk....kkkk..",
        "................",
    ]


def icon_play():
    # play button: pokeball
    return [
        "................",
        "................",
        "......kkkkk.....",
        "....kkrrrrrkk...",
        "...krwwrrrrrrk..",
        "...krwrrrrrrRk..",
        "..krrrrkkkrrrRk.",
        "..krrrkwwwkrrRk.",
        "..kkkkkwwwkkkkk.",
        "..kwwwkwwwkwwsk.",
        "..kwwwwkkkwwwsk.",
        "...kwwwwwwwwsk..",
        "...kwwwwwwwssk..",
        "....kkwwwsskk...",
        "......kkkkk.....",
        "................",
    ]


def icon_light():
    # sleep button: crescent moon and sparkles
    return [
        "................",
        ".......kkkk.....",
        ".....kkffffk....",
        "....kwfffkk.....",
        "...kwfffk....Y..",
        "...kwffk....YfY.",
        "..kffffk.....Y..",
        "..kffffk........",
        "..kffffk........",
        "..kfffYk........",
        "...kffYk....Y...",
        "...kfffYk.......",
        "....kfffYkk.....",
        ".....kkfffYk....",
        ".......kkkk.....",
        "................",
    ]


def icon_apple():
    # food item 0 (was the red berry)
    return [
        "................",
        "................",
        "........k.kkk...",
        ".......kckggGk..",
        ".......kCkGGk...",
        ".....kkkckkk....",
        "....krwrkrrrk...",
        "...krwwrrrrrrk..",
        "...krwrrrrrrRk..",
        "...krrrrrrrrRk..",
        "...krrrrrrrRRk..",
        "...krrrrrrrRRk..",
        "....krrrrrRRk...",
        ".....kRRkRRk....",
        "......kk.kk.....",
        "................",
    ]


#region Controller icon (main screen play button)
def icon_controller():
    # main screen play button: a gamepad with a d-pad on the left,
    # four colored face buttons on the right and two grips at the bottom
    return [
        "................",
        "................",
        "................",
        "...kkkkkkkkkk...",
        "..kssssssssssk..",
        ".ksskssssssrssk.",
        ".kskkkssssbsfsk.",
        ".ksskssSSssgssk.",
        ".ksssssssssssSk.",
        "kssssSkkkkSsssSk",
        "ksssSk....ksssSk",
        "kssSk......kssSk",
        ".kkk........kkk.",
        "................",
        "................",
        "................",
    ]
#endregion


#region Swords icon (play menu: wild battle)
def icon_swords():
    # play menu item 2: two swords crossing in an X, blades up,
    # gold guards and brown grips toward the bottom corners
    return [
        "..k..........k..",
        ".kSk........ksk.",
        ".kSsk......kwsk.",
        "..kSsk....kwsk..",
        "...kSsk..kwsk...",
        "....kSskkwsk....",
        ".....kSswsk.....",
        "..k...kSsk...k..",
        ".kYk.kwsSsk.kYk.",
        "..kYkwskkSskYk..",
        "...kfkk..kkfk...",
        "..kckYk..kYkck..",
        ".kCk.kYkkYk.kCk.",
        "kfk...k..k...kfk",
        ".k............k.",
        "................",
    ]
#endregion


#region Coin icon (play menu: coin chase)
def icon_coin():
    # gold coin turned slightly, as if spinning: a narrow upright oval face
    # (10x14) with its edge showing as a thin band on the right side
    # (cream at the top, brown below), darker gold rim with brown shading
    # toward the bottom right, white shine at the upper left and a
    # foreshortened 1 px embossed bar in the middle of the face
    return [
        "................",
        ".....kkkkkk.....",
        "....kYYYYkYk....",
        "...kYwwffYkYk...",
        "...kwwfffYkCk...",
        "..kYwffYffYkCk..",
        "..kYfffYffYkCk..",
        "..kYfffYffYkCk..",
        "..kYfffYfYYkCk..",
        "..kYfffYfYYkCk..",
        "..kYffffYYCkCk..",
        "...kYffYYCkCk...",
        "...kYYYYCCkCk...",
        "....kCCCCkCk....",
        ".....kkkkkk.....",
        "................",
    ]
#endregion


#region Boxing glove icon (play menu: training bag)
def icon_glove():
    # play menu item 2: red boxing glove seen from the side, swords move to
    # item 3. Thumb bulges out on the left with a dark seam, white shine at
    # the upper left, darker red shading on the right and a white cuff
    return [
        "................",
        ".....kkkkkk.....",
        "...kkrrrrrrkk...",
        "..krrwwrrrrrRk..",
        "..krwwrrrrrrRk..",
        ".kkkrrrrrrrrRRk.",
        "krrRkrrrrrrrRRk.",
        "krrrRkrrrrrrRRk.",
        "krrrrRkrrrrRRRk.",
        ".krrrrrrrrRRRk..",
        "..kRrrrrrRRRk...",
        "...kkkkkkkkk....",
        "...kwwwwwwSk....",
        "...kwwwwwwSk....",
        "...kkkkkkkkk....",
        "................",
    ]
#endregion


#region Pokedex icon (main screen button under the hamburger)
def icon_pokedex():
    # red Pokedex front: raised top-left lid with the blue lens and two
    # lights, a gray-framed dark screen, then the round button, green pad
    # and d-pad along the bottom; the darker right column is the hinge
    return [
        "................",
        ".kkkkkkkkkk.....",
        ".krkkkrrrrrkkkk.",
        ".kkwbbkrfrgrrRk.",
        ".kkbbBkrrrrrrRk.",
        ".krkkkrrRRRRRRk.",
        ".krrrrrrrrrrrRk.",
        ".krsssssssssrRk.",
        ".krskkkkkkksrRk.",
        ".krskkkkkkksrRk.",
        ".krskkkkkkksrRk.",
        ".krsssssssssrRk.",
        ".krrrrrrrrrkrRk.",
        ".krkkrgggrkkkRk.",
        ".krrrrrrrrrkrRk.",
        ".kkkkkkkkkkkkkk.",
    ]
#endregion


def icon_berry_blue():
    # food item 1: hamburger (keeps the berry name used by the firmware)
    return [
        "................",
        "................",
        ".....kkkkkk.....",
        "...kkwooyookk...",
        "..koyooooooyOk..",
        "..kooooooooOOk..",
        ".kgggggggggggGk.",
        "..kGrrrrrrrrGk..",
        "..kffffffffffk..",
        "..kcfccccccfCk..",
        "..kCCCCCCCCCCk..",
        ".koooooooooooOk.",
        "..kOOOOOOOOOOk..",
        "...kkkkkkkkkk...",
        "................",
        "................",
    ]


def icon_berry_green():
    # food item 2: pizza slice (keeps the berry name used by the firmware)
    return [
        "................",
        "................",
        "...kkkkkkkkkk...",
        "..kcccccccccCk..",
        "..kCCCCCCCCCCk..",
        "..kfwfffffffYk..",
        "...kfrrffffYk...",
        "...kfrRfrrfYk...",
        "....kfffrRYk....",
        "....kfffffYk....",
        ".....krrfYk.....",
        ".....krRfYk.....",
        "......kfYk......",
        "......kfYk......",
        ".......kk.......",
        "................",
    ]


def icon_candy():
    # food item 3: wrapped candy
    return [
        "................",
        "................",
        "................",
        ".k............k.",
        ".kk...kkkk...kk.",
        ".kpk.kpwppk.kpk.",
        ".kpkkpwpppwkkpk.",
        ".kpPkwpppwpkPpk.",
        ".kpPkpppwpPkPpk.",
        ".kpkkppwpPPkkpk.",
        ".kpk.kwpPPk.kpk.",
        ".kk...kkkk...kk.",
        ".k............k.",
        "................",
        "................",
        "................",
    ]


#region Coffee icon (food picker item 4)
def icon_coffee():
    # food item 4: white mug of coffee on a saucer, gray steam rising,
    # brown coffee surface, handle on the right
    return [
        "................",
        "......s...s.....",
        ".....s...s......",
        "......s...s.....",
        ".....s...s......",
        "................",
        "..kkkkkkkkkkk...",
        "..kCCcccccCCk...",
        "..kwwwwwwwwskkk.",
        "..kwwwwwwwwsk.k.",
        "..kwwwwwwwwsk.k.",
        "..kwwwwwwwwskkk.",
        "...kwwwwwwsk....",
        "....kkkkkkk.....",
        ".kkkkkkkkkkkkk..",
        "................",
    ]
#endregion


def icon_clean():
    # bath button: side view of a clawfoot bathtub with bubbles
    return [
        "................",
        "................",
        "................",
        ".kkkkk..........",
        ".kssSk...kk.....",
        ".ksk..kkkwbk.k..",
        ".ksk.kwbkbBkkbk.",
        ".kkkkkkkkkkkkkk.",
        ".kwwwwwwwwwwwsk.",
        ".kkkkkkkkkkkkkk.",
        "..kwwwwwwwwsSk..",
        "..kswwwwwwwsSk..",
        "...kssssssSSk...",
        "...kkkkkkkkkk...",
        "..kk........kk..",
        "................",
    ]

#endregion

#region Sprite table and expression anchors

SPRITES = {
    "CHARMANDER": charmander(),
    "CHARMELEON": charmeleon(),
    "CHARIZARD": charizard(),
    "BULBASAUR": bulbasaur(),
    "IVYSAUR": ivysaur(),
    "VENUSAUR": venusaur(),
    "SQUIRTLE": squirtle(),
    "WARTORTLE": wartortle(),
    "BLASTOISE": blastoise(),
    "ICON_FOOD": icon_food(),
    "ICON_PLAY": icon_play(),
    "ICON_LIGHT": icon_light(),
    "ICON_CLEAN": icon_clean(),
    "ICON_BERRY_B": icon_berry_blue(),
    "ICON_BERRY_G": icon_berry_green(),
    "ICON_CANDY": icon_candy(),
    "ICON_COFFEE": icon_coffee(),
    "ICON_APPLE": icon_apple(),
    "ICON_CONTROLLER": icon_controller(),
    "ICON_SWORDS": icon_swords(),
    "ICON_COIN": icon_coin(),
    "ICON_GLOVE": icon_glove(),
    "ICON_POKEDEX": icon_pokedex(),
    "EGG": egg(),
    "POOP": poop(),
    "HEART": heart(),
}

# anchors (3x4 eyes / mouth) and body color for the overlaid expressions
ANCHORS = {
    "CHARMANDER": dict(eyeRow=6, eyeL=9, eyeR=17, mouthRow=12, mouthCol=13, body='o'),
    "CHARMELEON": dict(eyeRow=6, eyeL=8, eyeR=16, mouthRow=12, mouthCol=12, body='r'),
    "CHARIZARD":  dict(eyeRow=6, eyeL=10, eyeR=18, mouthRow=12, mouthCol=14, body='o'),
    "BULBASAUR":  dict(eyeRow=10, eyeL=6, eyeR=13, mouthRow=16, mouthCol=9, body='t'),
    "IVYSAUR":    dict(eyeRow=10, eyeL=6, eyeR=13, mouthRow=16, mouthCol=9, body='t'),
    "VENUSAUR":   dict(eyeRow=10, eyeL=5, eyeR=12, mouthRow=16, mouthCol=8, body='t'),
    "SQUIRTLE":   dict(eyeRow=5, eyeL=8, eyeR=16, mouthRow=11, mouthCol=12, body='b'),
    "WARTORTLE":  dict(eyeRow=6, eyeL=7, eyeR=15, mouthRow=12, mouthCol=11, body='l'),
    "BLASTOISE":  dict(eyeRow=6, eyeL=11, eyeR=19, mouthRow=12, mouthCol=15, body='N'),
}

#endregion

#region Validation and contact sheet

def validate_sprites():
    all_valid = True
    for name, rows in SPRITES.items():
        expected_width = len(rows)
        for row_index, row in enumerate(rows):
            if len(row) != expected_width:
                print(f"{name} row {row_index}: {len(row)} chars (expected {expected_width})")
                all_valid = False
            unknown_characters = set(row) - set(PALETTE) - {'.'}
            if unknown_characters:
                print(f"{name} row {row_index}: unknown chars {unknown_characters}")
                all_valid = False
    return all_valid


def render_sprite_sheet(path=os.path.join(TOOLS_DIR, 'sheet.png'), scale=10, column_count=3):
    """Every sprite on a light and a dark background, for visual review."""
    def make_sprite_painter(rows):
        pixel_size = scale * W // len(rows)

        def paint_sprite(image, draw, left, top):
            for row_index, row in enumerate(rows):
                for column_index, character in enumerate(row):
                    if character in PALETTE:
                        draw.rectangle([left + column_index * pixel_size, top + row_index * pixel_size,
                                        left + (column_index + 1) * pixel_size - 1,
                                        top + (row_index + 1) * pixel_size - 1],
                                       fill=PALETTE[character])
        return paint_sprite

    tiles = [(sprite_index // column_count, sprite_index % column_count, name, make_sprite_painter(rows))
             for sprite_index, (name, rows) in enumerate(SPRITES.items())]
    render_contact_sheet(tiles, path, tile_size=W * scale, pad=16, label_height=18,
                         label_offset=3, group_gap=16, column_count=column_count)

#endregion

#region Species metadata

DEX_ROW_BY_DISPLAY_NAME = {row[2]: row for row in DEX}

# name -> (evolveLevel, scale). The type accent and the evolution target come
# from tools/dex_data.py. The levels are kept as shipped in species.h: note
# IVYSAUR says 36 here but 32 in dex_data.py (the firmware evolves with the
# dex_data.py value through dex.h; this field is not read by the firmware).
SPECIES_META = {
    "CHARMANDER": (16, 5),
    "CHARMELEON": (36, 6),
    "CHARIZARD":  (0, 7),
    "BULBASAUR":  (16, 5),
    "IVYSAUR":    (36, 6),
    "VENUSAUR":   (0, 7),
    "SQUIRTLE":   (16, 5),
    "WARTORTLE":  (36, 6),
    "BLASTOISE":  (0, 7),
}


def species_accent_hex(name):
    return TYPE_ACCENTS[DEX_ROW_BY_DISPLAY_NAME[name][3]]


def species_evolution_target(name):
    """Name of the flash species it evolves into, or None."""
    evolves_to = DEX_ROW_BY_DISPLAY_NAME[name][4]
    target_name = DEX[evolves_to - 1][2] if evolves_to else None
    return target_name if target_name in SPECIES_META else None

#endregion

#region C header

UI_COLORS = {
    'UI_BG_DAY': '#f2efe1', 'UI_BG_NIGHT': '#141828',
    'UI_INK': '#2a2a36', 'UI_INK_NIGHT': '#d8dcf0',
    'UI_TRACK': '#d8d2bd',
    'UI_BAR_OK': '#58b868', 'UI_BAR_WARN': '#e8a23c', 'UI_BAR_BAD': '#e8503a',
    'UI_WHITE': '#ffffff',
}


def build_species_header_text():
    """species.h text. Does not write anything."""
    species_names = list(SPECIES_META)
    out = []

    #region Includes and constants
    out.append("#pragma once\n\n#pragma region Includes\n#include <stdint.h>\n#pragma endregion\n\n")
    out.append("// GENERATED by tools/sprites.py - edit there and run:\n")
    out.append("//   python3 tools/sprites.py emit\n\n")
    out.append("#pragma region Constants and UI colors\n")
    out.append(f"#define SPRITE_W {W}\n#define SPRITE_H {H}\n\n")
    for color_name, hex_color in UI_COLORS.items():
        out.append(f"#define {color_name} 0x{hex_to_rgb565(hex_color):04X}  // {hex_color}\n")
    out.append(f"#define SPRITE_OUTLINE_COLOR 0x{hex_to_rgb565(PALETTE['k']):04X}  "
               f"// {PALETTE['k']}, spriteColor('k'): outline and eyes\n")
    out.append("#pragma endregion\n\n")
    #endregion

    #region Species ids and struct
    out.append("#pragma region Species ids and struct\n")
    out.append("enum : int8_t {\n")
    for species_index, name in enumerate(species_names):
        out.append(f"  SP_{name}{' = 0' if species_index == 0 else ''},\n")
    out.append("  NUM_SPECIES\n};\n\n")
    out.append(
        "struct Species {\n"
        "  const char *name;\n"
        "  const char *const *sprite;\n"
        "  uint8_t scale;\n"
        "  int8_t evolvesTo;\n"
        "  uint8_t evolveLevel;\n"
        "  uint8_t eyeRow, eyeColL, eyeColR;  // anchors for expressions (3x4 eyes)\n"
        "  uint8_t mouthRow, mouthCol;\n"
        "  uint16_t bodyColor;  // to erase eyes/mouth when expressing\n"
        "  uint16_t accent;     // type's UI color\n"
        "};\n"
        "#pragma endregion\n\n")
    #endregion

    #region Sprite palette
    out.append("#pragma region Sprite palette\n")
    out.append("// sprite character -> RGB565\n")
    out.append("inline uint16_t spriteColor(char ch) {\n  switch (ch) {\n")
    for character, hex_color in PALETTE.items():
        out.append(f"    case '{character}': return 0x{hex_to_rgb565(hex_color):04X};  // {hex_color}\n")
    out.append("    default: return 0;\n  }\n}\n#pragma endregion\n\n")
    #endregion

    #region Sprite bitmaps
    out.append("#pragma region Sprite bitmaps\n")
    for name, rows in SPRITES.items():
        out.append(f"inline constexpr const char *SPR_{name}[{len(rows)}] = {{  // {len(rows)}x{len(rows)}\n")
        for row in rows:
            out.append(f'  "{row}",\n')
        out.append("};\n\n")
    out.append("#pragma endregion\n\n")
    #endregion

    #region Species table
    out.append("#pragma region Species table\n")
    out.append("inline constexpr Species SPECIES[NUM_SPECIES] = {\n")
    for name in species_names:
        evolve_level, scale = SPECIES_META[name]
        evolution_target = species_evolution_target(name)
        evolves_to_text = f"SP_{evolution_target}" if evolution_target else "-1"
        anchor = ANCHORS[name]
        body_color = hex_to_rgb565(PALETTE[anchor['body']])
        accent_color = hex_to_rgb565(species_accent_hex(name))
        out.append(
            f'  {{ "{name}", SPR_{name}, {scale}, {evolves_to_text}, {evolve_level}, '
            f"{anchor['eyeRow']}, {anchor['eyeL']}, {anchor['eyeR']}, {anchor['mouthRow']}, "
            f"{anchor['mouthCol']}, 0x{body_color:04X}, 0x{accent_color:04X} }},\n")
    out.append("};\n#pragma endregion\n")
    #endregion

    return ''.join(out)


def emit_species_header(path=os.path.join(REPO_ROOT, 'species.h')):
    write_generated_text(path, build_species_header_text())
    print(f"saved {path}")

#endregion

#region JS data for the web preview

def build_sprite_data_javascript():
    species_names = list(SPECIES_META)
    meta = {}
    for name in species_names:
        evolve_level, scale = SPECIES_META[name]
        evolution_target = species_evolution_target(name)
        anchor = ANCHORS[name]
        meta[name] = {
            'accent': species_accent_hex(name),
            'evolvesTo': species_names.index(evolution_target) if evolution_target else None,
            'evolveLevel': evolve_level,
            'scale': scale,
            'eyeRow': anchor['eyeRow'], 'eyeL': anchor['eyeL'], 'eyeR': anchor['eyeR'],
            'mouthRow': anchor['mouthRow'], 'mouthCol': anchor['mouthCol'],
            'body': PALETTE[anchor['body']],
        }
    data = {'palette': PALETTE, 'sprites': SPRITES, 'meta': meta, 'order': species_names}
    return "const SPRITE_DATA=" + json.dumps(data, separators=(',', ':')) + ";\n"


def emit_sprite_data_javascript(path=os.path.join(TOOLS_DIR, 'emitted_sprites.js')):
    write_generated_text(path, build_sprite_data_javascript())
    print(f"saved {path}")

#endregion

#region Public API

def main():
    argument_parser = argparse.ArgumentParser(description='TamaPoke sprite workshop.')
    argument_parser.add_argument('command', nargs='?', choices=['emit'],
                                 help='also regenerate species.h and tools/emitted_sprites.js')
    argument_parser.add_argument('--no-sheet', action='store_true',
                                 help='do not render tools/sheet.png (no Pillow needed)')
    arguments = argument_parser.parse_args()

    if not validate_sprites():
        print("-- validation errors --")
        sys.exit(1)
    if not arguments.no_sheet:
        render_sprite_sheet()
    if arguments.command == 'emit':
        emit_species_header()
        emit_sprite_data_javascript()


if __name__ == '__main__':
    main()

#endregion
