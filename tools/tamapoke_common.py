"""Helpers shared by the TamaPoke workshop tools (tools/*.py).

Nothing here runs on import: every helper is a plain function, and the optional
third-party modules (Pillow, pyserial) are imported only by the helpers that
need them, so the generators keep working on a machine without them.
"""
#region Imports
import os
import time
import urllib.error
import urllib.request
#endregion

#region Directory paths
TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(TOOLS_DIR)
SDCARD_DIR = os.path.join(TOOLS_DIR, 'sdcard')
MONS_DIR = os.path.join(SDCARD_DIR, 'mons')
#endregion

#region Colors

def rgb_to_rgb565(red, green, blue):
    """8-bit-per-channel color -> RGB565 (what the displays take)."""
    return (red >> 3) << 11 | (green >> 2) << 5 | (blue >> 3)


def hex_to_rgb565(hex_color):
    """'#rrggbb' -> RGB565."""
    red = int(hex_color[1:3], 16)
    green = int(hex_color[3:5], 16)
    blue = int(hex_color[5:7], 16)
    return rgb_to_rgb565(red, green, blue)

#endregion

#region Files

def write_generated_text(path, text):
    """Writes a generated text file as UTF-8 with LF line endings on every OS."""
    with open(path, 'w', encoding='utf-8', newline='\n') as output_file:
        output_file.write(text)


def write_bytes_atomically(path, data):
    """Writes to a temporary file next to `path`, then renames it over `path`.

    An interrupted run never leaves a truncated file behind (the caches and
    the packed sprites are skipped when they already exist).
    """
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    temporary_path = f'{path}.part{os.getpid()}'
    with open(temporary_path, 'wb') as output_file:
        output_file.write(data)
    os.replace(temporary_path, path)

#endregion

#region Network

def download_with_retry(url, destination_path=None, attempt_count=4, timeout_seconds=30):
    """Downloads `url` and returns its bytes, or None if it could not be fetched.

    A 404 is final (the file does not exist upstream) and is not retried;
    other errors are retried with a growing pause. With `destination_path`
    the bytes are also saved there atomically (temporary file + rename).
    """
    request = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
    for attempt in range(attempt_count):
        try:
            with urllib.request.urlopen(request, timeout=timeout_seconds) as response:
                data = response.read()
        except urllib.error.HTTPError as http_error:
            if http_error.code == 404:
                return None
        except (urllib.error.URLError, OSError):
            pass
        else:
            if destination_path:
                write_bytes_atomically(destination_path, data)
            return data
        if attempt + 1 < attempt_count:
            time.sleep(1 + attempt)
    return None

#endregion

#region Serial port

# USB vendor ids of the boards' serial bridges: Espressif native USB,
# WCH CH340 (CYD), Silicon Labs CP210x, FTDI.
KNOWN_BOARD_USB_VENDOR_IDS = (0x303A, 0x1A86, 0x10C4, 0x0403)


def find_board_serial_port():
    """Name of the first serial port that looks like the board, or None."""
    from serial.tools import list_ports

    #region Prefer a known USB bridge, then any USB serial port
    available_ports = list(list_ports.comports())
    for port_info in available_ports:
        if port_info.vid in KNOWN_BOARD_USB_VENDOR_IDS:
            return port_info.device
    for port_info in available_ports:
        if port_info.vid is not None:
            return port_info.device
    return None
    #endregion


def open_board_serial(port=None, baud_rate=115200, timeout_seconds=1):
    """Opens the board's serial port. Returns (serial object, port name).

    Without `port` it is detected with serial.tools.list_ports, which works on
    Windows (COMx), macOS (/dev/cu.usbmodem*) and Linux (/dev/ttyACM*, ttyUSB*).
    Exits with a message if no board is found.
    """
    import serial

    port_name = port or find_board_serial_port()
    if not port_name:
        raise SystemExit('board not found: plug it in over USB or pass --port')
    return serial.Serial(port_name, baud_rate, timeout=timeout_seconds), port_name

#endregion

#region Contact sheets

def render_contact_sheet(tiles, output_path, tile_size, pad, label_height, label_offset,
                         group_gap, backgrounds=('#f2efe1', '#141828'), column_count=None):
    """Renders a PNG review sheet: the same grid once per background color.

    tiles: list of (row, column, label or None, paint), where
    paint(image, draw, left, top) draws the tile's content at that corner.
    Each tile gets its background square (tile_size x tile_size) and its label
    under it, `label_offset` px below the square. Rows are
    tile_size + pad + label_height tall, columns tile_size + pad wide, and
    each background's grid is followed by `group_gap` px. column_count defaults
    to the widest row actually used.
    """
    from PIL import Image, ImageDraw

    #region Grid geometry
    if column_count is None:
        column_count = max(column for _, column, _, _ in tiles) + 1
    row_count = max(row for row, _, _, _ in tiles) + 1
    column_width = tile_size + pad
    row_height = tile_size + pad + label_height
    group_width = column_count * column_width + group_gap
    image = Image.new('RGB', (len(backgrounds) * group_width + pad, row_count * row_height + pad),
                      '#888888')
    draw = ImageDraw.Draw(image)
    #endregion

    #region Paint every tile on every background
    for background_index, background_color in enumerate(backgrounds):
        group_left = pad + background_index * group_width
        for row, column, label, paint in tiles:
            left = group_left + column * column_width
            top = pad + row * row_height
            draw.rectangle([left, top, left + tile_size, top + tile_size], fill=background_color)
            paint(image, draw, left, top)
            if label:
                draw.text((left, top + tile_size + label_offset), label, fill='#000')
    #endregion

    image.save(output_path)
    print(f"saved {output_path}")

#endregion
