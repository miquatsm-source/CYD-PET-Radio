#include "pet_animation.h"

namespace {

constexpr uint16_t JACKET_PURPLE = 0xA81F;
constexpr uint16_t JACKET_PINK = 0xF81F;
constexpr uint16_t JACKET_GOLD = 0xFEA0;
constexpr uint16_t BOOT_BROWN = 0x8200;

}

// Draw the selected outfit over a pet, using the current bob offset to keep
// clothing aligned with its animated body.
void drawPetClothing(TFT_eSprite &frameBuffer, uint8_t animalIndex,
                     PetOutfit outfit, int16_t bob) {
  static const int16_t faceX[] = {48, 48, 48, 74, 66, 48, 48, 48, 77, 58};
  static const int16_t faceY[] = {112, 98, 95, 145, 72,
                                  80, 96, 99, 83, 116};
  const int16_t x = 48;
  const int16_t y = 137 + bob;

  switch (outfit) {
    case PetOutfit::Tie:
      frameBuffer.fillCircle(x, y - 8, 4, TFT_WHITE);
      frameBuffer.fillTriangle(x - 3, y - 5, x + 3, y - 5, x, y + 8,
                               animalIndex % 2 == 0 ? TFT_RED : TFT_CYAN);
      frameBuffer.fillTriangle(x - 3, y + 4, x + 3, y + 4, x, y + 11,
                               TFT_YELLOW);
      break;
    case PetOutfit::Sunglasses: {
      if (animalIndex == 0 || animalIndex == 9) {
        break;
      }
      const uint8_t index = animalIndex < 10 ? animalIndex : 5;
      const int16_t glassesY = faceY[index] + bob;
      const int16_t glassesX = faceX[index];
      frameBuffer.fillRoundRect(glassesX - 12, glassesY - 2, 10, 7, 2,
                                TFT_BLACK);
      frameBuffer.fillRoundRect(glassesX + 2, glassesY - 2, 10, 7, 2,
                                TFT_BLACK);
      frameBuffer.drawFastHLine(glassesX - 2, glassesY + 1, 4, TFT_BLACK);
      frameBuffer.drawFastHLine(glassesX - 17, glassesY, 5, TFT_BLACK);
      frameBuffer.drawFastHLine(glassesX + 12, glassesY, 5, TFT_BLACK);
      frameBuffer.drawPixel(glassesX - 9, glassesY, TFT_CYAN);
      frameBuffer.drawPixel(glassesX + 5, glassesY, TFT_CYAN);
      break;
    }
    case PetOutfit::Boots:
      frameBuffer.fillRoundRect(22, 174 + bob, 17, 10, 3, BOOT_BROWN);
      frameBuffer.fillRoundRect(57, 174 - bob, 17, 10, 3, BOOT_BROWN);
      frameBuffer.fillRoundRect(20, 181 + bob, 20, 4, 2, TFT_WHITE);
      frameBuffer.fillRoundRect(56, 181 - bob, 20, 4, 2, TFT_WHITE);
      frameBuffer.drawFastHLine(25, 178 + bob, 8, TFT_YELLOW);
      frameBuffer.drawFastHLine(60, 178 - bob, 8, TFT_YELLOW);
      break;
    case PetOutfit::GlitterJacket:
      frameBuffer.drawRoundRect(27, y - 4, 42, 42, 9, JACKET_PURPLE);
      frameBuffer.drawLine(48, y - 3, 42, y + 11, JACKET_PINK);
      frameBuffer.drawLine(48, y - 3, 54, y + 11, JACKET_PINK);
      frameBuffer.drawLine(48, y + 12, 48, y + 34, JACKET_GOLD);
      frameBuffer.fillCircle(34, y + 7, 1, TFT_YELLOW);
      frameBuffer.fillCircle(62, y + 12, 1, TFT_CYAN);
      frameBuffer.fillCircle(37, y + 26, 1, TFT_WHITE);
      frameBuffer.fillCircle(59, y + 31, 1, TFT_YELLOW);
      frameBuffer.drawPixel(47, y - 8, TFT_WHITE);
      frameBuffer.drawPixel(19, y + 3, TFT_YELLOW);
      frameBuffer.drawPixel(77, y - 10, TFT_CYAN);
      break;
  }
}
