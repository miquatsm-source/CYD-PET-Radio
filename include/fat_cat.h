#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "pet_animation.h"

constexpr int16_t FAT_CAT_PANEL_WIDTH = 96;
constexpr uint16_t FAT_CAT_FRAME_INTERVAL_MS = 100;

void drawFatCat(TFT_eSprite &frameBuffer, uint32_t now,
                const char *petName = "Claudia",
                PetDance dance = PetDance::Bounce,
                PetOutfit outfit = PetOutfit::Tie);
