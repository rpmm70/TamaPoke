// Audio backend: square tones via PWM (LEDC) on a bare speaker pin
// (SPEAKER_PIN), for boards without a codec (the CYD). The note tables, queue
// and task are in audio_core.cpp, shared with the I2S backend.
//
// Requires ESP32 Arduino core 3.x (per-pin ledcAttach/ledcWriteTone API),
// the same requirement ESP_I2S.h already imposes on the I2S backend.

#pragma region Includes
#include "audio_backend.h"
#pragma endregion

#if BOARD_AUDIO_LEDC

#pragma region Backend includes
#include <Arduino.h>
#pragma endregion

#pragma region Configuration
#define LEDC_BASE_FREQUENCY_HZ 2000
#define LEDC_RESOLUTION_BITS 10
#pragma endregion

#pragma region State
static bool speakerAttached = false;
#pragma endregion

#pragma region Backend hooks
bool audioBackendBegin() {
  if (!ledcAttach(SPEAKER_PIN, LEDC_BASE_FREQUENCY_HZ, LEDC_RESOLUTION_BITS)) {
    Serial.println("LEDC audio failed (audio off)");
    return false;
  }
  ledcWriteTone(SPEAKER_PIN, 0);
  speakerAttached = true;
  return true;
}

// frequency 0 writes a 0 Hz tone, which is silence
void audioBackendPlayNote(uint16_t frequencyHz, uint16_t durationMs) {
  ledcWriteTone(SPEAKER_PIN, frequencyHz);
  vTaskDelay(pdMS_TO_TICKS(durationMs));
}

// silence when the effect ends
void audioBackendSilence() {
  ledcWriteTone(SPEAKER_PIN, 0);
}

// muted or asleep: stop any tone in progress
void audioBackendPowerChanged(bool active) {
  if (!active && speakerAttached) ledcWriteTone(SPEAKER_PIN, 0);
}
#pragma endregion

#endif  // BOARD_AUDIO_LEDC
