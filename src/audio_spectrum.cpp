#include <math.h>

#include "audio_compressor.h"
#include "audio_spectrum.h"
#include "fat_cat.h"

namespace {

constexpr uint16_t FFT_SIZE = 256;
constexpr uint8_t AUDIO_BLOCK_COUNT = 3;
constexpr uint8_t SPECTRUM_BANDS = 8;
constexpr uint32_t SPECTRUM_REFRESH_INTERVAL_MS = 250;
constexpr uint32_t AUDIO_STALE_MS = 300;
constexpr int16_t SPECTRUM_BASELINE = 238;
constexpr int16_t SPECTRUM_MAX_HEIGHT = 26;

struct AudioBlock {
  int16_t samples[FFT_SIZE];
};

constexpr uint16_t BAND_EDGES[SPECTRUM_BANDS + 1] = {
    1, 2, 4, 7, 12, 20, 34, 58, 128};
constexpr uint16_t BAND_COLOURS[SPECTRUM_BANDS] = {
    0xF800, 0xFBE0, 0xFFE0, 0x87E0, 0x07E0, 0x07FF, 0x051F, 0xA81F};

AudioBlock audioBlocks[AUDIO_BLOCK_COUNT]{};
volatile uint8_t queuedBlocks = 0;
volatile bool captureEnabled = true;
uint8_t readBlock = 0;
uint8_t writeBlock = 0;
uint16_t captureIndex = 0;
volatile uint32_t lastAudioBlockAt = 0;
uint32_t lastSpectrumAnalysisAt = 0;
float real[FFT_SIZE]{};
float imaginary[FFT_SIZE]{};
uint8_t targetHeights[SPECTRUM_BANDS]{};
uint8_t displayedHeights[SPECTRUM_BANDS]{};

void analyzeBlock(const AudioBlock &block) {
  constexpr float FFT_TWO_PI = 6.28318530718f;
  for (uint16_t i = 0; i < FFT_SIZE; ++i) {
    const float window =
        0.5f -
        0.5f * cosf(FFT_TWO_PI * i / static_cast<float>(FFT_SIZE - 1));
    real[i] = block.samples[i] * window;
    imaginary[i] = 0.0f;
  }

  for (uint16_t i = 1, j = 0; i < FFT_SIZE; ++i) {
    uint16_t bit = FFT_SIZE >> 1;
    for (; j & bit; bit >>= 1) {
      j ^= bit;
    }
    j ^= bit;
    if (i < j) {
      const float realValue = real[i];
      real[i] = real[j];
      real[j] = realValue;
    }
  }

  for (uint16_t length = 2; length <= FFT_SIZE; length <<= 1) {
    const float angle = -FFT_TWO_PI / length;
    const float stepReal = cosf(angle);
    const float stepImaginary = sinf(angle);
    for (uint16_t start = 0; start < FFT_SIZE; start += length) {
      float twiddleReal = 1.0f;
      float twiddleImaginary = 0.0f;
      const uint16_t halfLength = length >> 1;
      for (uint16_t offset = 0; offset < halfLength; ++offset) {
        const uint16_t even = start + offset;
        const uint16_t odd = even + halfLength;
        const float oddReal =
            real[odd] * twiddleReal - imaginary[odd] * twiddleImaginary;
        const float oddImaginary =
            real[odd] * twiddleImaginary + imaginary[odd] * twiddleReal;
        const float evenReal = real[even];
        const float evenImaginary = imaginary[even];

        real[even] = evenReal + oddReal;
        imaginary[even] = evenImaginary + oddImaginary;
        real[odd] = evenReal - oddReal;
        imaginary[odd] = evenImaginary - oddImaginary;

        const float nextTwiddleReal =
            twiddleReal * stepReal - twiddleImaginary * stepImaginary;
        twiddleImaginary =
            twiddleReal * stepImaginary + twiddleImaginary * stepReal;
        twiddleReal = nextTwiddleReal;
      }
    }
  }

  constexpr float NORMALIZATION = FFT_SIZE * 0.25f * 32768.0f;
  for (uint8_t band = 0; band < SPECTRUM_BANDS; ++band) {
    float bandPower = 0.0f;
    const uint16_t firstBin = BAND_EDGES[band];
    const uint16_t endBin = BAND_EDGES[band + 1];
    for (uint16_t bin = firstBin; bin < endBin; ++bin) {
      const float magnitude =
          sqrtf(real[bin] * real[bin] +
                imaginary[bin] * imaginary[bin]) /
          NORMALIZATION;
      bandPower += magnitude * magnitude;
    }

    const float magnitude =
        sqrtf(bandPower / static_cast<float>(endBin - firstBin));
    const float decibels =
        magnitude > 0.001f ? 20.0f * log10f(magnitude) : -60.0f;
    const float scaledHeight =
        (decibels + 54.0f) * SPECTRUM_MAX_HEIGHT / 54.0f;
    targetHeights[band] = scaledHeight > 0.0f
                              ? min<int16_t>(SPECTRUM_MAX_HEIGHT,
                                             static_cast<int16_t>(scaledHeight))
                              : 0;
  }
}

}  // namespace

