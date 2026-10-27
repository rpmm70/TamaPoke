#!/usr/bin/env python3
"""Animated sprite (GIF) importer for TamaPoke.

Converts animated GIFs to game-ready 48x48 frames: crop that is stable across
frames, frame reduction to keyframes, scaling, binary alpha and per-species
color quantization. (Helper script, not part of the current pipeline.)

  python3 tools/import_gif.py            # converts tools/downloads/*.gif and
                                         # renders tools/import_sheet.png
"""
#region Imports
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tamapoke_common import TOOLS_DIR, render_contact_sheet
#endregion

#region Configuration
CELL_SIZE = 48         # game grid
KEYFRAME_COUNT = 4     # idle frames we keep
MAX_COLORS = 24        # maximum palette per species
ALPHA_THRESHOLD = 96   # binary alpha threshold
SOURCE_DIR = os.path.join(TOOLS_DIR, 'downloads')
SHEET_PATH = os.path.join(TOOLS_DIR, 'import_sheet.png')
#endregion

#region Internal helpers

def load_gif_frames(path):
    gif_image = Image.open(path)
    frame_count = getattr(gif_image, 'n_frames', 1)
    frames = []
    for frame_index in range(frame_count):
        gif_image.seek(frame_index)
        frames.append(gif_image.convert('RGBA'))
    return frames


def compute_union_bounding_box(frames):
    # box shared by all frames so it does not "dance" when cropped
    left, top, right, bottom = frames[0].size[0], frames[0].size[1], 0, 0
    for frame in frames:
        frame_box = frame.getchannel('A').point(
            lambda alpha: 255 if alpha >= ALPHA_THRESHOLD else 0).getbbox()
        if not frame_box:
            continue
        left, top = min(left, frame_box[0]), min(top, frame_box[1])
        right, bottom = max(right, frame_box[2]), max(bottom, frame_box[3])
    return (left, top, right, bottom)


def pick_keyframes(frames, keyframe_count=KEYFRAME_COUNT):
    if len(frames) <= keyframe_count:
        return frames
    step = len(frames) / keyframe_count
    return [frames[int(i * step)] for i in range(keyframe_count)]


def fit_frame_into_cell(frame, bounding_box):
    cropped = frame.crop(bounding_box)
    width, height = cropped.size
    scale = min(CELL_SIZE / width, CELL_SIZE / height, 1.0)
    new_width, new_height = max(1, round(width * scale)), max(1, round(height * scale))
    cropped = cropped.resize((new_width, new_height), Image.LANCZOS)
    cell = Image.new('RGBA', (CELL_SIZE, CELL_SIZE), (0, 0, 0, 0))
    cell.paste(cropped, ((CELL_SIZE - new_width) // 2, CELL_SIZE - new_height))  # resting on the ground
    #region Binary alpha
    cell_pixels = cell.load()
    for y in range(CELL_SIZE):
        for x in range(CELL_SIZE):
            red, green, blue, alpha = cell_pixels[x, y]
            cell_pixels[x, y] = (red, green, blue, 255) if alpha >= ALPHA_THRESHOLD else (0, 0, 0, 0)
    #endregion
    return cell


def quantize_cells_together(cells, max_colors=MAX_COLORS):
    # palette shared by all frames of the species (so the color does not flicker)
    strip = Image.new('RGBA', (CELL_SIZE * len(cells), CELL_SIZE))
    for i, cell in enumerate(cells):
        strip.paste(cell, (i * CELL_SIZE, 0))
    quantized_strip = strip.convert('RGB').quantize(colors=max_colors, method=Image.MEDIANCUT)
    quantized_strip = quantized_strip.convert('RGB')
    quantized_cells = []
    for i in range(len(cells)):
        quantized_cell = quantized_strip.crop((i * CELL_SIZE, 0, (i + 1) * CELL_SIZE, CELL_SIZE)).convert('RGBA')
        quantized_pixels, original_pixels = quantized_cell.load(), cells[i].load()
        for y in range(CELL_SIZE):
            for x in range(CELL_SIZE):
                if original_pixels[x, y][3] == 0:
                    quantized_pixels[x, y] = (0, 0, 0, 0)
        quantized_cells.append(quantized_cell)
    return quantized_cells


def convert_gif_to_cells(path):
    frames = load_gif_frames(path)
    bounding_box = compute_union_bounding_box(frames)
    keyframes = pick_keyframes(frames)
    cells = [fit_frame_into_cell(frame, bounding_box) for frame in keyframes]
    return quantize_cells_together(cells)


def render_import_sheet(cells_by_name, path=SHEET_PATH, scale=4):
    """One row per GIF (its name under the first frame), one column per keyframe."""
    def make_cell_painter(cell):
        enlarged = cell.resize((CELL_SIZE * scale, CELL_SIZE * scale), Image.NEAREST)
        return lambda image, draw, left, top: image.paste(enlarged, (left, top), enlarged)

    tiles = []
    for row, (name, cells) in enumerate(cells_by_name.items()):
        for column, cell in enumerate(cells):
            tiles.append((row, column, name if column == 0 else None, make_cell_painter(cell)))
    render_contact_sheet(tiles, path, tile_size=CELL_SIZE * scale, pad=12, label_height=16,
                         label_offset=2, group_gap=24, column_count=KEYFRAME_COUNT)

#endregion

#region Public API

def main():
    cells_by_name = {}
    for file_name in sorted(os.listdir(SOURCE_DIR)):
        if file_name.endswith('.gif'):
            cells_by_name[file_name[:-4]] = convert_gif_to_cells(os.path.join(SOURCE_DIR, file_name))
    if not cells_by_name:
        print(f"no GIFs in {SOURCE_DIR}")
        sys.exit(1)
    render_import_sheet(cells_by_name)


if __name__ == '__main__':
    main()

#endregion
