#!/usr/bin/env python3
"""Touch monitor: prints the board's serial output (Ctrl-C to exit).

  python3 tools/touch_log.py                # detects the board's port
  python3 tools/touch_log.py --port COM5
"""
#region Imports
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tamapoke_common import open_board_serial
#endregion

#region Public API

def main():
    argument_parser = argparse.ArgumentParser(description='Prints the board serial output.')
    argument_parser.add_argument('--port', help='serial port (default: detect the board)')
    arguments = argument_parser.parse_args()

    board_serial, port_name = open_board_serial(arguments.port)
    print(f"listening on {port_name}... touch the screen (Ctrl-C to exit)")
    #region Echo every line until Ctrl-C
    try:
        while True:
            line = board_serial.readline().decode(errors='replace').strip()
            if line:
                print(line)
    except KeyboardInterrupt:
        pass
    #endregion


if __name__ == '__main__':
    main()

#endregion
