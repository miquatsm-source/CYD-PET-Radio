#include "audio_dac.h"

namespace {

constexpr int32_t FADE_FULL = 32768;
constexpr int32_t DAC_STEP = 256;  // The built-in DAC uses the top 8 bits.
constexpr uint32_t AUDIO_MUTE_HOLD_MS = 1000;
constexpr uint32_t AUDIO_FADE_DURATION_MS = 750;

volatile bool noiseShapingEnabled = false;
volatile bool fadeArmed = false;
volatile bool fadeStarted = false;
volatile uint32_t fadeStartedAt = 0;
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

void startAudioFadeIn() {
  fadeStarted = false;
  fadeArmed = true;
  fadeStartedAt = 0;
  errorLeft = 0;
  errorRight = 0;
}

void processAudioDac(uint32_t *sample) {
  if (sample == nullptr) {
    return;
  }
  int32_t gain = FADE_FULL;
  if (fadeArmed) {
    int32_t left = static_cast<int16_t>(*sample >> 16);
    int32_t right = static_cast<int16_t>(*sample & 0xFFFF);
    if (!fadeStarted) {
      if (left != 0 || right != 0) {
        fadeStartedAt = millis();
        fadeStarted = true;
      } else {
        *sample = 0;
        return;
      }
    }

    const uint32_t elapsed = millis() - fadeStartedAt;
    if (elapsed < AUDIO_MUTE_HOLD_MS) {
      *sample = 0;
      return;
    }
    const uint32_t fadeElapsed = elapsed - AUDIO_MUTE_HOLD_MS;
    if (fadeElapsed < AUDIO_FADE_DURATION_MS) {
      gain = static_cast<int32_t>(
          fadeElapsed * static_cast<uint32_t>(FADE_FULL) /
          AUDIO_FADE_DURATION_MS);
    } else {
      fadeArmed = false;
    }
  }

  if (gain >= FADE_FULL && !noiseShapingEnabled) {
    return;
  }

  int32_t left = static_cast<int16_t>(*sample >> 16);
  int32_t right = static_cast<int16_t>(*sample & 0xFFFF);
  if (gain < FADE_FULL) {
    left = left * gain >> 15;
    right = right * gain >> 15;
  }
  if (noiseShapingEnabled) {
    left = quantize(left, errorLeft);
    right = quantize(right, errorRight);
  }
  *sample = (static_cast<uint32_t>(static_cast<uint16_t>(left)) << 16) |
            static_cast<uint16_t>(right);
}
