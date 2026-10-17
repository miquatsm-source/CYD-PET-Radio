#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>

#include "pet_animation.h"

struct MusicGenreAnimal {
  const char *genre;
  const char *animalType;
  const char *animalName;
  const char *description;
  int displayHeight;
};

constexpr uint8_t MUSIC_GENRE_COUNT = 10;

extern const MusicGenreAnimal genreList[MUSIC_GENRE_COUNT];

const MusicGenreAnimal *detectMusicGenre(const char *metadata);
void drawGenreAnimal(TFT_eSprite &frameBuffer, uint32_t now,
                    const MusicGenreAnimal &animal, PetDance dance,
                    PetOutfit outfit);
