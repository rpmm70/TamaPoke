#!/usr/bin/env python3
"""Sends the packed sprites to the board's SD over USB.

  python3 tools/send_sd.py                  # sends tools/sdcard/mons/*.bin
  python3 tools/send_sd.py --port COM5      # or /dev/cu.usbmodem101, /dev/ttyUSB0
  python3 tools/send_sd.py --ls             # lists what is on the SD
"""
#region Imports
import argparse
import glob
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tamapoke_common import MONS_DIR, open_board_serial
#endregion

#region Configuration
TRANSFER_CHUNK_BYTES = 2048
#endregion

#region Internal helpers

def wait_for_board_line(board_serial, expected_line, timeout_seconds=10):
    """True when the board prints `expected_line`, False on ERR or timeout."""
    deadline = time.time() + timeout_seconds
    while time.time() < deadline:
        line = board_serial.readline().decode(errors='replace').strip()
        if not line:
            continue
        print(f"  board: {line}")
        if line == expected_line:
            return True
        if line == 'ERR':
            return False
    return False


def send_file_chunks(board_serial, path):
    """Streams the file, waiting for the board's '#' ack after every chunk."""
    with open(path, 'rb') as source_file:
        while chunk := source_file.read(TRANSFER_CHUNK_BYTES):
            board_serial.write(chunk)
            acknowledgement = ''
            while acknowledgement not in ('#', 'ERR'):
                acknowledgement = board_serial.readline().decode(errors='replace').strip()
                if acknowledgement == '':
                    return False
            if acknowledgement == 'ERR':
                return False
    return True

#endregion

#region Public API

def main():
    #region Arguments and connection
    argument_parser = argparse.ArgumentParser(description='Sends the packed sprites to the SD card.')
    argument_parser.add_argument('--port', help='serial port (default: detect the board)')
    argument_parser.add_argument('--ls', action='store_true', help='list the SD contents')
    arguments = argument_parser.parse_args()

    board_serial, port_name = open_board_serial(arguments.port)
    print(f"port {port_name}")
    time.sleep(1.5)
    board_serial.reset_input_buffer()
    #endregion

    if arguments.ls:
        board_serial.write(b'LS\n')
        wait_for_board_line(board_serial, 'DONE', 5)
        return

    sprite_paths = sorted(glob.glob(os.path.join(MONS_DIR, '*.bin')))
    if not sprite_paths:
        sys.exit("no .bin files; run tools/pack_pmd.py first")

    #region Send every file with the PUT protocol
    for sprite_path in sprite_paths:
        file_size = os.path.getsize(sprite_path)
        name_on_card = f"mons/{os.path.basename(sprite_path)}"
        print(f"-> {name_on_card} ({file_size/1024:.0f} KB)")
        board_serial.write(f"PUT {name_on_card} {file_size}\n".encode())
        if not wait_for_board_line(board_serial, 'OK', 5):
            print("   the board did not accept the PUT, moving on to the next one")
            continue
        start_time = time.time()
        if send_file_chunks(board_serial, sprite_path) and wait_for_board_line(board_serial, 'DONE', 30):
            kilobytes_per_second = file_size / 1024 / max(0.01, time.time() - start_time)
            print(f"   ok ({kilobytes_per_second:.0f} KB/s)")
        else:
            print("   the transfer failed")
    print("done")
    #endregion


if __name__ == '__main__':
    main()

#endregion
