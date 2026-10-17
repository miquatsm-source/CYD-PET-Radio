#include <ctype.h>
#include <string.h>

#include "fat_cat.h"
#include "genres_animals.h"

namespace {

enum AnimalIndex : uint8_t {
  PopAnimal,
  RockAnimal,
  HipHopAnimal,
  TechnoAnimal,
  ClassicalAnimal,
  JazzAnimal,
  MetalAnimal,
  ReggaeAnimal,
  CountryAnimal,
  SchlagerAnimal
};

struct GenreKeyword {
  uint8_t animalIndex;
  const char *keyword;
};

const GenreKeyword GENRE_KEYWORDS[] = {
    {MetalAnimal, "heavy metal"}, {MetalAnimal, "metalcore"},
    {MetalAnimal, "death metal"}, {MetalAnimal, "black metal"},
    {MetalAnimal, "thrash"},      {MetalAnimal, "metal"},
    {ReggaeAnimal, "dancehall"},  {ReggaeAnimal, "reggae"},
    {ReggaeAnimal, "ska"},        {ReggaeAnimal, "dub"},
    {CountryAnimal, "bluegrass"}, {CountryAnimal, "country"},
    {CountryAnimal, "americana"}, {SchlagerAnimal, "volksmusik"},
    {SchlagerAnimal, "schlager"}, {ClassicalAnimal, "classical music"},
    {ClassicalAnimal, "classical"}, {ClassicalAnimal, "klassik"},
    {ClassicalAnimal, "orchestra"},  {ClassicalAnimal, "orchestral"},
    {ClassicalAnimal, "symphony"},   {ClassicalAnimal, "sinfonie"},
    {HipHopAnimal, "hip hop"},       {HipHopAnimal, "hiphop"},
    {HipHopAnimal, "rap"},           {HipHopAnimal, "trap"},
    {TechnoAnimal, "drum and bass"}, {TechnoAnimal, "dance music"},
    {TechnoAnimal, "techno"},        {TechnoAnimal, "electronic"},
    {TechnoAnimal, "house"},         {TechnoAnimal, "edm"},
    {TechnoAnimal, "trance"},        {TechnoAnimal, "electro"},
    {JazzAnimal, "bebop"},           {JazzAnimal, "jazz"},
    {JazzAnimal, "swing"},           {RockAnimal, "classic rock"},
    {RockAnimal, "hard rock"},       {RockAnimal, "alternative rock"},
    {RockAnimal, "indie rock"},      {RockAnimal, "rock"},
    {PopAnimal, "dance pop"},        {PopAnimal, "top 40"},
    {PopAnimal, "k pop"},            {PopAnimal, "pop"},
};

bool normalizeMetadata(const char *source, char *normalized,
                      size_t bufferSize) {
  if (source == nullptr || bufferSize < 3) {
    return false;
  }

  size_t output = 0;
  normalized[output++] = ' ';
  bool previousWasSpace = true;
  for (size_t input = 0; source[input] != '\0' && output < bufferSize - 2;
       ++input) {
    const unsigned char character = static_cast<unsigned char>(source[input]);
    if (character < 128 && isalnum(character)) {
      normalized[output++] = static_cast<char>(tolower(character));
      previousWasSpace = false;
    } else if (!previousWasSpace) {
      normalized[output++] = ' ';
      previousWasSpace = true;
    }
  }
  if (!previousWasSpace) {
    normalized[output++] = ' ';
  }
  normalized[output] = '\0';
  return output > 2;
}

bool containsKeyword(const char *normalized, const char *keyword) {
  char normalizedKeyword[32];
  if (!normalizeMetadata(keyword, normalizedKeyword,
                         sizeof(normalizedKeyword))) {
    return false;
  }

  const char *phrase = normalizedKeyword + 1;
  const size_t phraseLength = strlen(phrase) - 1;
  const char *match = normalized;
  while ((match = strstr(match, phrase)) != nullptr) {
    if (match > normalized && match[-1] == ' ' &&
        match[phraseLength] == ' ') {
      return true;
    }
    ++match;
  }
  return false;
}

void drawFrame(TFT_eSprite &frameBuffer, const char *petName) {
  frameBuffer.fillRect(0, 0, FAT_CAT_PANEL_WIDTH, 193, 0x0841);
  frameBuffer.drawFastHLine(0, 193, FAT_CAT_PANEL_WIDTH, 0x3186);
  frameBuffer.fillCircle(15, 44, 1, TFT_YELLOW);
  frameBuffer.fillCircle(79, 30, 1, TFT_CYAN);
  frameBuffer.fillCircle(77, 105, 1, TFT_YELLOW);
  frameBuffer.setTextDatum(MC_DATUM);
  frameBuffer.setTextColor(TFT_CYAN, 0x0841);
  frameBuffer.drawString(petName, 48, 18, 2);
  frameBuffer.setTextColor(0xAD55, 0x0841);
  frameBuffer.drawString("RADIO", 48, 35, 1);
}

void drawAnimal(TFT_eSprite &frame, uint8_t index, int16_t bob,
                PetDance dance, uint8_t pose) {
  static const int8_t sway[] = {0, 1, 2, 3, 2, 1, 0, -1, -2, -3, -2, -1};
  const int16_t x =
      48 + (dance == PetDance::Sway ? sway[pose] : dance == PetDance::Hop
                                                        ? sway[(pose + 3) % 12]
                                                        : 0);
  const int16_t y = 137 + bob;
  const uint16_t fur = 0xAD55;
  const uint16_t shadow = 0x6B4D;

  switch (index) {
    case PopAnimal: {
      for (int16_t feather = 0; feather < 7; ++feather) {
        const int16_t featherX = 17 + feather * 10;
        const int16_t featherY = 105 - abs(3 - feather) * 8 + bob;
        frame.drawLine(x, y + 4, featherX, featherY, 0x05BF);
        frame.fillCircle(featherX, featherY, 4, 0x07E0);
        frame.fillCircle(featherX, featherY, 2, TFT_YELLOW);
      }
      frame.fillEllipse(x, y, 13, 23, 0x05BF);
      frame.fillCircle(x, y - 25, 10, 0x05BF);
      frame.fillCircle(x, y - 27, 2, TFT_WHITE);
      frame.fillTriangle(x + 9, y - 26, x + 18, y - 22, x + 9, y - 20,
                         TFT_YELLOW);
      frame.drawLine(x - 5, y + 21, x - 7, 183 + bob, TFT_YELLOW);
      frame.drawLine(x + 5, y + 21, x + 7, 183 - bob, TFT_YELLOW);
      break;
    }
    case RockAnimal: {
      frame.fillCircle(x, y - 20, 29, 0xFD20);
      frame.fillCircle(x, y - 20, 20, 0xFBE0);
      frame.fillCircle(x - 12, y - 38, 8, fur);
      frame.fillCircle(x + 12, y - 38, 8, fur);
      frame.fillCircle(x - 7, y - 23, 2, TFT_BLACK);
      frame.fillCircle(x + 7, y - 23, 2, TFT_BLACK);
      frame.fillEllipse(x, y - 12, 9, 6, TFT_WHITE);
      frame.fillCircle(x, y - 14, 2, shadow);
      frame.fillRoundRect(x - 22, y + 2, 44, 39, 17, fur);
      frame.fillRoundRect(x - 14, y + 9, 28, 29, 12, 0xFFDF);
      break;
    }
    case HipHopAnimal: {
      frame.drawLine(x + 10, y + 12, 82, y + 4, shadow);
      frame.drawLine(82, y + 4, 87, y + 14, shadow);
      frame.fillEllipse(x, y, 19, 29, 0xC2A5);
      frame.fillEllipse(x, y + 10, 11, 12, 0xFBE0);
      frame.fillRoundRect(x - 20, y - 53, 12, 36, 5, fur);
      frame.fillRoundRect(x + 8, y - 53, 12, 36, 5, fur);
      frame.fillCircle(x - 8, y - 34, 12, fur);
      frame.fillCircle(x + 8, y - 34, 12, fur);
      frame.fillCircle(x - 4, y - 36, 2, TFT_BLACK);
      frame.fillCircle(x + 4, y - 36, 2, TFT_BLACK);
      frame.fillRoundRect(x - 15 + bob, 179, 19, 8, 4, 0xFFDF);
      frame.fillRoundRect(x + 1 - bob, 179, 19, 8, 4, 0xFFDF);
      break;
    }
    case TechnoAnimal: {
      frame.drawLine(11, 180, 84, 180, 0x7BEF);
      frame.fillEllipse(x, y, 24, 13, 0x07FF);
      frame.fillCircle(x + 18, y - 3, 12, 0x07FF);
      frame.drawCircle(15, y + 2, 14, 0xA81F);
      frame.drawCircle(15, y + 2, 8, 0xA81F);
      frame.fillCircle(x + 22, y - 7, 3, TFT_WHITE);
      frame.fillCircle(x + 23, y - 7, 1, TFT_BLACK);
      frame.fillTriangle(18, y - 5, 11, y - 14, 25, y - 8, 0x07FF);
      frame.fillTriangle(60, y - 5, 67, y - 14, 73, y - 7, 0x07FF);
      break;
    }
    case ClassicalAnimal: {
      frame.fillEllipse(x, y + 13, 28, 13, TFT_WHITE);
      frame.fillEllipse(x + 15, y - 23, 7, 22, TFT_WHITE);
      frame.fillCircle(x + 17, y - 42, 8, TFT_WHITE);
      frame.fillTriangle(x + 23, y - 43, x + 34, y - 39, x + 24, y - 36,
                         TFT_YELLOW);
      frame.drawLine(x - 12, y + 26, x - 15, 183 + bob, TFT_YELLOW);
      frame.drawLine(x + 5, y + 25, x + 3, 183 - bob, TFT_YELLOW);
      frame.drawLine(x + 2, y - 7, x + 20, y - 1, 0xBDF7);
      break;
    }
    case JazzAnimal: {
      const int16_t headY = 78 + bob;
      frame.fillCircle(x, y + 7, 32, shadow);
      frame.fillRoundRect(15, y, 66, 56, 25, shadow);
      frame.fillRoundRect(18, y - 2, 60, 54, 23, fur);
      frame.fillEllipse(x, y + 25, 18, 19, 0xFFDF);
      frame.fillTriangle(28, headY + 3, 25, headY - 23, 41, headY - 11,
                         fur);
      frame.fillTriangle(68, headY + 3, 71, headY - 23, 55, headY - 11,
                         fur);
      frame.fillCircle(x, headY, 26, fur);
      frame.fillCircle(38, headY + 1, 3, 0x07E0);
      frame.fillCircle(58, headY + 1, 3, 0x07E0);
      frame.fillTriangle(45, headY + 9, 51, headY + 9, 48, headY + 13,
                         0xF249);
      frame.drawLine(34, y + 9, 17, y - 7, shadow);
      frame.drawLine(62, y + 9, 79, y - 7, shadow);
      frame.drawLine(35, y + 53, 30 + bob, 183, shadow);
      frame.drawLine(61, y + 53, 66 - bob, 183, shadow);
      break;
    }
    case MetalAnimal: {
      frame.fillEllipse(x - 5, y, 28, 22, 0xA534);
      frame.fillEllipse(x + 12, y - 6, 17, 16, 0xBDF7);
      frame.fillTriangle(x + 12, y - 15, x + 23, y - 29, x + 21, y - 10,
                         TFT_WHITE);
      frame.fillTriangle(x + 16, y - 11, x + 34, y - 17, x + 22, y - 5,
                         0xFFDF);
      frame.fillTriangle(x - 16, y - 12, x - 23, y - 25, x - 7, y - 17,
                         0xA534);
      frame.fillCircle(x + 16, y - 8, 2, TFT_BLACK);
      frame.fillRoundRect(x - 24, y + 12, 10, 27, 4, 0xA534);
      frame.fillRoundRect(x - 5, y + 13, 10, 26, 4, 0xA534);
      frame.fillRoundRect(x + 12, y + 7, 10, 29, 4, 0xA534);
      break;
    }
    case ReggaeAnimal: {
      frame.drawLine(x - 14, y - 12, x - 17, y + 28, shadow);
      frame.drawLine(x + 13, y - 12, x + 18, y + 25, shadow);
      frame.fillRoundRect(x - 23, y - 7, 46, 49, 20, 0x84A9);
      frame.fillCircle(x, y - 29, 23, 0x84A9);
      frame.fillCircle(x - 8, y - 30, 7, 0x4208);
      frame.fillCircle(x + 8, y - 30, 7, 0x4208);
      frame.fillCircle(x - 8, y - 30, 3, TFT_WHITE);
      frame.fillCircle(x + 8, y - 30, 3, TFT_WHITE);
      frame.drawLine(x - 6, y - 18, x + 6, y - 18, 0x4208);
      frame.drawLine(x, y - 7, x - 7, y + 17, 0x4208);
      frame.drawLine(x - 7, y + 17, x + 7, y + 17, 0x4208);
      break;
    }
    case CountryAnimal: {
      frame.fillEllipse(x - 3, y + 3, 25, 17, 0x9A65);
      frame.fillTriangle(x - 6, y - 1, x + 4, y - 36, x + 23, y - 17,
                         0x9A65);
      frame.fillEllipse(x + 17, y - 25, 11, 18, 0x9A65);
      frame.fillTriangle(x + 12, y - 37, x + 10, y - 51, x + 18, y - 38,
                         shadow);
      frame.fillTriangle(x + 20, y - 37, x + 27, y - 49, x + 26, y - 34,
                         shadow);
      frame.fillCircle(x + 20, y - 26, 2, TFT_BLACK);
      frame.drawLine(x - 17, y + 13, x - 19, 183 + bob, shadow);
      frame.drawLine(x - 3, y + 15, x - 2, 183 - bob, shadow);
      frame.drawLine(x + 11, y + 10, x + 13, 183 + bob, shadow);
      frame.drawLine(x + 24, y + 5, x + 25, 183 - bob, shadow);
      break;
    }
    case SchlagerAnimal: {
      frame.fillEllipse(x, y, 21, 27, 0x07FF);
      frame.fillEllipse(x - 3, y + 3, 11, 20, 0x05BF);
      frame.fillCircle(x + 10, y - 21, 17, 0x07FF);
      frame.fillCircle(x + 5, y - 24, 2, TFT_BLACK);
      frame.fillTriangle(x + 23, y - 22, x + 35, y - 17, x + 23, y - 13,
                         0xFD20);
      frame.fillTriangle(x + 3, y - 37, x - 1, y - 49, x + 10, y - 37,
                         0x05BF);
      frame.fillTriangle(x + 14, y - 36, x + 24, y - 45, x + 21, y - 32,
                         0x05BF);
      frame.drawLine(x - 6, y + 25, x - 8, 183 + bob, TFT_YELLOW);
      frame.drawLine(x + 7, y + 24, x + 9, 183 - bob, TFT_YELLOW);
      break;
    }
  }

  if (dance == PetDance::Sway) {
    frame.drawString("♪", 13 + sway[pose], 91, 2);
    frame.drawString("♪", 79 - sway[pose], 112, 2);
  } else if (dance == PetDance::Hop && pose % 3 == 0) {
    frame.drawString("*", 12, 158, 2);
    frame.drawString("*", 80, 151, 2);
  } else if (pose % 6 == 0) {
    frame.fillCircle(14, 90, 2, TFT_YELLOW);
    frame.fillCircle(82, 108, 2, TFT_CYAN);
  }
}

void drawLabels(TFT_eSprite &frame, const MusicGenreAnimal &animal) {
  frame.fillRect(0, 198, FAT_CAT_PANEL_WIDTH, 42, 0x0841);
  frame.setTextDatum(MC_DATUM);
  frame.setTextColor(TFT_CYAN, 0x0841);
  frame.drawString(animal.genre, 48, 207, 1);
  frame.setTextColor(TFT_WHITE, 0x0841);
  frame.drawString(animal.animalType, 29, 224, 1);
  frame.drawString(animal.animalName, 70, 224, 1);
}

}  // namespace

