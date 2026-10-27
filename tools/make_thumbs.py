#!/usr/bin/env python3
"""Generates /mons/thumbs.bin: 40x40 thumbnails of every dex species for the gallery.

They are derived from the front frame (Idle, frame 0) of the already packed PMD
sprites (tools/sdcard/mons/pNNN.bin, TPK2 format) -> license-compliant thumbnails
(CC BY-NC), same style as the main screen. The TPTH layout is documented in
tools/sprite_formats.py.

  python3 tools/make_thumbs.py
  python3 tools/make_thumbs.py --output some/other/thumbs.bin
"""
#region Imports
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dex_data import DEX
from sprite_formats import (MAX_PALETTE_COLORS, TRANSPARENT_INDEX, build_tpth_blob,
                            build_tpth_bytes, read_tpk2_first_frame)
from tamapoke_common import MONS_DIR
#endregion

#region Configuration
THUMBNAIL_CELL_SIZE = 40
#endregion

#region Internal helpers

def read_pmd_idle_first_frame(path):
    """Frame 0 of the Idle action (id 0 = front view) of a PMD TPK2 sprite."""
    with open(path, 'rb') as sprite_file:
        return read_tpk2_first_frame(sprite_file)


def shrink_frame_to_cell(width, height, palette, pixel_data):
    """Nearest-neighbor scale into the cell, keeping the aspect ratio, with a compacted palette."""
    scale = min(THUMBNAIL_CELL_SIZE / width, THUMBNAIL_CELL_SIZE / height, 1.0)
    new_width, new_height = max(1, round(width * scale)), max(1, round(height * scale))
    output_pixels = bytearray()
    new_index_by_color = {}
    new_palette = []
    for target_y in range(new_height):
        source_y = min(height - 1, int(target_y / scale)) if scale < 1 else target_y
        for target_x in range(new_width):
            source_x = min(width - 1, int(target_x / scale)) if scale < 1 else target_x
            palette_index = pixel_data[source_y * width + source_x]
            if palette_index == TRANSPARENT_INDEX:
                output_pixels.append(TRANSPARENT_INDEX)
                continue
            color = palette[palette_index]
            if color not in new_index_by_color:
                new_index_by_color[color] = len(new_palette)
                new_palette.append(color)
            output_pixels.append(new_index_by_color[color])
    return new_width, new_height, new_palette, bytes(output_pixels)

#endregion

#region Public API

def main():
    #region Arguments
    argument_parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    argument_parser.add_argument('--output', default=os.path.join(MONS_DIR, 'thumbs.bin'),
                                 help='where to write thumbs.bin')
    arguments = argument_parser.parse_args()
    #endregion

    #region Build one blob per species
    blobs = []
    for species_number in range(1, len(DEX) + 1):
        sprite_path = os.path.join(MONS_DIR, f'p{species_number:03d}.bin')
        thumbnail = shrink_frame_to_cell(*read_pmd_idle_first_frame(sprite_path))
        if len(thumbnail[2]) > MAX_PALETTE_COLORS:
            raise ValueError(f'{species_number}: palette has {len(thumbnail[2])} colors')
        blobs.append(build_tpth_blob(*thumbnail))
    #endregion

    #region Write thumbs.bin
    file_bytes = build_tpth_bytes(blobs)
    with open(arguments.output, 'wb') as output_file:
        output_file.write(file_bytes)
    print(f"saved {arguments.output}: {len(file_bytes) / 1024:.0f} KB, {len(blobs)} thumbnails")
    #endregion


if __name__ == '__main__':
    main()

#endregion
