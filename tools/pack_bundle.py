#!/usr/bin/env python3
"""Packs all the SD sprites (tools/sdcard/mons/*.bin) into a single
web/sprites.pak file so the web installer can upload them in one click.

The TPAK layout is documented in tools/sprite_formats.py. The installer
(web/index.html) downloads it, splits it using the index and sends each file
to the board with the PUT protocol (same as tools/send_sd.py).

  python3 tools/pack_bundle.py
  python3 tools/pack_bundle.py --output some/other/sprites.pak
"""
#region Imports
import argparse
import glob
import os
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sprite_formats import build_tpak_index
from tamapoke_common import MONS_DIR, REPO_ROOT
#endregion

#region Public API

def main():
    #region Arguments
    argument_parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    argument_parser.add_argument('--output', default=os.path.join(REPO_ROOT, 'web', 'sprites.pak'),
                                 help='where to write sprites.pak')
    arguments = argument_parser.parse_args()
    #endregion

    #region Collect the sprite files
    sprite_paths = sorted(glob.glob(os.path.join(MONS_DIR, '*.bin')))
    if not sprite_paths:
        raise SystemExit('no sprites in ' + MONS_DIR)
    index_entries = [('mons/' + os.path.basename(sprite_path), os.path.getsize(sprite_path))
                     for sprite_path in sprite_paths]
    #endregion

    #region Write the index, then stream every file after it
    with open(arguments.output, 'wb') as pack_file:
        pack_file.write(build_tpak_index(index_entries))
        for sprite_path in sprite_paths:
            with open(sprite_path, 'rb') as sprite_file:
                shutil.copyfileobj(sprite_file, pack_file)
    #endregion

    total_data_bytes = sum(size for _, size in index_entries)
    print(f'{os.path.normpath(arguments.output)}: {len(sprite_paths)} sprites, '
          f'{total_data_bytes / 1048576:.1f} MB of data '
          f'({os.path.getsize(arguments.output) / 1048576:.1f} MB total)')


if __name__ == '__main__':
    main()

#endregion
