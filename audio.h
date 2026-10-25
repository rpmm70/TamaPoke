#pragma once
#include <stdint.h>

// Game sound effects (queued, non-blocking). The order matches the SFX table
// in audio.cpp.
enum Sfx : uint8_t {
  SFX_TAP = 0,  // tap / button
  SFX_EAT,      // eat
  SFX_PLAY,     // minigame point / hit
  SFX_HEART,    // likes it / cuddle
  SFX_HATCH,    // hatching
  SFX_EVOLVE,   // evolution
  SFX_MEDAL,    // medal / milestone
  SFX_DENY,     // action not allowed
  SFX_BYE,      // farewell
  SFX_LEVEL,    // level up
  //#region Wild battle and ball game effects (comment markers: GCC rejects #pragma inside an enum)
  SFX_WILD_APPEAR,  // a wild Pokemon shows up (battle prompt)
  SFX_BATTLE_HIT,   // our attack lands
  SFX_BATTLE_HURT,  // the wild Pokemon hits us
  SFX_BATTLE_WIN,   // battle won
  SFX_BATTLE_LOSS,  // battle lost
  SFX_CATCH_OK,     // catch succeeded
  SFX_CATCH_FAIL,   // the wild Pokemon broke free
  SFX_BALL_BOUNCE,  // ball game: ball bounced off the paddle
  SFX_BALL_MISS,    // ball game: ball dropped
  //#endregion
  SFX_COUNT
};

void audioBegin();          // init ES8311 + I2S + amplifier + audio task
void sfxPlay(uint8_t id);   // queues an effect (does not block the loop)
void audioSetEnabled(bool on);
bool audioEnabled();
void audioSetSleeping(bool sleeping);  // asleep: amplifier off
