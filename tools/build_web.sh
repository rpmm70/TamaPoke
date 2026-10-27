#!/bin/bash
# Rebuilds web/firmware/tamapoke.bin (merged firmware image) for the web installer.
# Usage: bash tools/build_web.sh [env]      (env defaults to waveshare-round)
#   env = a PlatformIO env from platformio.ini: waveshare-round or cyd-rect
# Note: web/manifest.json declares the chip family; it targets the
# ESP32-S3 (waveshare-round) build.
set -e
cd "$(dirname "$0")/.."

#region Configuration per env
pio_env="${1:-waveshare-round}"
case "$pio_env" in
  waveshare-round) chip=esp32s3; bootloader_offset=0x0 ;;
  cyd-rect|cyd-rect-touchdebug) chip=esp32; bootloader_offset=0x1000 ;;
  *) echo "Unknown env '$pio_env' (expected waveshare-round or cyd-rect)"; exit 1 ;;
esac
build_output_dir=".pio/build/$pio_env"
pio_core_dir="${PLATFORMIO_CORE_DIR:-$HOME/.platformio}"
boot_app0_bin="$pio_core_dir/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin"
#endregion

#region Build
echo "Building $pio_env..."
pio run -e "$pio_env"
#endregion

#region Merge the binaries into one image
echo "Merging binaries..."
mkdir -p web/firmware
esptool.py --chip "$chip" merge-bin -o web/firmware/tamapoke.bin \
  "$bootloader_offset" "$build_output_dir/bootloader.bin" \
  0x8000  "$build_output_dir/partitions.bin" \
  0xe000  "$boot_app0_bin" \
  0x10000 "$build_output_dir/firmware.bin"

echo "OK -> web/firmware/tamapoke.bin ($(du -h web/firmware/tamapoke.bin | cut -f1))"
#endregion

#region Sprite bundle
echo "Packing sprites..."
python3 tools/pack_bundle.py
#endregion
