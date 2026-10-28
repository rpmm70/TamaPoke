# TamaPoke

[![Flash in browser](https://img.shields.io/badge/flash-in%20browser-FF6B00?logo=googlechrome&logoColor=white)](https://socquique.github.io/TamaPoke/web/)
[![MakerWorld](https://img.shields.io/badge/MakerWorld-3D%20case-00AE42?logo=bambulab&logoColor=white)](https://makerworld.com/es/models/2937822-tamapoke-a-pokemon-pokeball-tamagotchi)
![Board](https://img.shields.io/badge/board-ESP32--S3%20round%20AMOLED-E7352C?logo=espressif&logoColor=white)
![Board](https://img.shields.io/badge/board-CYD%202.8%22%20ESP32--2432S028R-F5C400?logo=espressif&logoColor=white)
![Firmware](https://img.shields.io/badge/firmware-v1.24-8A2BE2)
[![CI](https://github.com/socquique/TamaPoke/actions/workflows/ci.yml/badge.svg)](https://github.com/socquique/TamaPoke/actions/workflows/ci.yml)
![Code](https://img.shields.io/badge/code-MIT-blue)
![Languages](https://img.shields.io/badge/languages-8-FFCB05)
[![Stars](https://img.shields.io/github/stars/socquique/TamaPoke?style=flat&logo=github&color=yellow)](https://github.com/socquique/TamaPoke/stargazers)

A gen-1-Pokémon-inspired tamagotchi. Raise any of the 151, evolve it, train it,
battle and catch wild ones, and complete them all (shinies included).

It runs on **two boards**, chosen at compile time:

| | **Waveshare ESP32-S3-Touch-AMOLED-1.75** | **ESP32-2432S028R "Cheap Yellow Display" (CYD)** |
|---|---|---|
| Screen | round 466×466 AMOLED (CO5300, QSPI) | rectangular 320×240 TFT (ILI9341, SPI) |
| Touch | capacitive (CST9217, I2C) | resistive (XPT2046) |
| Extras | battery + PMU, RTC, ES8311 audio, WiFi | speaker (PWM tones), BOOT button |
| Case | 3D-printed Pokéball ([MakerWorld](https://makerworld.com/es/models/2937822-tamapoke-a-pokemon-pokeball-tamagotchi)) | none (bare board) |
| Install | [web installer](https://socquique.github.io/TamaPoke/web/) or from source | from source (PlatformIO / Arduino IDE) |

> **Personal, non-commercial fan project.** Code is MIT; the sprites are from
> PMD SpriteCollab (CC BY-NC, Pokémon © Nintendo/Game Freak), and the 3D case is
> CC BY-NC-SA. See **[License](#license)** and **Credits**.

🔴 **3D-printed Pokéball case + print profiles → [on MakerWorld](https://makerworld.com/es/models/2937822-tamapoke-a-pokemon-pokeball-tamagotchi)** · flash it in your browser → **[web installer](https://socquique.github.io/TamaPoke/web/)**

## Status

Running on hardware, on both boards. Implemented: the 151 + shinies animated
from microSD, full life cycle (egg by rarity → evolution → farewell/release/runaway,
each gated behind a decision dialog), Pokédex gallery with filters (raised / caught /
shiny), battle stats (genes + training), **turn-based wild battles with catching**,
**random wild encounters** by time of day, retention hooks (streak / bond / medals /
name), biome + real-time backgrounds, three minigames (ball game, coin chase,
training bag), animated bath, a **settings menu** (screen off, sound, brightness,
date and time with 12h/24h, language, WiFi), RTC with offline progression,
battery (AXP2101) and PWR button, anti-burn-in dimming, **sound**, **8 UI languages
(English default)**, **starter choice on first run**, and a one-click **web installer**.

Pending: soak test, and a use for the WiFi connection (it connects, nothing in the
game uses it yet). See **Roadmap**.

## Game manual (the actual numbers)

A quick reference to how the game really works (values straight from the code).

### Time & leveling
- **1 real minute = 1 in-game minute.** Your Pokémon gains **+1 level every hour**
  of real time. Leveling is purely time-based - caring well doesn't speed it up,
  but neglect *delays evolution*.
- It keeps **aging while powered off** (the RTC runs), catching up to **2 weeks** max.
  On the CYD there is no RTC chip: the clock survives a reboot, but the time spent
  unpowered is not counted.

### The four stats (0–100)
Needs: **FOOD**, **JOY**, **ENE** (energy), **HYG** (hygiene). Start 80 / 80 / 80 / 100.
While **awake**, per minute:

| Stat | Drain/min | Notes |
|---|---|---|
| FOOD | −2 | |
| ENE | −1 | −1 extra if overweight (weight > 50 → sluggish) |
| HYG | −1 | **−4 more per poop** on screen (max 3 poops) |
| JOY | −1 | **−2 extra** if FOOD < 30, **−2 extra** if HYG < 30 |

- ~**15 %/min** chance to poop (only if FOOD > 40). Poops tank hygiene fast.
- **Care slip-up** = letting any stat hit **≤ 10** (30-min cooldown so it counts once).
  Each slip-up **delays evolution by 1 level** and cools the bond.

### Actions
- 🍎 **Apple / hamburger / pizza**: +25 FOOD. Each species has a **hidden favorite food**
  → +35 FOOD, +10 JOY, ♥, bond, and it gets revealed on the stat card.
- 🍬 **Candy:** +10 FOOD, +12 JOY, but **+12 weight** (fattening).
- ☕ **Coffee:** +25 ENE, but −10 FOOD (spoils the appetite). Never a favorite food.
- ⚽ **Ball game:** keep the ball in the air, 3 lives. Trains **ATK** (+1 per 5 points,
  max +2 per game), +JOY, −ENE, burns weight.
- 🪙 **Coin chase:** tap where the creature should walk and grab coins against the
  clock (20 s, +2 s per coin, 15 coins wins). Trains **SPD** (+1 at 5 coins, +2 at 10),
  +JOY, −ENE, burns weight. Walking speed grows with SPD.
- 🥊 **Training bag:** mash for 10 s. Trains **ATK** (~4 hits = 1 pt, cap +18/session),
  tires it, burns weight.
- ⚔️ **Wild battle:** see **Wild battles & catching** below.
- 🫧 **Bath:** clears poops, HYG → 100.
- 👆 **Pet it:** +5 JOY + bond.
- 🌙 **Sleep:** rest - ENE **+6/min**, needs drain ~**4× slower** with floors
  (FOOD 30 / JOY 35 / HYG 45). No poops, no slip-ups, can't run away while asleep.

### Eggs & who you get (spawn odds)
- **First ever pet:** you set the date and time, then pick a starter -
  **Bulbasaur / Charmander / Squirtle**.
- Hatch the egg: tap it **3×** (or wait - it hatches on its own).
- Every later egg rolls a **rarity tier** (over the ~79 base forms that come from eggs):

| Tier | Base chance | After a proper goodbye | # species |
|---|---|---|---|
| ✨ Legendary | ~3 %\* | ~10 % | 5 |
| 🔵 Rare | ~27 % | ~45 % | 27 |
| ⚪ Common | the rest | the rest | 47 |

  \* Legendaries only start appearing once you've **registered ≥ 25** Pokémon.
- A daily **streak** and high **bond** push rare/legendary odds higher.
- A clean **goodbye blesses** the next egg; a **run-away curses** it (forces Common).
- Within a tier it favors species whose **evolution line you haven't finished** (so
  all 151 are completable).
- **Shiny:** base **1 / 48** (→ **1 / 24** right after a goodbye), improved by
  streak/bond down to a best of **1 / 8**. Tracked separately in the dex.
- Every hatch rolls unique **genes** (90–110 % per stat) - no two are identical.

### Evolution
- Triggers when **level ≥ its evolution level** (16 for most base forms; ~30 for
  stone-style, ~40 for trade-style) **and every stat ≥ 40** at that moment.
- **Never automatic** - a button appears and **you tap to witness it** (with a
  flicker between the old and new form). Each **slip-up delays it by 1 level**.
- You can **decline** ("keep form"); it re-offers at the next level.
- *Eevee* branches toward whichever evolution you're still missing.

### The three endings (you choose & witness each - none auto-fire)
- 💛 **Farewell** - when it's a **final form** that has lived **3 days**. A button
  appears; triggering it **blesses your next egg**. You can **postpone** ("stay
  together", re-offered in a day). The good ending.
- 💔 **Run-away** - if you let **all four stats sit at 0 for a full hour**. A single
  act of care cancels it. It **curses the next egg** (forces Common). The sad ending.
- 👋 **Release** - long-press the creature to let it go on your terms (neutral).

After any ending, a **new egg** appears.

### Bonds, streaks, medals, Pokédex
- **Streak** (player-wide, survives across pets): first care each real day; milestones
  at **3 / 7 / 30 / 100** days; skipping a day breaks it.
- **Bond** (per pet, resets on hatch): grows with affection (**cap +8/day**), cools on
  neglect. Both streak & bond improve egg/shiny odds.
- **8 medals** (Lv10/25/50, favorite food found, 7-day streak, max bond, final form,
  "fit" = weight 0 & no slip-ups), per-pet + a global counter.
- **Pokédex:** raising a species registers it; **151 + shinies** to complete. Wild
  catches are tracked separately (their own filter in the gallery); a shiny catch also
  counts for the shiny registry.

### Battle stats
ATK / DEF / SPD = real **Gen-1 base** × genes + level + training:
- **ATK** ← training bag, ball game, and wins against defensive wild species
- **DEF** ← 12 h of unbroken good care, and wins against hard hitters
- **SPD** ← coin chase, and wins against fast species

Each training stat caps at 100. Max HP for battles comes from level and DEF.

### Wild battles & catching
- **Encounters:** on the idle main screen (awake, not an egg, no menu open), once a
  20-minute cooldown is over, the game rolls once a minute: **8 %** by day, **7 %** in
  the morning, **4 %** at night (ghost and poison species keep 8 %). A "Wild battle?"
  prompt shows for 20 s: **FIGHT** or **Later**. You can also start one any time from the
  ⚔️ in the play picker.
- **Who shows up:** never legendaries, never evolution-only forms; 25 % rare, the rest
  common. The time of day favors types: morning grass / flying / normal / bug, evening
  water / flying / fire, night ghost / poison / bug. Wild level is your level −4 to +3.
  Wild shinies are a fixed **1 / 4096**.
- **Your turn:** **Attack** opens *Quick* (85 % power, harder to dodge, you take 15 %
  less), *Normal* or *Heavy* (125 % power, easier to dodge, you take 20 % more).
  **Dodge** (85 % success, 70 % vs a quick attack, 95 % vs a heavy one) primes a
  **counter** (next hit ×1.5); a failed dodge still cuts the hit to a third.
  **Rest** heals 28 % of max HP and softens the hit that turn (2 uses per battle).
- The wild one picks attack / quick / heavy / dodge by the state of the fight and never
  chains dodges. No hit can take more than 35 % of max HP. The full modern type chart
  applies, softened: ×1.2 per weakness, ×0.85 per resistance, ×0.7 for an immunity.
  Battles last until one side runs out of HP.
- **Win:** a training reward (+1, +2 for a rare, +1 more for a close win), JOY, bond,
  and a **Catch** chance: common 55 %, rare 28 % (max 60 %), legendary 0, adjusted by the
  level gap, a close win and bond, clamped to 10–75 %. Battles started from the play
  picker give 70 % of that.
- **Loss:** JOY and ENE drop and the win streak resets. If the wild one was down to
  30 % HP or less, it may still let you try a **respect catch** (5–25 %).
- Wins, losses and the win streak persist across pets; the Stats page shows the record.

## Hardware

### Board 1: Waveshare ESP32-S3-Touch-AMOLED-1.75 (round)

- Board: [ESP32-S3-Touch-AMOLED-1.75](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.75)
  - get the **Standard** (no case) or **-G** (GPS, also fits) version; **not the "-B"**
  (ships with a protective case that won't fit). The separate "1.75**C**" is a different board.
  - **Where to buy**: [Waveshare store](https://www.waveshare.com/esp32-s3-touch-amoled-1.75.htm) (often on backorder), or the **-G** on Amazon - TamaPoke leaves the GPS unused, the board is otherwise identical: 🇺🇸 [.com](https://www.amazon.com/dp/B0F7XWWMJW?tag=capsuleradar-20) · 🇪🇸 [.es](https://www.amazon.es/dp/B0F7XWWMJW?tag=capsuleradar-21) · 🇩🇪 [.de](https://www.amazon.de/dp/B0F7XWWMJW?tag=capsulerada03-21) · 🇬🇧 [.co.uk](https://www.amazon.co.uk/dp/B0F7XWWMJW?tag=capsulerada0d-21) · 🇮🇹 [.it](https://www.amazon.it/dp/B0F7XWWMJW?tag=capsulerada08-21) · 🇫🇷 [.fr](https://www.amazon.fr/dp/B0F7XWWMJW?tag=capsulerada0e-21) · 🇯🇵 [.co.jp](https://www.amazon.co.jp/dp/B0F7XWWMJW?tag=capsuleradar-22) · 🇨🇦 [.ca](https://www.amazon.ca/dp/B0F7XWWMJW?tag=capsulerada09-20). Optional battery - 1100 mAh protected LiPo, MX1.25 plug: 🇪🇸 [.es](https://www.amazon.es/dp/B0F1FGZQS5?tag=capsuleradar-21) · 🇺🇸 [.com](https://www.amazon.com/dp/B0F1FGZQS5?tag=capsuleradar-20) · 🇮🇹 [.it](https://www.amazon.it/dp/B0F1FGZQS5?tag=capsulerada08-21) · 🇫🇷 [.fr](https://www.amazon.fr/dp/B0F1FGZQS5?tag=capsulerada0e-21) (DE/UK: that exact pack is not listed - search for a *protected* 3.7 V LiPo, ~1100 mAh, size **102540**, 1.25 mm micro-JST plug) - check plug polarity against the board first. <sub>Amazon links are affiliate links; as an Amazon Associate I earn from qualifying purchases.</sub>
  - **MicroSD card** (holds the sprite set - any small, class-10 card works; both boards need one): 🇪🇸 [.es](https://www.amazon.es/s?k=microsd+32gb&tag=capsuleradar-21) · 🇺🇸 [.com](https://www.amazon.com/s?k=microsd+32gb&tag=capsuleradar-20) · 🇩🇪 [.de](https://www.amazon.de/s?k=microsd+32gb&tag=capsulerada03-21) · 🇬🇧 [.co.uk](https://www.amazon.co.uk/s?k=microsd+32gb&tag=capsulerada0d-21) · 🇮🇹 [.it](https://www.amazon.it/s?k=microsd+32gb&tag=capsulerada08-21) · 🇫🇷 [.fr](https://www.amazon.fr/s?k=microsd+32gb&tag=capsulerada0e-21) · 🇯🇵 [.co.jp](https://www.amazon.co.jp/s?k=microsd+32gb&tag=capsuleradar-22) · 🇨🇦 [.ca](https://www.amazon.ca/s?k=microsd+32gb&tag=capsulerada09-20)
- Round 466×466 AMOLED, **CO5300** driver (QSPI, 80 MHz), framebuffer in the 8 MB OPI PSRAM
- Capacitive touch **CST9217** (I2C, address 0x5A)
- **AXP2101** (power management + battery + PWR button), **PCF85063** (RTC),
  microSD slot (SD_MMC), **ES8311** audio codec (→ amplifier → external speaker on the
  MX1.25 connector), 2.4 GHz WiFi
- Pins taken from the [official Waveshare repo](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75) (see `boards/board_waveshare_round.h`)

### Board 2: ESP32-2432S028R "Cheap Yellow Display" (rectangular)

The 2.8" CYD sold by ELEGOO and many others as "ESP32 2.8 Inch Touch Screen Display".
Tested on a real ELEGOO unit: display, touch, SD sprites, speaker and BOOT button all work.

- Classic **ESP32**, 4 MB flash, **no PSRAM**
- **ILI9341** 320×240 TFT over SPI (HSPI, 40 MHz), PWM backlight on GPIO 21
- **XPT2046** resistive touch, bit-banged on its own pins (a hardware SPI driver would
  fight the SD card for the bus); calibration in `boards/board_cyd_rect.h`
- microSD over SPI (VSPI pins 18/19/23, CS 5)
- Speaker on GPIO 26: PWM tones with the same note tables as the Waveshare's codec
- **BOOT button** (GPIO 0) is the power key: short press = screen on/off
- No PMU, no battery, no RTC chip: a soft clock kept in NVS (`rtcbat_cyd.cpp`)
- RGB LED and LDR are on the board but unused
- Pins from the community [CYD pinout](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display);
  other clones may differ (check whether yours has PSRAM)

What is different on the CYD, and why:

| | Waveshare (round) | CYD (rect) |
|---|---|---|
| Framebuffer | one canvas in PSRAM | 150 KB split into 32-row bands (`band_canvas.h`): the heap has room but no single block that big |
| Sprites | loaded whole into PSRAM | streamed from the SD one frame at a time (a short pass when a species loads) |
| Layout | `boards/layout_round_466.h` | `boards/layout_rect_320x240.h`: every menu, dialog and hit box redesigned for 320×240 |
| Clock | PCF85063, counts time while off | NVS epoch + `millis()`, saved at most every 10 min; unpowered time is lost |
| Power key | PWR: short = screen, long (4 s) = power-off | BOOT: short = screen on/off; touch is ignored while the screen is off |
| WiFi | yes | compiled out: the stack does not fit next to the framebuffer without PSRAM |
| Flash | 16 MB, `app3M_fat9M_16MB` | 4 MB, `huge_app` (3 MB app, no OTA) |

More detail, including how to recalibrate the touch, in [`CYD_PORT_NOTES.md`](CYD_PORT_NOTES.md).

## Libraries (Arduino IDE / arduino-cli)

| Library | Author | Board | Use |
|---|---|---|---|
| GFX Library for Arduino (`Arduino_GFX`) | moononournation | both | CO5300 over QSPI + framebuffer in PSRAM; ILI9341 over SPI |
| U8g2 | olikraus | both | CJK glyphs (`unifont_t_japanese3`, the only Japanese subset that also carries `！？。、「」`; `unifont_t_korean2` for Hangul, 2350 syllables against korean1's 478); only the font data is used, not its display driver |
| SensorLib | Lewis He | Waveshare | CST9217 touch + PCF85063 RTC |
| XPowersLib | Lewis He | Waveshare | AXP2101 PMU (battery, brightness, PWR button) |
| ESP_I2S (bundled in the ESP32 core) | Espressif | Waveshare | I2S to the ES8311 codec |

Requires ESP32 Arduino core **3.x**. `platformio.ini` pins the exact versions.

## IDE setup / build

The board is chosen at compile time with exactly one define (see
`boards/board_select.h`; building with none, or both, stops with `#error`):

| Define | Board | PlatformIO env |
|---|---|---|
| `BOARD_WAVESHARE_ROUND` | Waveshare ESP32-S3-Touch-AMOLED-1.75 (round 466×466) | `waveshare-round` |
| `BOARD_CYD_RECT` | ESP32-2432S028R / ELEGOO 2.8" (320×240) | `cyd-rect` (`cyd-rect-touchdebug` also prints raw touch readings) |

Each board's pins, capability flags (`BOARD_HAS_PMU`, `BOARD_HAS_WIFI`, ...),
display settings and touch calibration live in `boards/board_waveshare_round.h`
and `boards/board_cyd_rect.h`; the screen layout tables in `boards/layout*.h`.

### PlatformIO (VS Code or CLI)

`platformio.ini` pins the pioarduino platform (ESP32 Arduino core 3.3.12) and
the library versions; CI builds both envs with it. The default env is `cyd-rect`,
so always pass `-e` for the Waveshare.

```bash
pio run -e waveshare-round                 # build
pio run -e waveshare-round -t upload       # build and flash
pio device monitor -e waveshare-round      # serial, 115200, with exception decoder
pio run -e cyd-rect -t upload              # same for the CYD
pio run -e cyd-rect-touchdebug -t upload   # CYD with raw touch readings on serial
```

In VS Code (pioarduino extension), the tasks in `.vscode/tasks.json` ask for
the env: build, upload, upload + monitor, clean, and run the tests.

### Arduino IDE / arduino-cli

Uncomment one `#define` at the top of `boards/board_select.h`
(`BOARD_WAVESHARE_ROUND` or `BOARD_CYD_RECT`), or pass it as a build property.

Waveshare settings:

- Board: **ESP32S3 Dev Module** · Flash **16MB** · PSRAM **OPI PSRAM**
  (required: the 466×466×16-bit framebuffer ≈ 434 KB lives in PSRAM) ·
  Partition Scheme with FAT (e.g. `16M Flash (3MB APP/9MB FATFS)`) ·
  USB CDC On Boot **Enabled**

```bash
FQBN="esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB"
arduino-cli compile --fqbn "$FQBN" --build-property "build.extra_flags=-DBOARD_WAVESHARE_ROUND" .
arduino-cli upload -p /dev/cu.usbmodemXXXX --fqbn "$FQBN" .
```

CYD settings: board **ESP32 Dev Module** · Partition Scheme **Huge APP (3MB No OTA)**.
Only GFX Library for Arduino and U8g2 are needed.

### Easiest install: the web installer (Waveshare)

`web/index.html` flashes the firmware (ESP Web Tools) and pushes the sprites to
the SD over Web Serial, no Arduino needed. Serve it over HTTPS or `localhost`
(secure context) and open it in **Chrome/Edge**. See [`web/README.md`](web/README.md).
The installer's firmware is the ESP32-S3 (Waveshare) build only; the CYD is built
and flashed from source as above.

### Generate and load the sprites yourself

All sprites come from **[PMD SpriteCollab](https://github.com/PMDCollab/SpriteCollab)**
(CC BY-NC). You can regenerate the whole set and load it onto your board with the
pipeline below - the firmware accepts files over USB (PUT protocol with per-block
ACK) on both boards, so you don't have to remove the card (it formats the SD to FAT
if needed).

```bash
python3 tools/pack_pmd.py       # fetch + pack PMD sprites: the 151 + shiny -> tools/sdcard/mons/p[s]NNN.bin
python3 tools/make_thumbs.py    # Pokédex thumbnails (from the PMD sprites) -> thumbs.bin
python3 tools/send_sd.py        # send tools/sdcard/mons/* to the board's SD over USB
```

To make the **one-click web-installer bundle** instead of sending over USB:

```bash
python3 tools/pack_bundle.py    # bundle tools/sdcard/mons/* into web/sprites.pak
```

Then load it from the web installer's **"Load sprites"** button (or `send_sd.py`
above). `pack_pmd.py` also takes individual dex numbers, e.g. `pack_pmd.py 7 25`.
(~40 MB total, all PMD. Versioned under `tools/sdcard/`.) The files live in
`/mons/` on the card.

## How to play

On first run you **set the date and time**, then **choose a starter** (Bulbasaur /
Charmander / Squirtle). After that you start with an **egg**. Tap it 3 times or
wait and it hatches. From then on, care for your companion:

**Four stats** that decay: **FOOD**, **JOY**, **ENE** (energy), **HYG** (hygiene).
If one bottoms out it counts as a *slip-up*.

**Header:** the creature's name and level, the clock (12h or 24h), the battery, the
streak flame, the ☰ **settings** button (top left) and the **Pokédex** button under it.

**Buttons (bottom arc on the round board, a straight row on the CYD):**
- 🍴 **Feed** → food picker: apple, hamburger and pizza (each species has a hidden
  favourite that gives a bonus), a candy (+happiness but it fattens; weight makes it
  sluggish) and a coffee (+energy, -food).
- ⚽ **Play** → picker: ball game, coin chase, 🥊 training bag or ⚔️ a wild battle.
- 🌙 **Light** → sleep/wake (recovers energy, dims the screen). While asleep,
  needs decay much slower (rest).
- 🛁 **Bath** → a foam scene that cleans up the poops.

**Touch:**
- Tap the creature = pet it (+happiness, bond).
- Tap the creature's name (or the level line under it) = open the **stat card**
  (4 pages: Profile / Stats / Progress / Medals; switch with the side arrows or a
  horizontal swipe; tap the pencil next to the name on Profile to rename; the X in
  the top-right corner closes it).
- Tap the **Pokédex** button = open the **gallery**.
- Tap ☰ = open the **settings menu**.
- Long press (3 s) on the creature = **release** dialog.

**Physical button:**
- Waveshare **PWR**: short = screen on/off · long (4 s) = full power-off
  (the RTC stays alive, so time passes even while it's off).
- CYD **BOOT**: short = screen on/off. With the screen off, touches are ignored
  (the resistive panel can read stray presses); only BOOT turns it back on.

## Settings menu

Opened with ☰ on the main screen; the X is the only way out of each screen.

- **WiFi** (Waveshare only) and **Screen Off** share the first row. Screen Off goes
  back to the main screen with the panel dark.
- **Audio / Video:** sound on/off and a 0–10 brightness bar with -/+ buttons (saved
  when the screen closes).
- **Date / Time:** the date as `YYYY-MM-DD` and the time as `HH:MM`. Tap a field (year,
  month, day, hour or minute) to select it, then step it with -/+. A **12h/24h** toggle
  (also used by the header clock) and an **AM/PM** toggle in 12h mode. OK applies, X
  leaves without changing the time. The local time is used as-is (no time zone);
  moving the date moves the day the streak counts from.
- **Language:** the 8 languages in a grid; the active one is filled.
- The firmware version is shown at the bottom of the menu.

### WiFi (Waveshare)

On/Off switch (remembered), **Scan** for networks (strongest first, paged), each row
showing signal bars, SSID, band, security and dBm. Tap a network to connect: open
networks connect right away, others ask for the password on the on-screen keyboard.
The network is saved only once the connection succeeds, and reconnects on boot. The
password is stored in NVS encrypted with AES-256-GCM (key derived from the chip's
MAC). WPA-Enterprise networks are not supported. The game does not use the
connection for anything yet. On the CYD, WiFi is compiled out.

## Decisions: you choose, and you watch

The three life-cycle endings and evolution **don't happen on their own** - when
the conditions are met a button appears and you tap it (so you're present to
witness it), each opening a two-option dialog:

- **Evolution** (red button): *Evolve* (epic animation: halo, rays, sparkles and
  a **flicker between the old and new form**) or *Keep form* (re-offered next level).
- **Farewell** (gold button, final form + 3 days): *Say goodbye* (warm farewell,
  rising hearts → new egg) or *Stay together* (keep your companion; re-offered in
  a day). Tension: a maxed-out friend vs. completing the Pokédex.
- **Runaway** (dark button, total neglect for 1 h): a somber "feels abandoned"
  ending in the rain - caring for the creature cancels it.

## Sprites: PMD SpriteCollab everywhere

- **PMD SpriteCollab** (everything - main screen, stat card, minigames, battles **and
  the Pokédex grid + detail view**): behaviour sprites - `tools/pack_pmd.py` packs
  actions (Idle, Walk L/R, Sleep, Eat, Hurt, Attack, Pose, Nod, DeepBreath) into
  the multi-action **TPK2** format (`/mons/pNNN.bin`). The engine makes the creature
  wander, gesture, curl up to sleep, chew and wince, and lunge with its Attack pose in
  battle. Anchored by the feet (lowest content row), not the canvas. The Pokédex
  thumbnails (`thumbs.bin`, TPTH) are derived from these by `tools/make_thumbs.py`.
- **In-house workshop** (`tools/sprites.py`): 9 primitive-drawn sprites as a
  no-SD fallback + the UI icons. Generates `species.h`. Preview in
  `tools/sheet.png`, emit with `python3 tools/sprites.py emit`.

`sdmon.h/.cpp` loads the PMD sprites (`PmdMon` for TPK2) plus the thumbnails
(`SdThumbs`): whole into PSRAM on the Waveshare, streamed from the SD on boards
without PSRAM (decided at runtime). `SdMon` (TPK1) remains as a dormant legacy
fallback only.

## Pokédex and species data

`tools/dex_data.py` is the **single source**: name, slug, type (accent colour +
background biome), evolution line with gen-1 levels, rarities and starters.
`tools/dex_stats.py` has the real base stats (from PokéAPI). `tools/gen_names.py`
pulls the **official localized names** from PokéAPI into `tools/dex_names.py`
(only French and German differ in gen 1; Spanish, Italian and Portuguese use the
English ones). `gen_dex.py` emits `dex.h` (the `DEX_TBL[152]` table plus the
per-language name tables and the `dexName()` accessor). The pet's identity is its
Pokédex number (persisted in NVS).

- **Evolution** gen-1 style (levels 16/36/…; stones ≈30, trade ≈40; Eevee
  branches to whichever evolution you're missing). Each slip-up delays it 1
  level; it won't evolve with any stat < 40 or while asleep.

The **gallery** is a 4×4 grid of thumbnails with **All / Raised / Caught / Shiny**
filter buttons; swipe to change pages (swiping back from the first page closes it).
Tap a species for the detail view: the animated sprite, its type chips and whether
you caught it; the side arrows step through the filtered list.

## Battle stats and training

Each creature has ATK/DEF/SPD = real gen-1 base × **genes** (90–110 %, rolled at
hatch) + level + **training**:
- ATK ← the training bag (whacking), the ball game, wild wins
- DEF ← sustained good care (12 h with no slip-ups), wild wins
- SPD ← the coin chase, wild wins

Shown on the **Stats** page of the stat card, with the type chips, max battle HP, the
(hidden) weight, and the lifetime wild battle wins and losses. Weight goes up with
candy and burns off with the minigames.

## Retention: streak, bond, medals, name

- **Streak** (the player's, persists across creatures): the first care of each
  real day advances the streak; 3/7/30/100 milestones are celebrated; skipping a
  day breaks it. Flame badge on the main screen.
- **Bond** (the creature's): rises slowly with care and petting, drops with slip-ups.
- **Medals** for the individual (level, favorite food, streak, bond, final form, fit) +
  a global counter. Medals page of the stat card.
- **Name**: touch keyboard; the nickname rules the header and the card.

High streak and bond **improve the egg roll** (rarity and shiny): caring well
always pays off.

## Life cycle, eggs by rarity, languages

The life cycle lasts **3 days** of play. Three endings (all leave a new egg):
**farewell** (final form + 3 days), **release** (long press), **runaway** (all 4
bars at zero for 1 h). Each bred species is recorded in the **bred Pokédex**
(normal and shiny separately).

The egg rolls rarity over the ~79 base forms (47 common / 27 rare / 5 legendary),
**biased towards the lines you're missing** (all 151 are completable), blessed by
a farewell and punished by a runaway. Legendaries only with 25+ registered.
**Shiny** 1/48 (better with streak/bond/farewell).

**Languages:** the UI ships in 8 languages - English (default), Spanish, French,
German, Italian, Portuguese, Japanese and Korean - switchable from **Settings →
Language**.
**Pokémon names are localized too**: French, German, Japanese and Korean show
the official names (Bulbizarre, Bisasam, フシギダネ, 이상해씨...). Spanish, Italian
and Portuguese use the English ones, which is what those regions officially use
for gen 1. Nidoran♀/♂ read ニドランF/M in Japanese and 니드런 암/수 in Korean: no
U8g2 unifont subset carries ♀ or ♂, so each language picks its own substitute.

## Backgrounds: biome + real time

The idle screen paints the sky from the **real time** (dawn / day / dusk /
night with moon and stars; night is 20:00 to 05:59) and the ground from the
**type's biome** (meadow, beach, forest, volcano, mountain, snow).

Each activity has its own backdrop: a theater stage with red curtains for the ball
game, a fenced grass field for the coin chase, the inside of a boxing ring for the
training bag, and a top-down sand court for wild battles.

## Layout

- `TamaPoke.ino` - only `setup()` and `loop()`
- `app_state.h/.cpp` - shared game state (open screens, timers, minigame/battle state, `FW_VERSION`); `colors.h` - UI colours
- `board_display.h/.cpp` - display construction and init per board, brightness, screen clears; `band_canvas.h` - banded framebuffer for boards without PSRAM
- `ui_text`, `ui_widgets`, `ui_icons` - text helpers, shared widgets (bars, boxes, buttons, page navigation, type chips) and drawn icons (WiFi, pencil, check...)
- `main_screen` (render dispatcher), `scene` (sky, biomes and activity backdrops), `hud` (header, clock, bars, buttons), `touch_input` (gestures), `power_ui` (dimming, power key, idle saves), `serial_console`
- screens: `settings_screens` (menu, audio/video, date/time, language, WiFi), `keyboard` (names and WiFi passwords), `info_card`, `gallery`, `battle_ui` (wild prompt + battle), `minigame_common` / `minigame_ball` / `minigame_coin` / `minigame_bag`
- drawing: `pet_render`, `sprite_draw`, `ceremony_fx`, `bath`
- `pet.h` / `pet.cpp` / `pet_storage.cpp` - pet state and logic (stats, evolution, life cycle, streak/bond/medals, catching and battle rewards); `pet_storage.cpp` saves it to NVS as one versioned record
- `battle.h` / `battle.cpp` - turn battle engine (type chart, damage, wild picks, the wild one's moves); pure logic, tested on the host
- `wifi_manager.h` / `wifi_manager.cpp` - WiFi on/off, async scan, connection, encrypted saved password (stubs on boards without WiFi)
- `sdmon.h` / `sdmon.cpp` - TPK1 (animated) and TPK2 (PMD) sprites + thumbnails; `sd_upload.cpp` - file reception over USB (PUT/LS); `sd_host.h` - SD_MMC or SPI SD per board
- `rtcbat.h` / `rtcbat.cpp` - PCF85063 RTC + AXP2101 PMU (battery, brightness, PWR button)
- `audio_core.cpp` - Game-Boy-style tone synth and SFX queue (non-blocking task); `audio.cpp` (ES8311 + I2S) and `audio_cyd.cpp` (LEDC) are the per-board backends
- `i18n.h` / `i18n.cpp` - the 8-language string tables
- `dex.h` - GENERATED (`gen_dex.py`): the 151 table
- `species.h` - GENERATED (`sprites.py`): fallback sprites, UI icons, colours
- `boards/board_select.h` - picks the board header from the `BOARD_*` define
- `boards/board_waveshare_round.h` / `boards/board_cyd_rect.h` - pins, capability flags, display settings, touch calibration
- `boards/layout.h` (+ `layout_common.h`, `layout_round_466.h`, `layout_rect_320x240.h`) - per-screen layout tables
- `touch_hal_waveshare.cpp` / `touch_hal_cyd.cpp` - CST9217 / XPT2046 touch
- `rtcbat_cyd.cpp` - soft clock (NVS), battery stubs and BOOT power key for boards without RTC/PMU
- `test/` - host-side tests of the game logic (see **Tests**)
- `tools/` - pipeline: `dex_data.py` (data), `dex_stats.py`, `dex_names.py` +
  `gen_names.py` (localized names), `gen_dex.py`,
  `sprites.py` (workshop), `pack_pmd.py` / `make_thumbs.py`
  (packers), `pack_bundle.py` (web bundle), `send_sd.py` (SD upload), `touch_log.py`,
  `pio_gfx_filter.py` (compiles only the GFX drivers each env needs), `test_i18n_formats.py`
- `tools/sdcard/mons/` - the generated .bin files (animated, shiny, PMD, thumbnails)
- `web/` - the browser installer (ESP Web Tools + Web Serial sprite loader)

## Tests

`pet.cpp`, `battle.cpp`, `i18n.cpp` and `dex.h` compile and run on the PC against
Arduino shims (fake `millis()` / `random()`, in-memory NVS), no board needed:

```bash
./test/run_tests.sh          # or --asan for AddressSanitizer + UBSan
```

CI builds both board envs, runs these tests, checks that `dex.h` / `species.h` match
their generators, lints `tools/` with ruff and checks the i18n format specifiers.
See [`test/README.md`](test/README.md).

## Serial console (115200, debug)

`STATS` (full state) · `SPEC <dex>` (change species) · `LVL <n>` · `HATCH` ·
`SHINY` · `NICK <x>` · `BYE` / `RUN` (farewell / runaway) · `ABANDON` (force the
runaway-ready state) · `WIPE` (factory reset → new game) · `BEEP` (audio test) ·
`REG` (Pokédex) · `EGGS` (simulate 20 eggs) · `GAL` (gallery) · `CAREDAY` ·
`BATTLE` (random wild battle) / `BATTLE <dex>` (against that species) · `WILD`
(open the wild encounter prompt now) ·
`TIME <epoch>` / `RTCSET <epoch>` · `HEALTH` (uptime, heap and WiFi state for the
soak test) · `LS` / `PUT` (SD files).

To test fast: lower `PET_TICK_MS`, `MINUTES_PER_LEVEL` and `FAREWELL_AGE_MIN` in `pet.h`.

## Roadmap

- **Soak test** 24–48 h (instrumentation ready: `HEALTH` command/heartbeat).
- **A use for WiFi** (Waveshare): it connects and remembers the network; nothing in
  the game uses it yet.
- **CYD:** if animation is not smooth at SPI-SD speed on your unit, cache all frames
  of the current action; optional RTC + battery or I2S DAC add-ons only need
  `rtcbat_cyd.cpp` / `audio_cyd.cpp` replaced (see [`CYD_PORT_NOTES.md`](CYD_PORT_NOTES.md)).

*(Done: 3D-printed case [published on MakerWorld](https://makerworld.com/es/models/2937822-tamapoke-a-pokemon-pokeball-tamagotchi); repo public with the browser installer + one-click sprite bundle; wild encounters and turn-based battles with catching; second board (CYD); settings menu with date, brightness and WiFi.)*

## Community forks

- **[TamaPoke - Expanded](https://github.com/ShadowEnemyx/TamaPoke/tree/tamapoke-expanded-update)** by **ShadowEnemy** - a substantial community fork (different author/branch): a full **type-matchup battle system**, all **151 + shinies** with a **Pokédex / collection box** and daily goals, **6 UI languages**, **ES8311 sound**, starter choice and a one-click web installer. Worth a look. 🎮
- **[TamaPoke](https://github.com/DylanPDao/TamaPoke)** by **DylanPDao** - another substantial fork: **gym battles** and **LAN battles** between two devices, **movesets**, a **party + box** system, an **EV/IV** stat system, and coverage extended **up to Gen 3 (386)**. Keeps the PMD sprite pipeline. 🏆

## Credits

All sprites: [PMD SpriteCollab](https://github.com/PMDCollab/SpriteCollab)
(community, CC BY-NC). Base stats: [PokéAPI](https://pokeapi.co). Pokémon is a ™ of
Nintendo / Game Freak / The Pokémon Company. Non-commercial, personal-use project.
Full list in [`CREDITS.md`](CREDITS.md). Version history in
[`CHANGELOG.md`](CHANGELOG.md) - most of the fixes there came from people
who built one and reported what they found.

## License

- **Source code** (firmware + tooling): **[MIT](LICENSE)**.
- **Sprites & names**: © Nintendo / Game Freak / The Pokémon Company; pixel art
  from [PMD SpriteCollab](https://github.com/PMDCollab/SpriteCollab) (CC BY-NC 4.0).
  **Non-commercial use only.**
- **3D-printed case**: remix of *"Pokeball"* by **yoyothechicken**
  ([MakerWorld #839922](https://makerworld.com/es/models/839922-pokeball)),
  licensed **CC BY-NC-SA**, and shared here under the same terms.

This is an unofficial fan project, not affiliated with or endorsed by Nintendo.
