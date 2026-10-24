#pragma region Includes
#include "ui_icons.h"
#include <math.h>
#pragma endregion

#pragma region Internal helpers
// Two triangles sharing the first->third diagonal. Corners in drawing order (convex quad).
static void fillQuadrilateral(Arduino_GFX *canvas,
                              float firstX, float firstY, float secondX, float secondY,
                              float thirdX, float thirdY, float fourthX, float fourthY,
                              uint16_t color) {
  int16_t roundedFirstX = (int16_t)lroundf(firstX), roundedFirstY = (int16_t)lroundf(firstY);
  int16_t roundedThirdX = (int16_t)lroundf(thirdX), roundedThirdY = (int16_t)lroundf(thirdY);
  canvas->fillTriangle(roundedFirstX, roundedFirstY,
                       (int16_t)lroundf(secondX), (int16_t)lroundf(secondY),
                       roundedThirdX, roundedThirdY, color);
  canvas->fillTriangle(roundedFirstX, roundedFirstY,
                       roundedThirdX, roundedThirdY,
                       (int16_t)lroundf(fourthX), (int16_t)lroundf(fourthY), color);
}

// Stroke thickness for an icon of the given extent: about 1/divisor of it, at least 2 px.
static int16_t strokeThicknessForExtent(int16_t extent, int16_t divisor) {
  int16_t thickness = (int16_t)((extent + divisor / 2) / divisor);
  return thickness < 2 ? 2 : thickness;
}
#pragma endregion

#pragma region Thick line
void drawThickLine(Arduino_GFX *canvas, int16_t startX, int16_t startY, int16_t endX, int16_t endY, int16_t thickness, uint16_t color) {
  #pragma region Thin or degenerate lines
  if (thickness <= 1) {
    canvas->drawLine(startX, startY, endX, endY, color);
    return;
  }
  float deltaX = (float)(endX - startX);
  float deltaY = (float)(endY - startY);
  float lineLength = sqrtf(deltaX * deltaX + deltaY * deltaY);
  int16_t capRadius = (int16_t)((thickness - 1) / 2);
  if (lineLength < 0.5f) {
    canvas->fillCircle(startX, startY, capRadius, color);
    return;
  }
  #pragma endregion

  #pragma region Body quad + round caps
  // perpendicular offset of half the thickness on each side of the center line
  float halfThickness = thickness * 0.5f;
  float offsetX = -deltaY / lineLength * halfThickness;
  float offsetY = deltaX / lineLength * halfThickness;
  fillQuadrilateral(canvas,
                    startX + offsetX, startY + offsetY,
                    endX + offsetX, endY + offsetY,
                    endX - offsetX, endY - offsetY,
                    startX - offsetX, startY - offsetY, color);
  canvas->fillCircle(startX, startY, capRadius, color);
  canvas->fillCircle(endX, endY, capRadius, color);
  #pragma endregion
}
#pragma endregion

#pragma region Close button
void drawCloseButton(Arduino_GFX *canvas, int16_t centerX, int16_t centerY, int16_t radius, uint16_t backgroundColor, uint16_t crossColor) {
  canvas->fillCircle(centerX, centerY, radius, backgroundColor);

  #pragma region Cross strokes
  // arms reach ~40% of the radius along each axis, so the round caps stay well inside the circle
  int16_t armOffset = (int16_t)lroundf(radius * 0.4f);
  int16_t thickness = strokeThicknessForExtent(radius, 5);
  drawThickLine(canvas, centerX - armOffset, centerY - armOffset, centerX + armOffset, centerY + armOffset, thickness, crossColor);
  drawThickLine(canvas, centerX - armOffset, centerY + armOffset, centerX + armOffset, centerY - armOffset, thickness, crossColor);
  #pragma endregion
}
#pragma endregion

