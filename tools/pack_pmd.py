#!/usr/bin/env python3
"""PMD sprite packer (SpriteCollab) for the main screen.

Generates /mons/pNNN.bin (and psNNN.bin shiny) in the multi-action TPK2 format
(layout in tools/sprite_formats.py).

Actions: 0 Idle, 1 WalkL, 2 WalkR, 3 Sleep, 4 Eat, 5 Hurt, 6 Attack,
7 Pose, 8 Hop, 9 Nod, 10 DeepBreath, 11 Sit. Missing ones are skipped.

  python3 tools/pack_pmd.py             # every dex species, normal + shiny
  python3 tools/pack_pmd.py 7 25        # specific dex numbers
  python3 tools/pack_pmd.py normal 1 4  # normal only
  python3 tools/pack_pmd.py --jobs 4    # parallel downloads/packing (default 8)
"""
#region Imports
import argparse
import os
import sys
import xml.etree.ElementTree as ElementTree
from concurrent.futures import ThreadPoolExecutor

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dex_data import DEX
from sprite_formats import MAX_PALETTE_COLORS, TRANSPARENT_INDEX, build_tpk2_bytes
from tamapoke_common import (MONS_DIR, TOOLS_DIR, download_with_retry, rgb_to_rgb565,
                             write_bytes_atomically)
#endregion

#region Configuration
CACHE_DIR = os.path.join(TOOLS_DIR, 'pmd_cache')
SPRITECOLLAB_BASE_URL = 'https://raw.githubusercontent.com/PMDCollab/SpriteCollab/master/sprite'
SLOWDOWN_FACTOR = 1.4     # PMD's original pace feels fast on the tamagotchi
MINIMUM_FRAME_MS = 70
ALPHA_THRESHOLD = 128     # pixels below this alpha are transparent
MAXIMUM_FRAMES_PER_ACTION = 24
DEFAULT_JOB_COUNT = 8

# (id, action name, sheet row) - row 0 if the sheet has only one
# sheet directions: 0 down, 2 RIGHT, 6 LEFT (verified on the board)
ACTIONS = [
    (0, 'Idle', 0),
    (1, 'Walk', 6),   # left
    (2, 'Walk', 2),   # right
    (3, 'Sleep', 0),
    (4, 'Eat', 0),
    (5, 'Hurt', 0),
    (6, 'Attack', 0),
    (7, 'Pose', 0),
    (8, 'Hop', 0),
    (9, 'Nod', 0),
    (10, 'DeepBreath', 0),
    (11, 'Sit', 0),
]
#endregion

#region Internal helpers

def fetch_to_cache(url, destination_path):
    """True if the file is in the cache (already there or just downloaded)."""
    if os.path.exists(destination_path):
        return True
    return download_with_retry(url, destination_path) is not None


def load_animation_data(species_folder):
    """AnimData.xml -> {name: (frame width, frame height, durations, source animation name)}."""
    animations = {}
    tree = ElementTree.parse(os.path.join(species_folder, 'AnimData.xml'))
    for animation_node in tree.getroot().find('Anims'):
        name = animation_node.find('Name').text
        if animation_node.find('FrameWidth') is None:
            #region Alias (CopyOf): points to another animation
            copy_of_node = animation_node.find('CopyOf')
            if copy_of_node is not None:
                animations[name] = ('copy', copy_of_node.text)
            continue
            #endregion
        animations[name] = (int(animation_node.find('FrameWidth').text),
                            int(animation_node.find('FrameHeight').text),
                            [int(duration.text) for duration in animation_node.find('Durations')],
                            name)
    #region Resolve aliases (the PNG is the original animation's)
    for name, value in list(animations.items()):
        if isinstance(value, tuple) and value[0] == 'copy':
            animations[name] = animations.get(value[1])
    #endregion
    return animations


