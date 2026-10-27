"""Binary layouts of the SD card sprite files (all little-endian).

TPK2  /mons/pNNN.bin, psNNN.bin  multi-action PMD sprite (tools/pack_pmd.py)
  char[4] "TPK2"
  u8  actionCount
  u16 paletteCount
  u16 palette[paletteCount]          (RGB565)
  per action:
    u8 id, u8 width, u8 height, u8 frameCount
    u16 frameMs[frameCount]
    u8 pixels[width*height*frameCount] (palette indices, 0xFF transparent)

TPTH  /mons/thumbs.bin  gallery thumbnails (tools/make_thumbs.py)
  char[4] "TPTH"
  u16 count
  u32 offset[count]                  (from the start of the file; offset[0] = dex 1)
  blobs: u8 width, u8 height, u8 paletteCount, u16 palette[paletteCount],
         u8 pixels[width*height]     (0xFF transparent)

TPAK  web/sprites.pak  bundle for the web installer (tools/pack_bundle.py)
  char[4] "TPAK"
  u16 count
  count x { u8 nameLength; char name[nameLength]; u32 size }   (index)
  ...the data of each file, in index order...
"""
#region Imports
import struct
#endregion

#region Constants
TRANSPARENT_INDEX = 0xFF
MAX_PALETTE_COLORS = 255          # index 0xFF is reserved for transparency

TPK2_MAGIC = b'TPK2'
TPK2_HEADER = struct.Struct('<BH')          # actionCount, paletteCount
TPK2_ACTION_HEADER = struct.Struct('<4B')   # id, width, height, frameCount
PMD_ACTION_IDLE = 0                         # front view, used for the thumbnails

TPTH_MAGIC = b'TPTH'
TPTH_COUNT = struct.Struct('<H')
TPTH_OFFSET = struct.Struct('<I')
TPTH_BLOB_HEADER = struct.Struct('<3B')     # width, height, paletteCount

TPAK_MAGIC = b'TPAK'
TPAK_COUNT = struct.Struct('<H')
TPAK_NAME_LENGTH = struct.Struct('<B')
TPAK_FILE_SIZE = struct.Struct('<I')
#endregion

#region TPK2

def build_tpk2_bytes(palette_rgb565, packed_actions):
    """packed_actions: list of (id, width, height, frame_count, frame_ms list, pixel bytes)."""
    parts = [TPK2_MAGIC, TPK2_HEADER.pack(len(packed_actions), len(palette_rgb565))]
    parts.append(struct.pack(f'<{len(palette_rgb565)}H', *palette_rgb565))
    for action_id, width, height, frame_count, frame_ms, pixel_data in packed_actions:
        parts.append(TPK2_ACTION_HEADER.pack(action_id, width, height, frame_count))
        parts.append(struct.pack(f'<{frame_count}H', *frame_ms))
        parts.append(pixel_data)
    return b''.join(parts)


def read_tpk2_first_frame(source_file, wanted_action_id=PMD_ACTION_IDLE):
    """(width, height, palette, pixels) of frame 0 of one action of a TPK2 file.

    Reads only the header, the palette, the action headers it has to skip and
    the one frame it returns: the other frames are seeked over.
    """
    #region Header and palette
    if source_file.read(4) != TPK2_MAGIC:
        raise ValueError('bad magic, expected TPK2')
    action_count, palette_count = TPK2_HEADER.unpack(source_file.read(TPK2_HEADER.size))
    palette = list(struct.unpack(f'<{palette_count}H', source_file.read(palette_count * 2)))
    #endregion

    #region Walk the actions
    for _ in range(action_count):
        action_id, width, height, frame_count = TPK2_ACTION_HEADER.unpack(
            source_file.read(TPK2_ACTION_HEADER.size))
        source_file.seek(frame_count * 2, 1)  # frameMs[]
        if action_id == wanted_action_id:
            return width, height, palette, source_file.read(width * height)
        source_file.seek(width * height * frame_count, 1)
    raise ValueError(f'no action with id {wanted_action_id}')
    #endregion

#endregion

#region TPTH

def build_tpth_blob(width, height, palette_rgb565, pixel_data):
    return (TPTH_BLOB_HEADER.pack(width, height, len(palette_rgb565))
            + struct.pack(f'<{len(palette_rgb565)}H', *palette_rgb565) + pixel_data)


def build_tpth_bytes(blobs):
    header_size = len(TPTH_MAGIC) + TPTH_COUNT.size + TPTH_OFFSET.size * len(blobs)
    offsets, position = [], header_size
    for blob in blobs:
        offsets.append(position)
        position += len(blob)
    return b''.join([TPTH_MAGIC, TPTH_COUNT.pack(len(blobs)),
                     struct.pack(f'<{len(blobs)}I', *offsets), *blobs])

#endregion

#region TPAK

def build_tpak_index(entries):
    """entries: list of (name inside the pack, size in bytes) -> the TPAK header + index."""
    parts = [TPAK_MAGIC, TPAK_COUNT.pack(len(entries))]
    for name, size in entries:
        encoded_name = name.encode()
        parts += [TPAK_NAME_LENGTH.pack(len(encoded_name)), encoded_name, TPAK_FILE_SIZE.pack(size)]
    return b''.join(parts)

#endregion
