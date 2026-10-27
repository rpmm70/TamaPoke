// Audio backend: ES8311 codec (DAC -> NS4150B PA amplifier -> speaker) over
// I2S (Waveshare). The note tables, queue and task are in audio_core.cpp.
// ES8311 init ported from Espressif's official driver (esp-bsp): 16kHz, 16-bit,
// I2S slave. The notes are square tones synthesized into PCM.

#pragma region Includes
#include "audio_backend.h"
#pragma endregion

#if BOARD_AUDIO_I2S_ES8311

#pragma region Backend includes
#include <Arduino.h>
#include <Wire.h>
#include <ESP_I2S.h>
#pragma endregion

#pragma region Configuration
#define ES8311_ADDR 0x18
#define SAMPLE_RATE 16000
#define TONE_AMPLITUDE 5000
#define TONE_CHUNK_SAMPLES 256     // samples synthesized per I2S write
#define TONE_ATTACK_SAMPLES 64     // anti-click ramp at the start of a tone
#define TONE_RELEASE_SAMPLES 96    // anti-click ramp at the end of a tone
#pragma endregion

#pragma region State
static I2SClass i2s;
static int16_t toneBuffer[TONE_CHUNK_SAMPLES * 2];  // interleaved stereo (L=R)
#pragma endregion

#pragma region Codec I2C
static bool writeCodecRegister(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

static uint8_t readCodecRegister(uint8_t reg) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(ES8311_ADDR, 1);
  return Wire.available() ? Wire.read() : 0;
}

// Init sequence VERIFIED ON THIS BOARD (PlaneRadar2.0 project, same
// Waveshare 1.75). Key points: clock DERIVED FROM BCLK (reg01=0xBF, no external
// MCLK) and internal reference feeding the DAC (reg44=0x58); without those two
// the codec answered on I2C but no audio came out. 16kHz, 16-bit, I2S slave.
static bool initEs8311Codec() {
  Wire.beginTransmission(ES8311_ADDR);
  if (Wire.endTransmission() != 0) return false;

  #pragma region open()
  writeCodecRegister(0x0D, 0xFA); writeCodecRegister(0x44, 0x08); writeCodecRegister(0x44, 0x08);  // power up + first-write quirk
  writeCodecRegister(0x01, 0x30); writeCodecRegister(0x02, 0x00); writeCodecRegister(0x03, 0x10); writeCodecRegister(0x16, 0x24);
  writeCodecRegister(0x04, 0x10); writeCodecRegister(0x05, 0x00); writeCodecRegister(0x0B, 0x00); writeCodecRegister(0x0C, 0x00);
  writeCodecRegister(0x10, 0x1F); writeCodecRegister(0x11, 0x7F);
  writeCodecRegister(0x00, 0x80); writeCodecRegister(0x00, 0x80);  // reset clock, slave
  writeCodecRegister(0x01, 0xBF);                                  // clk src = BCLK (no external MCLK)
  { uint8_t registerValue = readCodecRegister(0x06); registerValue &= ~0x20; writeCodecRegister(0x06, registerValue); }  // SCLK not inverted
  writeCodecRegister(0x13, 0x10); writeCodecRegister(0x1B, 0x0A); writeCodecRegister(0x1C, 0x6A);
  writeCodecRegister(0x44, 0x58);                                  // internal reference -> feeds the DAC
  #pragma endregion

  #pragma region config_sample(): BCLK*8 = DIG_MCLK
  writeCodecRegister(0x02, 0x18); writeCodecRegister(0x05, 0x00); writeCodecRegister(0x03, 0x10); writeCodecRegister(0x04, 0x20);
  { uint8_t registerValue = readCodecRegister(0x07); registerValue &= 0xC0; writeCodecRegister(0x07, registerValue); }
  writeCodecRegister(0x08, 0xFF);
  { uint8_t registerValue = readCodecRegister(0x06); registerValue &= 0xE0; registerValue |= 0x03; writeCodecRegister(0x06, registerValue); }  // bclk_div=4
  #pragma endregion

  #pragma region 16-bit I2S format
  writeCodecRegister(0x09, 0x0C); writeCodecRegister(0x0A, 0x0C);
  #pragma endregion

  #pragma region start() slave DAC
  writeCodecRegister(0x00, 0x80); writeCodecRegister(0x01, 0xBF); writeCodecRegister(0x09, 0x0C); writeCodecRegister(0x0A, 0x0C);
  writeCodecRegister(0x17, 0xBF); writeCodecRegister(0x0E, 0x02); writeCodecRegister(0x12, 0x00); writeCodecRegister(0x14, 0x1A);
  writeCodecRegister(0x0D, 0x01); writeCodecRegister(0x15, 0x40); writeCodecRegister(0x37, 0x08); writeCodecRegister(0x45, 0x00);
  #pragma endregion

  #pragma region Volume + unmute
  writeCodecRegister(0x32, 0xBF);                                  // DAC volume ~0 dB
  { uint8_t registerValue = readCodecRegister(0x31); registerValue &= 0x9F; writeCodecRegister(0x31, registerValue); }  // unmute
  #pragma endregion
  return true;
}
#pragma endregion

