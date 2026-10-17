#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

enum class PetDance : uint8_t {
  Bounce,
  Sway,
  Hop
};

enum class PetOutfit : uint8_t {
  Tie,
  Sunglasses,
  Boots,
  GlitterJacket
};

void drawPetClothing(TFT_eSprite &frameBuffer, uint8_t animalIndex,
                     PetOutfit outfit, int16_t bob);