def pack_species_sprite(species_number, shiny=False):
    """Downloads (or reuses the cache) and writes one TPK2 file. Returns a log line."""
    #region Locate the species on SpriteCollab and in the cache
    shiny_subpath = '/0000/0001' if shiny else ''
    species_folder = os.path.join(CACHE_DIR, f'{species_number:04d}{"s" if shiny else ""}')
    species_url = f'{SPRITECOLLAB_BASE_URL}/{species_number:04d}{shiny_subpath}'
    if not fetch_to_cache(f'{species_url}/AnimData.xml', os.path.join(species_folder, 'AnimData.xml')):
        raise RuntimeError('no AnimData.xml')
    animations = load_animation_data(species_folder)
    #endregion

    #region Quantize every action into one shared palette
    palette_index_by_color, palette_colors = {}, []
    packed_actions = []
    for action_id, action_name, sheet_row in ACTIONS:
        if animations.get(action_name) is None:
            continue
        frame_width, frame_height, durations, source_name = animations[action_name]
        sheet_path = os.path.join(species_folder, f'{source_name}-Anim.png')
        if not fetch_to_cache(f'{species_url}/{source_name}-Anim.png', sheet_path):
            continue
        sheet_image = Image.open(sheet_path).convert('RGBA')
        sheet_row_count = sheet_image.size[1] // frame_height
        used_row = sheet_row if sheet_row_count > sheet_row else 0
        frame_count = min(len(durations), sheet_image.size[0] // frame_width, MAXIMUM_FRAMES_PER_ACTION)
        pixel_data = bytearray()
        for frame_index in range(frame_count):
            frame_image = sheet_image.crop((frame_index * frame_width, used_row * frame_height,
                                            (frame_index + 1) * frame_width, (used_row + 1) * frame_height))
            for pixel in frame_image.getdata():
                if pixel[3] < ALPHA_THRESHOLD:
                    pixel_data.append(TRANSPARENT_INDEX)
                    continue
                color = pixel[:3]
                if color not in palette_index_by_color:
                    if len(palette_colors) >= MAX_PALETTE_COLORS:
                        # nearest existing color (rare in PMD, short palettes)
                        nearest_color = min(palette_index_by_color,
                                            key=lambda known: sum((known_channel - channel) ** 2
                                                                  for known_channel, channel in zip(known, color)))
                        palette_index_by_color[color] = palette_index_by_color[nearest_color]
                    else:
                        palette_index_by_color[color] = len(palette_colors)
                        palette_colors.append(color)
                pixel_data.append(palette_index_by_color[color])
        frame_ms = [max(MINIMUM_FRAME_MS, round(duration * 1000 / 60 * SLOWDOWN_FACTOR))
                    for duration in durations[:frame_count]]
        packed_actions.append((action_id, frame_width, frame_height, frame_count, frame_ms,
                               bytes(pixel_data)))
    #endregion

    if not any(packed_action[0] == 0 for packed_action in packed_actions):
        raise RuntimeError('no Idle')

    #region Write the TPK2 file atomically
    output_name = f'p{"s" if shiny else ""}{species_number:03d}.bin'
    file_bytes = build_tpk2_bytes([rgb_to_rgb565(*color) for color in palette_colors], packed_actions)
    write_bytes_atomically(os.path.join(MONS_DIR, output_name), file_bytes)
    return (f"  -> {output_name}: {len(packed_actions)} actions, "
            f"{len(palette_colors)} colors, {len(file_bytes) / 1024:.0f} KB")
    #endregion


def run_pack_job(species_number, shiny):
    """Thread pool job: (species, shiny, log lines, succeeded)."""
    header = f"#{species_number:03d}{' shiny' if shiny else ''}"
    try:
        return species_number, shiny, [header, pack_species_sprite(species_number, shiny)], True
    except Exception as pack_error:  # one species failing must not stop the rest
        return species_number, shiny, [header, f"  FAILED: {pack_error}"], False

#endregion

#region Public API

def main():
    #region Arguments
    argument_parser = argparse.ArgumentParser(description='Packs PMD sprites into TPK2 files.')
    argument_parser.add_argument('selection', nargs='*',
                                 help="dex numbers to pack (default: all); 'normal' skips the shiny ones")
    argument_parser.add_argument('--jobs', type=int, default=DEFAULT_JOB_COUNT,
                                 help='species packed in parallel')
    arguments = argument_parser.parse_args()
    unknown_words = [word for word in arguments.selection if word != 'normal' and not word.isdigit()]
    if unknown_words:
        argument_parser.error(f'unexpected arguments: {unknown_words}')
    normal_only = 'normal' in arguments.selection
    species_numbers = ([int(word) for word in arguments.selection if word.isdigit()]
                       or list(range(1, len(DEX) + 1)))
    #endregion

    #region Pack in parallel, print in order
    jobs = [(species_number, shiny) for species_number in species_numbers
            for shiny in ([False] if normal_only else [False, True])]
    failures = []
    with ThreadPoolExecutor(max_workers=max(1, arguments.jobs)) as executor:
        for species_number, shiny, log_lines, succeeded in executor.map(lambda job: run_pack_job(*job), jobs):
            print('\n'.join(log_lines))
            if not succeeded:
                failures.append((species_number, shiny))
    print(f"FAILURES: {failures}" if failures else "ALL PACKED")
    #endregion


if __name__ == '__main__':
    main()

#endregion
