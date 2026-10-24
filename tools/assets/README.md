# TamaPoke assets - where the sprites come from

The sprites are NOT edited by hand or stored here: they are **downloaded from
their sources and packed** with the scripts in `tools/`. This folder is only
documentation of the workflow (it used to contain a PNG import proposal that
became obsolete).

## The actual workflow

Two formats live side by side on the microSD (`/mons/`), both derived from PMD SpriteCollab:

| Format | Script | Source | What it is |
|---|---|---|---|
| **TPK2** `pNNN.bin` / `psNNN.bin` | `pack_pmd.py` | [PMD SpriteCollab](https://github.com/PMDCollab/SpriteCollab) (CC BY-NC) | Multi-action animations (idle, walk, sleep, eat, hurt, attack, gestures) - used **everywhere**: main screen and **Pokédex / gallery** |
| **TPTH** `thumbs.bin` | `make_thumbs.py` | (derived from the TPK2) | 40×40 gallery thumbnails |

```bash
python3 tools/pack_pmd.py     # all 151 + shiny -> tools/sdcard/mons/p[s]NNN.bin
python3 tools/make_thumbs.py  # -> tools/sdcard/mons/thumbs.bin
python3 tools/send_sd.py      # sends everything to the board's SD over USB
```

(`s` = shiny variant. `pack_pmd.py` accepts individual Pokédex numbers,
e.g. `python3 tools/pack_pmd.py 7 25`.)

## Binary formats

Defined in the header of each packer and parsed in `sdmon.cpp`:

- **TPK1** (`SdMon`): `"TPK1"`, `u16 w,h,frames,frameMs`, `u16 palCount`,
  `u16 pal[]` (RGB565), `u8 data[frames*w*h]` (palette index, `0xFF`
  transparent).
- **TPK2** (`PmdMon`): `"TPK2"`, `u8 nActs`, `u16 palCount`, `u16 pal[]`, and per
  action `u8 id,w,h,nFrames` + `u16 ms[nFrames]` + `u8 data[w*h*nFrames]`.
- **TPTH** (`SdThumbs`): `"TPTH"`, `u16 count`, `u32 offset[count]`, and per
  thumbnail `u8 w,h,palCount` + `u16 pal[]` + `u8 data[w*h]`.

The firmware validates sizes on load, so a truncated `.bin` is rejected without
breaking anything.

## Download cache

`pack_pmd.py` caches the original SpriteCollab PNGs in `tools/pmd_cache/`
(ignored by git, regenerable). The final `.bin` files are versioned in
`tools/sdcard/mons/` as a backup.

> See [CREDITS.md](../../CREDITS.md) for the origin and terms of the
> sprites. They are third-party: do not redistribute for commercial purposes.
