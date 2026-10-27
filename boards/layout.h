#pragma once

// Layout constants derived from each board's screen shape. Includers only need
// this header; it pulls in the board selection, the board-independent formulas
// and the table for the current screen:
//   layout_common.h       CX/CY, SX()/SY()/SK(), shared formulas (centered boxes, grids, mirrors)
//   layout_round_466.h    round 466x466 board: the original's literal values
//   layout_rect_320x240.h rectangular 320x240 board: dedicated redesign

#pragma region Includes
#include "board_select.h"
#include "layout_common.h"
#if BOARD_IS_ROUND
  #include "layout_round_466.h"
#else
  #include "layout_rect_320x240.h"
#endif
#pragma endregion