#pragma region Pencil icon
void drawPencilIcon(Arduino_GFX *canvas, int16_t left, int16_t top, int16_t size, uint16_t color) {
  #pragma region Geometry along the diagonal
  // The pencil axis runs from the tip (bottom-left) to the eraser end (top-right).
  // Points are expressed as (distance along the axis, perpendicular offset).
  const float axisUnitX = 0.70710678f, axisUnitY = -0.70710678f;
  const float perpendicularUnitX = 0.70710678f, perpendicularUnitY = 0.70710678f;
  float margin = size * 0.08f;
  float tipX = left + margin;
  float tipY = top + size - 1 - margin;
  float pencilLength = ((left + size - 1 - margin) - tipX) * 1.41421356f;
  float halfWidth = size * 0.13f;
  if (halfWidth < 1.0f) halfWidth = 1.0f;  // body at least 2 px wide
  float gapLength = size * 0.06f;
  if (gapLength < 1.5f) gapLength = 1.5f;  // the gaps must survive rasterizing on small sizes
  float tipLength = pencilLength * 0.28f;
  float eraserLength = pencilLength * 0.16f;
  float bodyStart = tipLength + gapLength;
  float bodyEnd = pencilLength - eraserLength - gapLength;
  float eraserStart = pencilLength - eraserLength;
  #pragma endregion

  #pragma region Point helpers
  auto pointX = [&](float distanceAlongAxis, float perpendicularOffset) {
    return tipX + axisUnitX * distanceAlongAxis + perpendicularUnitX * perpendicularOffset;
  };
  auto pointY = [&](float distanceAlongAxis, float perpendicularOffset) {
    return tipY + axisUnitY * distanceAlongAxis + perpendicularUnitY * perpendicularOffset;
  };
  #pragma endregion

  #pragma region Tip, body and eraser
  canvas->fillTriangle((int16_t)lroundf(tipX), (int16_t)lroundf(tipY),
                       (int16_t)lroundf(pointX(tipLength, -halfWidth)), (int16_t)lroundf(pointY(tipLength, -halfWidth)),
                       (int16_t)lroundf(pointX(tipLength, halfWidth)), (int16_t)lroundf(pointY(tipLength, halfWidth)),
                       color);
  if (bodyEnd > bodyStart) {
    fillQuadrilateral(canvas,
                      pointX(bodyStart, -halfWidth), pointY(bodyStart, -halfWidth),
                      pointX(bodyEnd, -halfWidth), pointY(bodyEnd, -halfWidth),
                      pointX(bodyEnd, halfWidth), pointY(bodyEnd, halfWidth),
                      pointX(bodyStart, halfWidth), pointY(bodyStart, halfWidth), color);
  }
  fillQuadrilateral(canvas,
                    pointX(eraserStart, -halfWidth), pointY(eraserStart, -halfWidth),
                    pointX(pencilLength, -halfWidth), pointY(pencilLength, -halfWidth),
                    pointX(pencilLength, halfWidth), pointY(pencilLength, halfWidth),
                    pointX(eraserStart, halfWidth), pointY(eraserStart, halfWidth), color);
  #pragma endregion
}
#pragma endregion

#pragma region Hamburger icon
void drawHamburgerIcon(Arduino_GFX *canvas, int16_t left, int16_t top, int16_t width, int16_t height, uint16_t color) {
  #pragma region Bar geometry
  // three bars, the two gaps share what is left of the height
  int16_t barThickness = strokeThicknessForExtent(height, 5);
  int16_t barSpacing = (height - barThickness) / 2;  // distance between the tops of neighboring bars
  int16_t cornerRadius = barThickness / 2;
  #pragma endregion

  #pragma region Draw the three bars
  for (int16_t barIndex = 0; barIndex < 3; barIndex++) {
    canvas->fillRoundRect(left, top + barIndex * barSpacing, width, barThickness, cornerRadius, color);
  }
  #pragma endregion
}
#pragma endregion

#pragma region Page arrow
void drawPageArrow(Arduino_GFX *canvas, int16_t centerX, int16_t centerY, int16_t size, int8_t direction, uint16_t color) {
  int16_t halfSize = size / 2;
  if (halfSize < 1) halfSize = 1;
  int16_t pointingSign = direction < 0 ? -1 : 1;
  int16_t tipX = centerX + pointingSign * halfSize;
  int16_t baseX = centerX - pointingSign * halfSize;
  canvas->fillTriangle(tipX, centerY, baseX, centerY - halfSize, baseX, centerY + halfSize, color);
}
#pragma endregion

#pragma region Touch hit tests
bool isTouchInsideCircle(int16_t touchX, int16_t touchY, int16_t centerX, int16_t centerY, int16_t radius) {
  int32_t distanceX = (int32_t)touchX - centerX;
  int32_t distanceY = (int32_t)touchY - centerY;
  return distanceX * distanceX + distanceY * distanceY <= (int32_t)radius * radius;
}

bool isTouchInsideRect(int16_t touchX, int16_t touchY, int16_t left, int16_t top, int16_t width, int16_t height) {
  return (int32_t)touchX >= left && (int32_t)touchX < (int32_t)left + width &&
         (int32_t)touchY >= top && (int32_t)touchY < (int32_t)top + height;
}
#pragma endregion
