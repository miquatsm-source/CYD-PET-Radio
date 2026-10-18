#include "audio_dac.h"

namespace {

constexpr int32_t FADE_FULL = 32768;
constexpr int32_t FADE_STEP = 3;
constexpr int32_t DAC_STEP = 256;  // The built-in DAC uses the top 8 bits.

volatile bool noiseShapingEnabled = false;
volatile int32_t fadeGain = FADE_FULL;
int32_t errorLeft = 0;
int32_t errorRight = 0;

int32_t quantize(int32_t value, int32_t &error) {
  const int32_t wanted = value - error;
  int32_t quantized = ((wanted + DAC_STEP / 2) >> 8) << 8;
  if (quantized > 32512) {
    quantized = 32512;
  } else if (quantized < -32768) {
    quantized = -32768;
  }
  error = quantized - wanted;
  if (error > DAC_STEP) {
    error = DAC_STEP;
  } else if (error < -DAC_STEP) {
    error = -DAC_STEP;
  }
  return quantized;
}

}  // namespace

void setAudioNoiseShapingEnabled(bool enabled) {
  noiseShapingEnabled = enabled;
  errorLeft = 0;
  errorRight = 0;
}

// Ramps the volume up after a stream starts to avoid a click.
void startAudioFadeIn() { fadeGain = 0; }

void processAudioDac(uint32_t *sample) {
  if (sample == nullptr) {
    return;
  }
  const int32_t gain = fadeGain;
  if (gain >= FADE_FULL && !noiseShapingEnabled) {
    return;
  }

  int32_t left = static_cast<int16_t>(*sample >> 16);
  int32_t right = static_cast<int16_t>(*sample & 0xFFFF);
  if (gain < FADE_FULL) {
    left = left * gain >> 15;
    right = right * gain >> 15;
    fadeGain = gain + FADE_STEP;
  }
  if (noiseShapingEnabled) {
    left = quantize(left, errorLeft);
    right = quantize(right, errorRight);
  }
  *sample = (static_cast<uint32_t>(static_cast<uint16_t>(left)) << 16) |
            static_cast<uint16_t>(right);
}