const MusicGenreAnimal *detectMusicGenre(const char *metadata) {
  char normalized[256];
  if (!normalizeMetadata(metadata, normalized, sizeof(normalized))) {
    return nullptr;
  }

  for (const GenreKeyword &entry : GENRE_KEYWORDS) {
    if (containsKeyword(normalized, entry.keyword)) {
      return &genreList[entry.animalIndex];
    }
  }
  return nullptr;
}

void drawGenreAnimal(TFT_eSprite &frameBuffer, uint32_t now,
                     const MusicGenreAnimal &animal, PetDance dance,
                     PetOutfit outfit) {
  uint8_t index = 0;
  while (index < MUSIC_GENRE_COUNT && &genreList[index] != &animal) {
    ++index;
  }
  if (index >= MUSIC_GENRE_COUNT) {
    return;
  }

  drawFrame(frameBuffer, animal.animalName);
  const uint8_t pose = (now / FAT_CAT_FRAME_INTERVAL_MS) % 12;
  static const int8_t bounce[] = {0, -2, -5, -8, -10, -8,
                                  -5, -2, 0, 2, 1, 0};
  static const int8_t sway[] = {0, -1, -2, -3, -2, -1, 0, 1, 2, 3, 2, 1};
  static const int8_t hops[] = {0, -4, -8, -12, -8, -4,
                                0, -3, -7, -11, -7, -3};
  const int16_t bob = dance == PetDance::Sway
                          ? sway[pose]
                          : dance == PetDance::Hop ? hops[pose] : bounce[pose];
  drawAnimal(frameBuffer, index, bob, dance, pose);
  drawPetClothing(frameBuffer, index, outfit, bob);
  drawLabels(frameBuffer, animal);
}
