# Tests

A test suite that runs **on the PC, without the board**: it compiles the pure
firmware logic (`pet.cpp`, `pet_storage.cpp`, `i18n.cpp`, `battle.cpp`, `dex.h`)
against some Arduino shims and runs it with a fake clock and a fake `random()`,
so the results are deterministic and take less than a second.

```bash
./test/run_tests.sh           # everything
./test/run_tests.sh --asan    # also with AddressSanitizer + UBSan
```

Or piece by piece:

```bash
cd test && make               # only the firmware logic
cd test && make FILTER=evolve # only the tests with "evolve" in the name
cd test && make asan          # with sanitizers
cd test && make clean
python3 test/test_tools.py    # only the tools in tools/
```

The Makefile builds one object per source with `-MMD -MP`, so editing a header
rebuilds everything that includes it. Any new `test_*.cpp` is picked up
automatically.

## What is covered

| File | What it checks |
|---|---|
| `test_pet.cpp` | egg and starter choice, food, stats tick, sleep, care mistakes, levels, evolution (including the Eevee branch), battle stats, training and minigames, streak and bond, medals, ceremonies, offline progression, saving to NVS, catching and wild battle rewards |
| `test_battle.cpp` | wild battles (`battle.cpp`): auto battle, turn battle moves (attack, quick, heavy, dodge, rest, counter), the wild one's moves, wild species and level picks, day phases, level cap and the type chart |
| `test_dex.cpp` | Pokedex integrity: name tables sized `DEX_COUNT + 1` (FR, DE, JA, KO), uppercase ASCII Latin names, evolution chains without loops, every species reachable, no line impossible to complete, UI buffer limits, battle types |
| `test_i18n.cpp` | all 8 languages (ES, EN, FR, DE, IT, PT, JA, KO) complete, Latin accents as single CP437 bytes (not UTF-8), the same `%u`/`%s` in every language and at each call of the sketch, medal and menu lengths |
| `test_tools.py` | the `tools/` scripts compile, the Pokedex data is consistent, `dex.h` still matches what `tools/gen_dex.py` generates, and the formatted text buffers fit in every language |

## How it is set up

- `shim/Arduino.h` + `shim/Preferences.h` + `shim/shim.cpp`: the bare minimum of
  Arduino that the firmware logic uses. The clock (`millis()`) and `random()`
  are fake and controllable; `Preferences` is an in-memory map that mimics the
  NVS (including `putBytes` / `getBytes` / `getBytesLength`), so a reboot can be
  simulated by creating another `Pet` and calling `begin()`. The audio
  functions are empty stubs.
- `framework.h` + `main.cpp`: our own micro framework, no dependencies.
- Tests marked with `TEST_KNOWN_ISSUE` describe the **correct** behavior of
  something that fails today: they do not break the suite, they are listed
  separately, and they warn if they ever start passing (so the mark can be removed).

## About fonts

The firmware's GFX font is a 256-glyph CP437 table: Latin accents are allowed,
but each one must be a single byte (written as an octal escape in `i18n.cpp`),
because the UI centers text with `strlen() * 6`. Japanese and Korean are UTF-8
on purpose and are drawn with U8g2 fonts, so their length checks are skipped
(bytes are not on-screen width there).

## What is NOT covered

Everything that needs the board: display and touch (`TamaPoke.ino`), audio
(`audio.cpp`), SD card (`sdmon.cpp`), WiFi and RTC/battery (`rtcbat.cpp`). The
`.ino` is not compiled here either: that needs PlatformIO or `arduino-cli` with
the ESP32 core.
