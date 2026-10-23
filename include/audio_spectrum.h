#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

void setAudioSpectrumEnabled(bool enabled);
void drawAudioSpectrum(TFT_eSPI &frameBuffer, uint32_t now,
                       bool streamActive, bool enabled);
