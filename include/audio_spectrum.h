#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "genres_animals.h"

void setAudioSpectrumEnabled(bool enabled);
void drawAudioSpectrum(TFT_eSprite &frameBuffer, uint32_t now,
                       const MusicGenreAnimal *animal, bool streamActive,
                       bool enabled);