#pragma region Backend hooks
bool audioBackendBegin() {
  // I2S first: starts the clock the codec needs to lock on
  pinMode(PA, OUTPUT);
  digitalWrite(PA, LOW);   // until we know whether sound is enabled

  i2s.setPins(I2S_BCK_IO, I2S_WS_IO, I2S_DO_IO, I2S_DI_IO, I2S_MCK_IO);
  if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT,
                 I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
    Serial.println("I2S begin failed");
    return false;
  }
  if (!initEs8311Codec()) {
    Serial.println("ES8311 not responding (audio off)");
    return false;
  }
  return true;
}

// plays a tone (or silence if frequencyHz==0) with an anti-click attack/release ramp
void audioBackendPlayNote(uint16_t frequencyHz, uint16_t durationMs) {
  int totalSamples = SAMPLE_RATE * durationMs / 1000;
  int halfPeriodSamples = frequencyHz ? (SAMPLE_RATE / (2 * frequencyHz)) : 0;
  int phaseSamples = 0, doneSamples = 0;
  bool high = true;
  while (doneSamples < totalSamples) {
    int chunkSamples = totalSamples - doneSamples;
    if (chunkSamples > TONE_CHUNK_SAMPLES) chunkSamples = TONE_CHUNK_SAMPLES;
    for (int i = 0; i < chunkSamples; i++) {
      int16_t sample = 0;
      if (frequencyHz) {
        sample = high ? TONE_AMPLITUDE : -TONE_AMPLITUDE;
        int sampleIndex = doneSamples + i;
        if (sampleIndex < TONE_ATTACK_SAMPLES)
          sample = (int16_t)(sample * sampleIndex / TONE_ATTACK_SAMPLES);
        else if (sampleIndex > totalSamples - TONE_RELEASE_SAMPLES)
          sample = (int16_t)(sample * (totalSamples - sampleIndex) / TONE_RELEASE_SAMPLES);
        if (++phaseSamples >= halfPeriodSamples) { phaseSamples = 0; high = !high; }
      }
      toneBuffer[i * 2] = sample;
      toneBuffer[i * 2 + 1] = sample;
    }
    i2s.write((uint8_t *)toneBuffer, chunkSamples * 4);
    doneSamples += chunkSamples;
  }
}

// nothing to do: the last note already ended on its release ramp
void audioBackendSilence() {}

// The NS4150B takes well over 8 ms to settle after each power-off, so turning
// it on right before each effect swallowed the short ones: the startup jingle
// (440 ms) was audible and the 35 ms beeps were not. It stays on while sound is
// enabled and the pet is awake, as in Waveshare's verified example, and is only
// turned off when sleeping or muted. "active" includes "codec started", so a
// board where the codec did not start never powers the amplifier (it would only
// add hiss).
void audioBackendPowerChanged(bool active) {
  digitalWrite(PA, active ? HIGH : LOW);
}
#pragma endregion

#endif  // BOARD_AUDIO_I2S_ES8311
