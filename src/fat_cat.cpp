#include "fat_cat.h"

namespace {

constexpr uint16_t PANEL_COLOUR = 0x0841;
constexpr uint16_t FUR_COLOUR = 0xAD55;
constexpr uint16_t FUR_SHADOW = 0x6B4D;
constexpr uint16_t BELLY_COLOUR = 0xFFDF;
constexpr uint16_t INNER_EAR = 0xF249;
constexpr uint16_t EYE_COLOUR = 0x07E0;

}  // namespace

void drawFatCat(TFT_eSprite &frameBuffer, uint32_t now) {
  const uint8_t pose = (now / FAT_CAT_FRAME_INTERVAL_MS) % 4;
  const int16_t bounce[] = {0, -3, -5, -2};
  const int16_t armLift[] = {-8, -16, -20, -12};
  const int16_t legKick[] = {-4, 4, 8, 0};
  const int16_t bob = bounce[pose];
  const int16_t headX = 48;
  const int16_t headY = 78 + bob;
  const int16_t bodyY = 126 + bob;

  frameBuffer.fillScreen(PANEL_COLOUR);
  frameBuffer.drawFastHLine(0, 193, FAT_CAT_PANEL_WIDTH, 0x3186);
  frameBuffer.fillCircle(15, 44, 1, TFT_YELLOW);
  frameBuffer.fillCircle(79, 30, 1, TFT_CYAN);
  frameBuffer.fillCircle(77, 105, 1, TFT_YELLOW);
  frameBuffer.setTextDatum(MC_DATUM);
  frameBuffer.setTextColor(TFT_CYAN, PANEL_COLOUR);
  frameBuffer.drawString("PET", 48, 18, 2);
  frameBuffer.setTextColor(0xAD55, PANEL_COLOUR);
  frameBuffer.drawString("RADIO", 48, 35, 1);

  frameBuffer.drawLine(70, bodyY + 23, 83, bodyY + 16 + bob / 2, FUR_SHADOW);
  frameBuffer.drawLine(83, bodyY + 16 + bob / 2, 87, bodyY + 6 + bob / 2,
                   FUR_SHADOW);
  frameBuffer.fillCircle(87, bodyY + 6 + bob / 2, 4, FUR_COLOUR);

  frameBuffer.fillTriangle(headX - 20, headY - 14, headX - 23, headY - 36,
                       headX - 7, headY - 25, FUR_SHADOW);
  frameBuffer.fillTriangle(headX + 20, headY - 14, headX + 23, headY - 36,
                       headX + 7, headY - 25, FUR_SHADOW);
  frameBuffer.fillTriangle(headX - 19, headY - 19, headX - 20, headY - 31,
                       headX - 11, headY - 24, INNER_EAR);
  frameBuffer.fillTriangle(headX + 19, headY - 19, headX + 20, headY - 31,
                       headX + 11, headY - 24, INNER_EAR);
  frameBuffer.fillRoundRect(15, bodyY, 66, 64, 27, FUR_SHADOW);
  frameBuffer.fillRoundRect(18, bodyY - 2, 60, 60, 25, FUR_COLOUR);
  frameBuffer.fillEllipse(48, bodyY + 31, 20, 23, BELLY_COLOUR);

  frameBuffer.drawLine(27, bodyY + 11, 15, bodyY + armLift[pose], FUR_SHADOW);
  frameBuffer.drawLine(69, bodyY + 11, 81, bodyY + armLift[(pose + 2) % 4],
                   FUR_SHADOW);
  frameBuffer.fillCircle(15, bodyY + armLift[pose], 5, BELLY_COLOUR);
  frameBuffer.fillCircle(81, bodyY + armLift[(pose + 2) % 4], 5, BELLY_COLOUR);

  frameBuffer.fillCircle(headX, headY, 26, FUR_COLOUR);
  frameBuffer.fillCircle(38, headY + 1, 3, EYE_COLOUR);
  frameBuffer.fillCircle(58, headY + 1, 3, EYE_COLOUR);
  frameBuffer.fillCircle(38, headY + 1, 1, TFT_BLACK);
  frameBuffer.fillCircle(58, headY + 1, 1, TFT_BLACK);
  frameBuffer.fillTriangle(45, headY + 9, 51, headY + 9, 48, headY + 13,
                       INNER_EAR);
  frameBuffer.drawLine(48, headY + 13, 44, headY + 16, TFT_BLACK);
  frameBuffer.drawLine(48, headY + 13, 52, headY + 16, TFT_BLACK);
  frameBuffer.drawLine(31, headY + 11, 21, headY + 9, BELLY_COLOUR);
  frameBuffer.drawLine(31, headY + 15, 21, headY + 16, BELLY_COLOUR);
  frameBuffer.drawLine(65, headY + 11, 75, headY + 9, BELLY_COLOUR);
  frameBuffer.drawLine(65, headY + 15, 75, headY + 16, BELLY_COLOUR);

  frameBuffer.drawLine(34, bodyY + 55, 30 + legKick[pose], 181 + bob,
                       FUR_SHADOW);
  frameBuffer.drawLine(61, bodyY + 55, 66 - legKick[pose], 181 - bob,
                       FUR_SHADOW);
  frameBuffer.fillRoundRect(22 + legKick[pose], 178 + bob, 17, 8, 4,
                            BELLY_COLOUR);
  frameBuffer.fillRoundRect(59 - legKick[pose], 178 - bob, 17, 8, 4,
                            BELLY_COLOUR);

  frameBuffer.setTextColor(TFT_YELLOW, PANEL_COLOUR);
  frameBuffer.drawString("*", 13, 73 + bob, 2);
  frameBuffer.drawString("*", 82, 56 - bob, 2);
}
