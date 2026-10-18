#pragma once

#include <Arduino.h>

void setAudioNoiseShapingEnabled(bool enabled);
void startAudioFadeIn();
void processAudioDac(uint32_t *sample);