void setAudioSpectrumEnabled(bool enabled) {
  captureEnabled = enabled;
  queuedBlocks = 0;
  readBlock = 0;
  writeBlock = 0;
  captureIndex = 0;
  lastAudioBlockAt = 0;
  lastSpectrumAnalysisAt = 0;
  for (uint8_t band = 0; band < SPECTRUM_BANDS; ++band) {
    targetHeights[band] = 0;
    displayedHeights[band] = 0;
  }
}

void audio_process_i2s(uint32_t *sample, bool *continueI2S) {
  *continueI2S = true;
  processAudioCompressor(sample);
  if (!captureEnabled) {
    return;
  }
  if (queuedBlocks >= AUDIO_BLOCK_COUNT) {
    return;
  }

  const int16_t left = static_cast<int16_t>(*sample >> 16);
  const int16_t right = static_cast<int16_t>(*sample & 0xFFFF);
  audioBlocks[writeBlock].samples[captureIndex++] =
      static_cast<int16_t>((static_cast<int32_t>(left) + right) / 2);

  if (captureIndex == FFT_SIZE) {
    captureIndex = 0;
    lastAudioBlockAt = millis();
    writeBlock = (writeBlock + 1) % AUDIO_BLOCK_COUNT;
    ++queuedBlocks;
  }
}

void drawAudioSpectrum(TFT_eSprite &frameBuffer, uint32_t now,
                       const MusicGenreAnimal *animal, bool streamActive,
                       bool enabled) {
  frameBuffer.fillRect(0, 195, FAT_CAT_PANEL_WIDTH, 45, 0x0841);
  frameBuffer.setTextDatum(MC_DATUM);
  frameBuffer.setTextColor(TFT_CYAN, 0x0841);

  if (!enabled) {
    frameBuffer.drawString("SPECTRUM OFF", FAT_CAT_PANEL_WIDTH / 2, 218, 1);
    return;
  }

  if (queuedBlocks > 0 &&
      now - lastSpectrumAnalysisAt >= SPECTRUM_REFRESH_INTERVAL_MS) {
    const uint8_t newestBlock =
        (writeBlock + queuedBlocks - 1) % AUDIO_BLOCK_COUNT;
    analyzeBlock(audioBlocks[newestBlock]);
    queuedBlocks = 0;
    readBlock = writeBlock;
    lastSpectrumAnalysisAt = now;
  }

  if (!streamActive || now - lastAudioBlockAt > AUDIO_STALE_MS) {
    for (uint8_t band = 0; band < SPECTRUM_BANDS; ++band) {
      targetHeights[band] = 0;
    }
  }

  if (animal == nullptr) {
    frameBuffer.drawString("BASS", 16, 201, 1);
    frameBuffer.drawString("TREBLE", 78, 201, 1);
  } else {
    frameBuffer.drawString(String(animal->genre) + " " + animal->animalName,
                           FAT_CAT_PANEL_WIDTH / 2, 201, 1);
  }

  constexpr int16_t BAR_WIDTH = 7;
  constexpr int16_t BAR_GAP = 4;
  constexpr int16_t TOTAL_WIDTH =
      SPECTRUM_BANDS * BAR_WIDTH + (SPECTRUM_BANDS - 1) * BAR_GAP;
  constexpr int16_t START_X = (FAT_CAT_PANEL_WIDTH - TOTAL_WIDTH) / 2;
  for (uint8_t band = 0; band < SPECTRUM_BANDS; ++band) {
    const uint8_t target = targetHeights[band];
    displayedHeights[band] =
        target > displayedHeights[band]
            ? min<uint8_t>(target, displayedHeights[band] + 3)
            : max<uint8_t>(target,
                           displayedHeights[band] > 3
                               ? displayedHeights[band] - 3
                               : 0);

    const uint8_t height = displayedHeights[band];
    if (height > 0) {
      const int16_t x = START_X + band * (BAR_WIDTH + BAR_GAP);
      frameBuffer.fillRoundRect(x, SPECTRUM_BASELINE - height, BAR_WIDTH,
                                height, 2, BAND_COLOURS[band]);
    }
  }
}
