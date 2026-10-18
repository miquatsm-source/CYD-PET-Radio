#include "audio_compressor.h"

namespace {

constexpr uint32_t COMPRESSOR_THRESHOLD = 2048;
constexpr uint32_t COMPRESSOR_ATTACK_Q15 = 256;
constexpr uint32_t COMPRESSOR_RELEASE_Q15 = 4;
constexpr uint32_t COMPRESSOR_MAKEUP_Q15 = 98304;
constexpr uint8_t COMPRESSOR_RATIO = 4;

bool compressorEnabled = false;
bool envelopeInitialized = false;
uint32_t envelope = 0;

int16_t saturateSample(int32_t sample) {
  if (sample < -32768) {
    return -32768;
  }
  if (sample > 32767) {
    return 32767;
  }
  return static_cast<int16_t>(sample);
}

// Apply a Q15 gain while keeping the integer and fractional products separate.
int32_t applyGain(int16_t sample, uint32_t gainQ15) {
  const int32_t integerGain = gainQ15 >> 15;
  const int32_t fractionalGain = gainQ15 & 0x7FFF;
  return static_cast<int32_t>(sample) * integerGain +
         (static_cast<int32_t>(sample) * fractionalGain >> 15);
}

}  // namespace

// Enable compression and reset its envelope so it adapts cleanly to new audio.
void setAudioCompressorEnabled(bool enabled) {
  compressorEnabled = enabled;
  envelopeInitialized = false;
  envelope = 0;
}

// Apply peak compression with smoothed attack/release and fixed-point gain.
void processAudioCompressor(uint32_t *sample) {
  if (!compressorEnabled || sample == nullptr) {
    return;
  }

  int16_t left = static_cast<int16_t>(*sample >> 16);
  int16_t right = static_cast<int16_t>(*sample & 0xFFFF);
  const uint32_t leftLevel =
      static_cast<uint32_t>(left < 0 ? -static_cast<int32_t>(left) : left);
  const uint32_t rightLevel =
      static_cast<uint32_t>(right < 0 ? -static_cast<int32_t>(right) : right);
  const uint32_t peak = max(leftLevel, rightLevel);

  if (!envelopeInitialized) {
    envelope = peak;
    envelopeInitialized = true;
  } else {
    const uint32_t coefficient =
        peak > envelope ? COMPRESSOR_ATTACK_Q15 : COMPRESSOR_RELEASE_Q15;
    const int32_t difference = static_cast<int32_t>(peak) -
                               static_cast<int32_t>(envelope);
    envelope = static_cast<uint32_t>(
        static_cast<int32_t>(envelope) +
        (difference * static_cast<int32_t>(coefficient) >> 15));
  }

  uint32_t gainQ15 = COMPRESSOR_MAKEUP_Q15;
  if (envelope > COMPRESSOR_THRESHOLD) {
    const uint32_t compressedLevel =
        COMPRESSOR_THRESHOLD +
        (envelope - COMPRESSOR_THRESHOLD) / COMPRESSOR_RATIO;
    gainQ15 = (compressedLevel * COMPRESSOR_MAKEUP_Q15) / envelope;
  }

  left = saturateSample(applyGain(left, gainQ15));
  right = saturateSample(applyGain(right, gainQ15));
  *sample = (static_cast<uint32_t>(static_cast<uint16_t>(left)) << 16) |
            static_cast<uint16_t>(right);
}
