#pragma once

#include <Arduino.h>

void setAudioCompressorEnabled(bool enabled);
void processAudioCompressor(uint32_t *sample);
