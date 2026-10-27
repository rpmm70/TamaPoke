#pragma once

// Hooks between the shared sound engine (audio_core.cpp: note tables, queue,
// task, NVS switch) and the board's sound hardware. Exactly one backend is
// compiled in, chosen by the board capability flags:
//   BOARD_AUDIO_I2S_ES8311  audio.cpp      ES8311 codec over I2S + NS4150B amplifier
//   BOARD_AUDIO_LEDC        audio_cyd.cpp  square tones on a speaker pin (LEDC PWM)
// To add a board with other sound hardware, write a new backend file with
// these four functions and the two settings below.

#pragma region Includes
#include <stdint.h>
#include "boards/board_select.h"
#pragma endregion

#pragma region Backend settings
#if BOARD_AUDIO_I2S_ES8311
  #define AUDIO_TASK_STACK_BYTES 4096
  // effects are still queued while asleep; the task discards them
  #define AUDIO_DROP_SFX_WHILE_SLEEPING 0
#elif BOARD_AUDIO_LEDC
  #define AUDIO_TASK_STACK_BYTES 3072
  // effects requested while asleep are dropped before reaching the queue
  #define AUDIO_DROP_SFX_WHILE_SLEEPING 1
#else
  #error "No audio backend: the board header must set BOARD_AUDIO_I2S_ES8311 or BOARD_AUDIO_LEDC to 1"
#endif
#pragma endregion

#pragma region Backend hooks
// Brings the hardware up. False if it failed (the backend already printed why):
// audio then stays off for this session.
bool audioBackendBegin();
// Plays one note and returns when it is over (called from the audio task only).
// frequencyHz == 0 is a silence of durationMs.
void audioBackendPlayNote(uint16_t frequencyHz, uint16_t durationMs);
// Called by the audio task after the last note of an effect.
void audioBackendSilence();
// active = hardware ready && sound enabled && pet awake. Called whenever one of
// those may have changed (also before audioBegin() if the switch is toggled).
void audioBackendPowerChanged(bool active);
#pragma endregion
