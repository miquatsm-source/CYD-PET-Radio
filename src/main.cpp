#include <Arduino.h>
#include <cstring>
#include <Preferences.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <time.h>
#include <XPT2046_Touchscreen.h>
#include <WiFi.h>
#include <Audio.h>
#include <esp_system.h>

#include "audio_spectrum.h"
#include "audio_compressor.h"
#include "audio_dac.h"
#include "bubble_texts.h"
#include "colors.h"
#include "fat_cat.h"
#include "genres_animals.h"
#include "default_playlist.h"

namespace {

constexpr int16_t DISPLAY_WIDTH = 320;
constexpr int16_t DISPLAY_HEIGHT = 240;
constexpr int16_t CAT_PANEL_WIDTH = FAT_CAT_PANEL_WIDTH;
constexpr int16_t HOME_UI_LEFT = 103;
constexpr int16_t HOME_UI_WIDTH = 210;
constexpr int16_t MENU_UI_LEFT = 12;
constexpr int16_t MENU_UI_WIDTH = DISPLAY_WIDTH - 2 * MENU_UI_LEFT;
constexpr uint8_t MAX_STATIONS = 15;
constexpr uint8_t STATIONS_PER_PAGE = 5;
constexpr uint8_t STATION_NAME_SIZE = 32;
constexpr uint8_t STATION_URL_SIZE = 192;
constexpr uint16_t CAT_FRAME_INTERVAL_MS = FAT_CAT_FRAME_INTERVAL_MS;
constexpr uint16_t TOUCH_DEBOUNCE_MS = 180;
constexpr uint32_t WIFI_TIMEOUT_MS = 25000;
constexpr uint32_t WIFI_RECONNECT_DELAY_MS = 30000;
constexpr uint32_t STREAM_RETRY_DELAY_MS = 3000;
constexpr uint16_t CLOCK_UPDATE_INTERVAL_MS = 60000;
constexpr uint16_t BITRATE_UPDATE_INTERVAL_MS = 1000;
constexpr uint32_t BUBBLE_VISIBLE_MS = 2000;
constexpr uint32_t MIN_BUBBLE_INTERVAL_MS = 5000;
constexpr uint32_t MAX_BUBBLE_INTERVAL_MS = 15000;
constexpr uint32_t RANDOM_PET_INTERVAL_MS = 30000;
constexpr uint32_t PET_TRANSITION_DURATION_MS = 900;
constexpr uint32_t MIN_DANCE_DURATION_MS = 8000;
constexpr uint32_t MAX_DANCE_DURATION_MS = 16000;
constexpr int8_t AUDIO_BOOST_DB = 6;
constexpr uint8_t AUDIO_AMP_ENABLE_PIN = 4;
constexpr char BERLIN_TIMEZONE[] = "CET-1CEST,M3.5.0/2,M10.5.0/3";

constexpr uint16_t PANEL = 0x18E3;
constexpr uint16_t PANEL_LIGHT = 0x2945;
constexpr uint16_t ACCENT = 0x05BF;
constexpr uint8_t THEME_COLOR_COUNT = 5;
constexpr uint8_t DEFAULT_THEME_COLOR = 1;
constexpr int16_t HOME_THEME_SWATCH_WIDTH = 36;
constexpr int16_t HOME_THEME_SWATCH_HEIGHT = 16;
constexpr int16_t HOME_THEME_SWATCH_GAP = 6;
constexpr int16_t MENU_THEME_SWATCH_WIDTH = 50;
constexpr int16_t MENU_THEME_SWATCH_HEIGHT = 20;
constexpr int16_t MENU_THEME_SWATCH_GAP = 8;
constexpr uint16_t THEME_COLORS[THEME_COLOR_COUNT] = {
    COLOR_DARK_BLUE, COLOR_DARK_GREY, COLOR_DARK_GREEN, COLOR_DARK_RED,
    COLOR_DARK_SLATE};
constexpr uint16_t TEXT_MUTED = 0x9CF3;
constexpr uint16_t SUCCESS = 0x4FE7;
constexpr uint16_t ERROR_COLOUR = 0xF986;

struct Station {
  char name[STATION_NAME_SIZE]{};
  char url[STATION_URL_SIZE]{};
};

struct TouchCalibration {
  uint16_t xLeft;
  uint16_t xRight;
  uint16_t yTop;
  uint16_t yBottom;
};

enum class Screen : uint8_t {
  Home,
  Settings,
  Wifi,
  Stations,
  StationEditor,
  Keyboard
};

enum class KeyboardTarget : uint8_t {
  Ssid,
  Password,
  StationName,
  StationUrl
};

enum class PetTransition : uint8_t {
  Entering,
  Idle,
  Exiting
};

TFT_eSPI display;
// Composite the UI and cat before pushing one complete frame to the TFT.
TFT_eSprite screenFrameBuffer(&display);
TFT_eSPI *renderTarget = &display;
int16_t UI_LEFT = HOME_UI_LEFT;
int16_t UI_WIDTH = HOME_UI_WIDTH;
int16_t themeSwatchWidth = HOME_THEME_SWATCH_WIDTH;
int16_t themeSwatchHeight = HOME_THEME_SWATCH_HEIGHT;
int16_t themeSwatchGap = HOME_THEME_SWATCH_GAP;
SPIClass touchSPI(HSPI);
XPT2046_Touchscreen touch(33, 36);
Preferences preferences;
// DAC channel 2 is GPIO 26, the CYD speaker output.
Audio radioAudio(true, I2S_DAC_CHANNEL_LEFT_EN);

Station stations[MAX_STATIONS]{};
uint8_t stationCount = 0;
uint8_t selectedStation = 0;
uint8_t stationListPage = 0;
uint8_t volume = 10;
uint8_t themeColorIndex = DEFAULT_THEME_COLOR;
uint16_t backgroundColor = THEME_COLORS[DEFAULT_THEME_COLOR];
uint8_t editorIndex = 0;
uint8_t deleteIndex = 0;
TouchCalibration touchCalibration{};
Screen currentScreen = Screen::Home;
Screen keyboardReturnScreen = Screen::Wifi;
KeyboardTarget keyboardTarget = KeyboardTarget::Ssid;

String wifiSsid;
String wifiPassword;
String editName;
String editUrl;
String keyboardValue;
String streamTitle;
String streamStation;
String statusMessage = "Set up Wi-Fi";

bool touchHeld = false;
bool wifiConnecting = false;
bool playWhenConnected = false;
bool streamActive = false;
bool streamRestartPending = false;
bool showWifiPassword = false;
bool deleteConfirmation = false;
bool stationEditorIsNew = false;
bool audioBoost = false;
bool audioAmpEnabled = false;
bool audioSpectrumEnabled = true;
bool audioCompressorEnabled = false;
bool audioNoiseShaping = false;
bool keyboardUppercase = false;
uint8_t keyboardPage = 0;
bool uiDirty = true;
bool audioDirty = false;
bool displayReady = false;
volatile uint8_t wifiDisconnectReason = 0;
volatile bool wifiDisconnectReasonPending = false;
const MusicGenreAnimal *currentGenreAnimal = nullptr;
const MusicGenreAnimal *fallbackPet = nullptr;
const MusicGenreAnimal *displayedPet = nullptr;
uint32_t wifiConnectStartedAt = 0;
uint32_t streamRestartAt = 0;
uint32_t lastTouchAt = 0;
uint32_t lastCatFrameAt = 0;
uint32_t petTransitionStartedAt = 0;
uint32_t nextDanceChangeAt = 0;
uint32_t lastClockUpdateAt = 0;
uint32_t lastBitrateUpdateAt = 0;
uint32_t lastDisplayedBitrateKbps = UINT32_MAX;
uint32_t lastFallbackPetChangeAt = 0;
uint32_t lastBubbleShownAt = 0;
uint32_t bubbleIntervalMs = MIN_BUBBLE_INTERVAL_MS;
uint8_t bubblePetIndex = UINT8_MAX;
uint8_t bubbleTextIndex = 0;
bool bubbleVisible = false;
bool petAnimationInitialized = false;
PetTransition petTransition = PetTransition::Entering;
PetDance petDance = PetDance::Bounce;
PetOutfit petOutfit = PetOutfit::Tie;

void startWifiConnection();
void markUiDirty();
void updateScreenLayout();
uint8_t stationPageCount();
void showStationPageFor(uint8_t index);

void updateGenreAnimal() {
  char metadata[256];
  const char *configuredStation =
      stationCount > 0 && selectedStation < stationCount
          ? stations[selectedStation].name
          : "";
  snprintf(metadata, sizeof(metadata), "%s %s %s", configuredStation,
           streamStation.c_str(), streamTitle.c_str());

  const MusicGenreAnimal *detected = detectMusicGenre(metadata);
  if (detected != currentGenreAnimal) {
    currentGenreAnimal = detected;
    fallbackPet = nullptr;
    lastFallbackPetChangeAt = millis();
    if (detected == nullptr) {
      Serial.println("Genre: no genre found in station or stream metadata.");
    } else {
      Serial.printf("Genre: %s; animal: %s (%s).\n", detected->genre,
                    detected->animalType, detected->animalName);
    }
    audioDirty = true;
  }
}

void chooseDifferentFallbackPet() {
  uint8_t previousIndex = 5;
  if (fallbackPet != nullptr) {
    for (uint8_t i = 0; i < MUSIC_GENRE_COUNT; ++i) {
      if (&genreList[i] == fallbackPet) {
        previousIndex = i;
        break;
      }
    }
  }

  uint8_t nextIndex = esp_random() % (MUSIC_GENRE_COUNT - 1);
  if (nextIndex >= previousIndex) {
    ++nextIndex;
  }
  fallbackPet = &genreList[nextIndex];
}

void chooseDifferentBubbleText() {
  uint8_t nextIndex = esp_random() % 9;
  if (nextIndex >= bubbleTextIndex) {
    ++nextIndex;
  }
  bubbleTextIndex = nextIndex;
}

uint32_t randomBubbleInterval() {
  return MIN_BUBBLE_INTERVAL_MS +
         esp_random() % (MAX_BUBBLE_INTERVAL_MS - MIN_BUBBLE_INTERVAL_MS + 1);
}

const MusicGenreAnimal *desiredPet() {
  return currentGenreAnimal != nullptr ? currentGenreAnimal : fallbackPet;
}

PetDance randomDifferentDance(PetDance previous) {
  uint8_t choice = esp_random() % 2;
  const uint8_t previousIndex = static_cast<uint8_t>(previous);
  if (choice >= previousIndex) {
    ++choice;
  }
  return static_cast<PetDance>(choice);
}

uint32_t randomDanceDuration() {
  return MIN_DANCE_DURATION_MS +
         esp_random() % (MAX_DANCE_DURATION_MS - MIN_DANCE_DURATION_MS + 1);
}

void updatePetAnimation(uint32_t now) {
  const MusicGenreAnimal *wantedPet = desiredPet();
  if (!petAnimationInitialized) {
    displayedPet = wantedPet;
    petDance = static_cast<PetDance>(esp_random() % 3);
    petOutfit = static_cast<PetOutfit>(esp_random() % 4);
    petTransition = PetTransition::Entering;
    petTransitionStartedAt = now;
    nextDanceChangeAt = now + randomDanceDuration();
    petAnimationInitialized = true;
    return;
  }

  if (petTransition == PetTransition::Exiting &&
      now - petTransitionStartedAt >= PET_TRANSITION_DURATION_MS) {
    displayedPet = wantedPet;
    petDance = static_cast<PetDance>(esp_random() % 3);
    petOutfit = static_cast<PetOutfit>(esp_random() % 4);
    petTransition = PetTransition::Entering;
    petTransitionStartedAt = now;
    nextDanceChangeAt = now + randomDanceDuration();
  } else if (petTransition == PetTransition::Entering &&
             now - petTransitionStartedAt >= PET_TRANSITION_DURATION_MS) {
    petTransition = PetTransition::Idle;
    if (displayedPet != wantedPet) {
      petTransition = PetTransition::Exiting;
      petTransitionStartedAt = now;
    }
  } else if (petTransition == PetTransition::Idle) {
    if (displayedPet != wantedPet) {
      petTransition = PetTransition::Exiting;
      petTransitionStartedAt = now;
    } else if (now >= nextDanceChangeAt) {
      petDance = randomDifferentDance(petDance);
      nextDanceChangeAt = now + randomDanceDuration();
    }
  }
}

int16_t petTransitionOffset(uint32_t now) {
  if (petTransition == PetTransition::Idle) {
    return 0;
  }
  const uint32_t elapsed = min<uint32_t>(
      now - petTransitionStartedAt, PET_TRANSITION_DURATION_MS);
  const int16_t travel = static_cast<int16_t>(
      elapsed * FAT_CAT_PANEL_WIDTH / PET_TRANSITION_DURATION_MS);
  return petTransition == PetTransition::Exiting
             ? -travel
             : travel - FAT_CAT_PANEL_WIDTH;
}

void shiftPetPanel(TFT_eSprite &frameBuffer, int16_t offset) {
  if (offset == 0) {
    return;
  }
  const int16_t distance = abs(offset);
  if (distance >= FAT_CAT_PANEL_WIDTH) {
    frameBuffer.fillRect(0, 0, FAT_CAT_PANEL_WIDTH, 195, 0x0841);
    return;
  }

  auto *pixels = static_cast<uint8_t *>(frameBuffer.getPointer());
  if (pixels == nullptr) {
    Serial.println("ERROR: missing display frame buffer during pet transition.");
    return;
  }
  for (int16_t y = 0; y < 195; ++y) {
    uint8_t *row = pixels + y * DISPLAY_WIDTH;
    if (offset > 0) {
      memmove(row + distance, row, FAT_CAT_PANEL_WIDTH - distance);
    } else {
      memmove(row, row + distance, FAT_CAT_PANEL_WIDTH - distance);
    }
  }
  if (offset > 0) {
    frameBuffer.fillRect(0, 0, distance, 195, 0x0841);
  } else {
    frameBuffer.fillRect(FAT_CAT_PANEL_WIDTH - distance, 0, distance, 195,
                         0x0841);
  }
}

void updateBubbleText(uint32_t now) {
  const MusicGenreAnimal *pet =
      currentGenreAnimal != nullptr ? currentGenreAnimal : fallbackPet;
  uint8_t nextPetIndex = 5;
  if (pet != nullptr) {
    for (uint8_t i = 0; i < MUSIC_GENRE_COUNT; ++i) {
      if (&genreList[i] == pet) {
        nextPetIndex = i;
        break;
      }
    }
  }

  if (nextPetIndex != bubblePetIndex) {
    bubblePetIndex = nextPetIndex;
    bubbleTextIndex = esp_random() % 10;
    bubbleVisible = true;
    lastBubbleShownAt = now;
    bubbleIntervalMs = randomBubbleInterval();
    markUiDirty();
    return;
  }

  if (bubbleVisible && now - lastBubbleShownAt >= BUBBLE_VISIBLE_MS) {
    bubbleVisible = false;
    markUiDirty();
  } else if (!bubbleVisible &&
             now - lastBubbleShownAt >= bubbleIntervalMs) {
    chooseDifferentBubbleText();
    bubbleVisible = true;
    lastBubbleShownAt = now;
    bubbleIntervalMs = randomBubbleInterval();
    markUiDirty();
  }
}

void drawSpeechBubble(TFT_eSprite &frameBuffer, const char *text) {
  constexpr int16_t BUBBLE_X = 4;
  constexpr int16_t BUBBLE_Y = 48;
  constexpr int16_t BUBBLE_WIDTH = 88;
  constexpr int16_t BUBBLE_HEIGHT = 30;
  frameBuffer.fillRoundRect(BUBBLE_X, BUBBLE_Y, BUBBLE_WIDTH, BUBBLE_HEIGHT,
                            6, TFT_WHITE);
  frameBuffer.drawRoundRect(BUBBLE_X, BUBBLE_Y, BUBBLE_WIDTH, BUBBLE_HEIGHT,
                            6, 0x4208);
  frameBuffer.fillTriangle(39, 76, 49, 76, 44, 84, TFT_WHITE);
  frameBuffer.drawLine(39, 76, 44, 84, 0x4208);
  frameBuffer.drawLine(49, 76, 44, 84, 0x4208);
  frameBuffer.setTextDatum(MC_DATUM);
  frameBuffer.setTextColor(TFT_BLACK, TFT_WHITE);
  frameBuffer.drawString(text, 48, 63, 1);
}

void drawCatFrame(uint32_t now) {
  if (displayedPet == nullptr) {
    drawFatCat(screenFrameBuffer, now, "Claudia", petDance, petOutfit);
  } else {
    drawGenreAnimal(screenFrameBuffer, now, *displayedPet, petDance, petOutfit);
  }
  if (bubbleVisible) {
    drawSpeechBubble(screenFrameBuffer,
                     bubbleTexts[bubblePetIndex][bubbleTextIndex]);
  }
  shiftPetPanel(screenFrameBuffer, petTransitionOffset(now));
  drawAudioSpectrum(screenFrameBuffer, now, currentGenreAnimal, streamActive,
                    audioSpectrumEnabled);
  screenFrameBuffer.pushSprite(0, 0);
}

bool pointInRect(int16_t x, int16_t y, int16_t left, int16_t top,
                 int16_t width, int16_t height) {
  return x >= left && x < left + width && y >= top && y < top + height;
}

void markUiDirty() {
  uiDirty = true;
}

String fittedText(const String &source, int16_t maxWidth, uint8_t font = 2) {
  TFT_eSPI &display = *renderTarget;
  String result = source;
  if (display.textWidth(result, font) <= maxWidth) {
    return result;
  }

  while (result.length() > 3 &&
         display.textWidth(result + "...", font) > maxWidth) {
    result.remove(result.length() - 1);
  }
  return result + "...";
}

String passwordForDisplay(const String &password) {
  if (showWifiPassword) {
    return password;
  }

  String masked;
  masked.reserve(password.length());
  for (size_t i = 0; i < password.length(); ++i) {
    masked += '*';
  }
  return masked;
}

String keyboardRow(uint8_t row) {
  if (keyboardPage == 1) {
    const String symbolRows[] = {"1234567890", "!@#$%^&*()",
                                 "-_./:;?+="};
    return symbolRows[row];
  }
  if (keyboardPage == 2) {
    const String symbolRows[] = {"[]{}<>~", "`\\|,", "'\""};
    return symbolRows[row];
  }
  const String letterRows[] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
  return letterRows[row];
}

bool wideKeyboardLayout() {
  return UI_WIDTH == MENU_UI_WIDTH;
}

int16_t keyboardKeyGap() {
  return wideKeyboardLayout() ? 4 : 1;
}

int16_t keyboardKeyHeight() {
  return wideKeyboardLayout() ? 32 : 25;
}

int16_t keyboardRowY(uint8_t row) {
  return wideKeyboardLayout() ? 67 + row * 37 : 66 + row * 29;
}

int16_t keyboardKeyWidth(const String &keys) {
  const int16_t gap = keyboardKeyGap();
  const int16_t availableWidth = UI_WIDTH - (keys.length() - 1) * gap;
  return min<int16_t>(wideKeyboardLayout() ? 48 : 32,
                      availableWidth / keys.length());
}

void keyboardControlBounds(uint8_t index, int16_t &x, int16_t &width) {
  if (wideKeyboardLayout()) {
    constexpr int16_t controlWidths[] = {56, 60, 90, 54};
    const int16_t controlsWidth = 56 + 6 + 60 + 6 + 90 + 6 + 54;
    const int16_t controlsLeft = UI_LEFT + (UI_WIDTH - controlsWidth) / 2;
    x = controlsLeft;
    for (uint8_t i = 0; i < index; ++i) {
      x += controlWidths[i] + 6;
    }
    width = controlWidths[index];
    return;
  }

  constexpr int16_t controlLefts[] = {0, 43, 95, 165};
  constexpr int16_t controlWidths[] = {39, 48, 66, 45};
  x = UI_LEFT + controlLefts[index];
  width = controlWidths[index];
}

uint16_t primaryTextColor() {
  return TFT_WHITE;
}

int16_t themeSwatchX(uint8_t index) {
  const int16_t paletteWidth =
      THEME_COLOR_COUNT * themeSwatchWidth +
      (THEME_COLOR_COUNT - 1) * themeSwatchGap;
  return UI_LEFT + (UI_WIDTH - paletteWidth) / 2 +
         index * (themeSwatchWidth + themeSwatchGap);
}

void updateScreenLayout() {
  if (currentScreen == Screen::Home) {
    UI_LEFT = HOME_UI_LEFT;
    UI_WIDTH = HOME_UI_WIDTH;
    themeSwatchWidth = HOME_THEME_SWATCH_WIDTH;
    themeSwatchHeight = HOME_THEME_SWATCH_HEIGHT;
    themeSwatchGap = HOME_THEME_SWATCH_GAP;
    return;
  }
  UI_LEFT = MENU_UI_LEFT;
  UI_WIDTH = MENU_UI_WIDTH;
  themeSwatchWidth = MENU_THEME_SWATCH_WIDTH;
  themeSwatchHeight = MENU_THEME_SWATCH_HEIGHT;
  themeSwatchGap = MENU_THEME_SWATCH_GAP;
}

void setThemeColor(uint8_t index) {
  if (index >= THEME_COLOR_COUNT || index == themeColorIndex) {
    return;
  }

  themeColorIndex = index;
  backgroundColor = THEME_COLORS[index];
  if (preferences.putUChar("theme", themeColorIndex) == 0) {
    Serial.println("ERROR: could not save background color setting.");
    statusMessage = "Could not save background color";
  }
  markUiDirty();
}

void drawText(const String &text, int16_t x, int16_t y, uint8_t font = 2,
              uint16_t colour = TFT_WHITE) {
  TFT_eSPI &display = *renderTarget;
  display.setTextDatum(TL_DATUM);
  display.setTextColor(colour == TFT_WHITE ? primaryTextColor() : colour,
                       backgroundColor);
  display.drawString(text, x, y, font);
}

void drawButton(int16_t x, int16_t y, int16_t width, int16_t height,
                const String &label, uint16_t fill = PANEL_LIGHT,
                uint16_t colour = TFT_WHITE, uint8_t font = 2) {
  TFT_eSPI &display = *renderTarget;
  display.fillRoundRect(x, y, width, height, 7, fill);
  display.drawRoundRect(x, y, width, height, 7, 0x4208);
  display.setTextDatum(MC_DATUM);
  display.setTextColor(colour == TFT_WHITE ? primaryTextColor() : colour,
                       fill);
  display.drawString(label, x + width / 2, y + height / 2, font);
}

void drawPanelHeader(const String &title) {
  TFT_eSPI &display = *renderTarget;
  const int16_t panelLeft =
      currentScreen == Screen::Home ? CAT_PANEL_WIDTH : 0;
  display.fillRect(panelLeft, 0, DISPLAY_WIDTH - panelLeft,
                   DISPLAY_HEIGHT, backgroundColor);
  display.setTextDatum(TL_DATUM);
  display.setTextColor(TFT_CYAN, backgroundColor);
  display.drawString(title, UI_LEFT, 9, 2);
  display.drawFastHLine(UI_LEFT, 31, UI_WIDTH, 0x4208);
}

String wifiStatusText() {
  if (wifiConnecting) {
    return "Connecting to Wi-Fi...";
  }
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.localIP().toString();
  }
  if (wifiSsid.isEmpty()) {
    return "Wi-Fi not configured";
  }
  return statusMessage;
}

void drawHome() {
  TFT_eSPI &display = *renderTarget;
  drawPanelHeader("PET RADIO");
  struct tm localTime {};
  char clockText[24] = "--.--.---- --:--";
  if (getLocalTime(&localTime, 0)) {
    snprintf(clockText, sizeof(clockText), "%02d.%02d.%04d %02d:%02d",
             localTime.tm_mday, localTime.tm_mon + 1,
             localTime.tm_year + 1900, localTime.tm_hour, localTime.tm_min);
  }
  display.setTextDatum(TR_DATUM);
  display.setTextColor(primaryTextColor(), backgroundColor);
  display.drawString(clockText, UI_LEFT + UI_WIDTH, 2, 1);
  const String genreText =
      currentGenreAnimal == nullptr
          ? "Genre: --"
          : String("Genre: ") + currentGenreAnimal->genre;
  display.setTextColor(TFT_CYAN, backgroundColor);
  display.drawString(fittedText(genreText, 112, 1), UI_LEFT + UI_WIDTH, 14, 1);

  const bool connected = WiFi.status() == WL_CONNECTED;
  display.fillCircle(UI_LEFT + 5, 43, 3, connected ? SUCCESS : ERROR_COLOUR);
  const String wifiLabel =
      connected ? "Wi-Fi connected " + String(WiFi.RSSI()) + " dBm"
                : wifiStatusText();
  drawText(fittedText(wifiLabel, UI_WIDTH - 10, 1), UI_LEFT + 13, 37, 1,
           connected ? SUCCESS : TEXT_MUTED);
  if (connected) {
    const uint32_t bitrateKbps =
        streamActive ? (radioAudio.getBitRate() + 500) / 1000 : 0;
    display.setTextDatum(TR_DATUM);
    display.setTextColor(TEXT_MUTED, backgroundColor);
    display.drawString(bitrateKbps == 0
                           ? "-- kbit/s"
                           : String(bitrateKbps) + " kbit/s",
                       UI_LEFT + UI_WIDTH, 45, 1);
  }

  display.fillRoundRect(UI_LEFT, 53, UI_WIDTH, 45, 7, PANEL);
  display.setTextDatum(TL_DATUM);
  display.setTextColor(primaryTextColor(), PANEL);
  const String stationName =
      stationCount == 0 ? "No stations" : stations[selectedStation].name;
  display.drawString(fittedText(stationName, UI_WIDTH - 14), UI_LEFT + 7, 59,
                     2);
  display.setTextColor(TEXT_MUTED, PANEL);
  String subtitle = streamTitle.isEmpty()
                        ? (streamStation.isEmpty() ? statusMessage
                                                   : streamStation)
                        : streamTitle;
  display.drawString(fittedText(subtitle, UI_WIDTH - 14, 1), UI_LEFT + 7, 82,
                     1);

  drawButton(UI_LEFT, 105, 49, 36, "|<", PANEL_LIGHT);
  drawButton(UI_LEFT + 55, 105, 100, 36,
             streamActive ? "PAUSE" : "PLAY",
             streamActive ? 0x3286 : ACCENT);
  drawButton(UI_LEFT + 161, 105, 49, 36, ">|", PANEL_LIGHT);

  display.setTextDatum(TL_DATUM);
  display.setTextColor(primaryTextColor(), backgroundColor);
  display.drawString("VOLUME", UI_LEFT, 151, 1);
  display.setTextDatum(TR_DATUM);
  display.drawString(String(volume * 100 / 21) + "%", UI_LEFT + UI_WIDTH,
                     151, 1);
  display.fillRoundRect(UI_LEFT, 168, UI_WIDTH, 8, 4, PANEL_LIGHT);
  const int16_t sliderWidth =
      static_cast<int16_t>((UI_WIDTH - 4) * volume / 21);
  display.fillRoundRect(UI_LEFT + 2, 170, sliderWidth, 4, 2, ACCENT);
  display.fillCircle(UI_LEFT + 2 + sliderWidth, 172, 6, TFT_WHITE);

  drawButton(UI_LEFT, 190, 99, 36, "STATIONS", PANEL_LIGHT);
  drawButton(UI_LEFT + 105, 190, 105, 36, "MENU", PANEL_LIGHT);
}

void drawSettings() {
  TFT_eSPI &display = *renderTarget;
  drawPanelHeader("SETTINGS");
  const int16_t toggleWidth = (UI_WIDTH - 6) / 2;
  drawButton(UI_LEFT, 36, toggleWidth, 25, "WI-FI", PANEL_LIGHT);
  drawButton(UI_LEFT + toggleWidth + 6, 36, toggleWidth, 25, "STATION LIST",
             PANEL_LIGHT);
  drawButton(UI_LEFT, 63, toggleWidth, 24,
             String("BOOST +6: ") + (audioBoost ? "ON" : "OFF"),
             audioBoost ? 0x2147 : PANEL_LIGHT, TFT_WHITE, 1);
  drawButton(UI_LEFT + toggleWidth + 6, 63, toggleWidth, 24,
             String("SPECTRUM: ") + (audioSpectrumEnabled ? "ON" : "OFF"),
             audioSpectrumEnabled ? 0x2147 : PANEL_LIGHT, TFT_WHITE, 1);
  drawButton(UI_LEFT, 90, toggleWidth, 24,
             String("COMPRESSION: ") + (audioCompressorEnabled ? "ON" : "OFF"),
             audioCompressorEnabled ? 0x2147 : PANEL_LIGHT, TFT_WHITE, 1);
  drawButton(UI_LEFT + toggleWidth + 6, 90, toggleWidth, 24,
             String("GPIO4 AMP: ") + (audioAmpEnabled ? "ON" : "OFF"),
             audioAmpEnabled ? 0x2147 : PANEL_LIGHT, TFT_WHITE, 1);
  drawButton(UI_LEFT, 117, toggleWidth, 22, "RECALIBRATE TOUCH",
             PANEL_LIGHT, TFT_WHITE, 1);
  drawButton(UI_LEFT + toggleWidth + 6, 117, toggleWidth, 22,
             String("NOISE SHAPE: ") + (audioNoiseShaping ? "ON" : "OFF"),
             audioNoiseShaping ? 0x2147 : PANEL_LIGHT, TFT_WHITE, 1);
  display.setTextDatum(TL_DATUM);
  display.setTextColor(TEXT_MUTED, backgroundColor);
  display.drawString("VOLUME", UI_LEFT, 147, 1);
  display.setTextDatum(TR_DATUM);
  display.drawString(String(volume * 100 / 21) + "%", UI_LEFT + UI_WIDTH,
                     147, 1);
  display.fillRoundRect(UI_LEFT, 163, UI_WIDTH, 8, 4, PANEL_LIGHT);
  const int16_t sliderWidth =
      static_cast<int16_t>((UI_WIDTH - 4) * volume / 21);
  display.fillRoundRect(UI_LEFT + 2, 165, sliderWidth, 4, 2, ACCENT);
  display.fillCircle(UI_LEFT + 2 + sliderWidth, 167, 6, TFT_WHITE);
  display.setTextDatum(TL_DATUM);
  display.setTextColor(primaryTextColor(), backgroundColor);
  display.drawString("BACKGROUND", UI_LEFT, 177, 1);
  constexpr int16_t swatchY = 188;
  for (uint8_t index = 0; index < THEME_COLOR_COUNT; ++index) {
    const int16_t swatchX = themeSwatchX(index);
    display.fillRoundRect(swatchX, swatchY, themeSwatchWidth,
                          themeSwatchHeight, 4, THEME_COLORS[index]);
    display.drawRoundRect(
        swatchX - 1, swatchY - 1, themeSwatchWidth + 2,
        themeSwatchHeight + 2, 5,
        index == themeColorIndex ? primaryTextColor() : 0x4208);
  }
  drawButton(UI_LEFT, 213, UI_WIDTH, 23, "BACK", PANEL_LIGHT, TFT_WHITE, 1);
}

void drawWifiSettings() {
  TFT_eSPI &display = *renderTarget;
  drawPanelHeader("WI-FI SETTINGS");
  display.setTextDatum(TL_DATUM);
  display.setTextColor(TEXT_MUTED, backgroundColor);
  display.drawString("NETWORK NAME (SSID)", UI_LEFT, 40, 1);
  drawButton(UI_LEFT, 54, UI_WIDTH, 34,
             fittedText(wifiSsid.isEmpty() ? "(Enter SSID)" : wifiSsid,
                        UI_WIDTH - 16, 1),
             PANEL);
  display.setTextColor(TEXT_MUTED, backgroundColor);
  display.drawString("PASSWORD", UI_LEFT, 97, 1);
  String passwordPreview = passwordForDisplay(wifiPassword);
  if (wifiPassword.isEmpty()) {
    passwordPreview = "(Enter password)";
  }
  drawButton(UI_LEFT, 111, UI_WIDTH, 34,
             fittedText(passwordPreview, UI_WIDTH - 16, 1), PANEL);

  drawButton(UI_LEFT, 151, UI_WIDTH, 18,
             String("SHOW PASSWORD: ") + (showWifiPassword ? "ON" : "OFF"),
             showWifiPassword ? 0x2147 : PANEL, TFT_WHITE, 1);
  const int16_t actionWidth = (UI_WIDTH - 8) / 2;
  drawButton(UI_LEFT, 176, actionWidth, 32, "CONNECT",
             wifiConnecting ? 0x3186 : ACCENT, TFT_WHITE, 1);
  drawButton(UI_LEFT + actionWidth + 8, 176, actionWidth, 32, "BACK",
             PANEL_LIGHT, TFT_WHITE, 1);
  drawText(fittedText(wifiStatusText(), UI_WIDTH, 1), UI_LEFT, 216, 1,
           WiFi.status() == WL_CONNECTED ? SUCCESS : TEXT_MUTED);
}

void drawStations() {
  TFT_eSPI &display = *renderTarget;
  drawPanelHeader("STATION LIST");
  const uint8_t pageCount = stationPageCount();
  const uint8_t pageStart = stationListPage * STATIONS_PER_PAGE;
  const uint8_t visibleStations =
      min<uint8_t>(STATIONS_PER_PAGE, stationCount - pageStart);
  drawText(String(stationCount) + " STATIONS", UI_LEFT, 32, 1, TEXT_MUTED);
  if (deleteConfirmation) {
    for (uint8_t row = 0; row < visibleStations; ++row) {
      const uint8_t i = pageStart + row;
      const int16_t rowY = 41 + row * 27;
      drawButton(UI_LEFT, rowY, UI_WIDTH, 24,
                 fittedText(stations[i].name, UI_WIDTH - 10, 1),
                 i == selectedStation ? 0x2147 : PANEL, TFT_WHITE, 1);
    }
    display.fillRoundRect(UI_LEFT + 9, 83, UI_WIDTH - 18, 83, 8, PANEL_LIGHT);
    display.drawRoundRect(UI_LEFT + 9, 83, UI_WIDTH - 18, 83, 8, TFT_CYAN);
    display.setTextDatum(MC_DATUM);
    display.setTextColor(primaryTextColor(), PANEL_LIGHT);
    display.drawString("Delete station?", 208, 101, 2);
    display.drawString(
        fittedText(stations[deleteIndex].name, 170, 1), 208, 124, 1);
    drawButton(UI_LEFT + 14, 136, 127, 25, "CANCEL", PANEL, TFT_WHITE, 1);
    drawButton(UI_LEFT + 147, 136, 127, 25, "DELETE", ERROR_COLOUR,
               TFT_WHITE, 1);
    return;
  }

  for (uint8_t row = 0; row < visibleStations; ++row) {
    const uint8_t i = pageStart + row;
    const int16_t rowY = 41 + row * 27;
    const uint16_t fill = i == selectedStation ? 0x2147 : PANEL;
    display.fillRoundRect(UI_LEFT, rowY, UI_WIDTH, 24, 5, fill);
    display.setTextDatum(ML_DATUM);
    display.setTextColor(primaryTextColor(), fill);
    display.drawString(fittedText(stations[i].name, UI_WIDTH - 82, 1),
                       UI_LEFT + 7, rowY + 12, 1);
    display.setTextDatum(MC_DATUM);
    display.setTextColor(TEXT_MUTED, fill);
    display.drawString("E", UI_LEFT + UI_WIDTH - 48, rowY + 12, 1);
    display.drawString("x", UI_LEFT + UI_WIDTH - 17, rowY + 12, 2);
  }
  const int16_t controlsWidth = 36 + 4 + 36 + 4 + 52 + 4 + 100 + 4 + 52;
  const int16_t controlsLeft = UI_LEFT + (UI_WIDTH - controlsWidth) / 2;
  drawButton(controlsLeft, 204, 36, 25, "<", PANEL_LIGHT, TFT_WHITE, 1);
  drawButton(controlsLeft + 40, 204, 36, 25, ">", PANEL_LIGHT, TFT_WHITE, 1);
  display.setTextDatum(MC_DATUM);
  display.setTextColor(primaryTextColor(), backgroundColor);
  display.drawString(String(stationListPage + 1) + "/" + String(pageCount),
                     controlsLeft + 104, 216, 1);
  drawButton(controlsLeft + 132, 204, 100, 25, "+ STATION", ACCENT,
             TFT_WHITE, 1);
  drawButton(controlsLeft + 236, 204, 52, 25, "BACK", PANEL_LIGHT, TFT_WHITE,
             1);
}

void drawStationEditor() {
  TFT_eSPI &display = *renderTarget;
  drawPanelHeader(stationEditorIsNew ? "NEW STATION" : "EDIT STATION");
  display.setTextDatum(TL_DATUM);
  display.setTextColor(TEXT_MUTED, backgroundColor);
  display.drawString("NAME", UI_LEFT, 40, 1);
  drawButton(UI_LEFT, 54, UI_WIDTH, 32,
             fittedText(editName, UI_WIDTH - 14, 1), PANEL);
  display.setTextColor(TEXT_MUTED, backgroundColor);
  display.drawString("STREAM-URL", UI_LEFT, 94, 1);
  drawButton(UI_LEFT, 108, UI_WIDTH, 32,
             fittedText(editUrl, UI_WIDTH - 14, 1), PANEL);
  const int16_t actionWidth = (UI_WIDTH - 8) / 2;
  drawButton(UI_LEFT, 159, actionWidth, 36, "SAVE", ACCENT, TFT_WHITE, 1);
  drawButton(UI_LEFT + actionWidth + 8, 159, actionWidth, 36, "BACK",
             PANEL_LIGHT, TFT_WHITE, 1);
  drawText("MP3/AAC-Stream (http/https)", UI_LEFT, 210, 1, TEXT_MUTED);
}

void drawKeyboard() {
  TFT_eSPI &display = *renderTarget;
  drawPanelHeader(keyboardTarget == KeyboardTarget::Ssid
                      ? "ENTER SSID"
                  : keyboardTarget == KeyboardTarget::Password
                      ? "ENTER PASSWORD"
                  : keyboardTarget == KeyboardTarget::StationName
                      ? "STATION NAME"
                      : "ENTER STREAM URL");
  const bool editingPassword = keyboardTarget == KeyboardTarget::Password;
  const String visibleValue =
      editingPassword ? passwordForDisplay(keyboardValue) : keyboardValue;
  const int16_t valueWidth = editingPassword && !showWifiPassword
                                 ? UI_WIDTH - (wideKeyboardLayout() ? 62 : 53)
                                 : UI_WIDTH;
  drawText(fittedText(visibleValue, valueWidth, 1), UI_LEFT,
           wideKeyboardLayout() ? 38 : 40, 1, TFT_WHITE);
  if (editingPassword) {
    const int16_t showWidth = wideKeyboardLayout() ? 54 : 48;
    drawButton(UI_LEFT + UI_WIDTH - showWidth, 36, showWidth,
               wideKeyboardLayout() ? 22 : 19,
               showWifiPassword ? "HIDE" : "SHOW", PANEL_LIGHT,
               TFT_WHITE, 1);
  }
  const int16_t separatorY = wideKeyboardLayout() ? 61 : 58;
  display.drawFastHLine(UI_LEFT, separatorY, UI_WIDTH, 0x4208);

  for (uint8_t row = 0; row < 3; ++row) {
    const String keys = keyboardRow(row);
    if (keys.isEmpty()) {
      continue;
    }
    const int16_t keyGap = keyboardKeyGap();
    const int16_t keyWidth = keyboardKeyWidth(keys);
    const int16_t rowWidth =
        keys.length() * keyWidth + (keys.length() - 1) * keyGap;
    const int16_t rowLeft = UI_LEFT + (UI_WIDTH - rowWidth) / 2;
    for (uint8_t column = 0; column < keys.length(); ++column) {
      const int16_t x = rowLeft + column * (keyWidth + keyGap);
      const int16_t y = keyboardRowY(row);
      String label(keys[column]);
      if (keyboardPage == 0 && keyboardUppercase) {
        label.toUpperCase();
      }
      drawButton(x, y, keyWidth, keyboardKeyHeight(), label, PANEL_LIGHT,
                 TFT_WHITE, 2);
    }
  }

  const int16_t controlY = wideKeyboardLayout() ? 180 : 156;
  const int16_t controlHeight = wideKeyboardLayout() ? 26 : 24;
  int16_t controlX = 0;
  int16_t controlWidth = 0;
  keyboardControlBounds(0, controlX, controlWidth);
  drawButton(controlX, controlY, controlWidth, controlHeight,
             keyboardPage == 0 && !keyboardUppercase ? "abc" : "ABC",
             PANEL_LIGHT, TFT_WHITE, 1);
  keyboardControlBounds(1, controlX, controlWidth);
  drawButton(controlX, controlY, controlWidth, controlHeight,
             keyboardPage == 0 ? "?123"
                               : keyboardPage == 1 ? "SYM2" : "SYM1",
             PANEL_LIGHT, TFT_WHITE, 1);
  keyboardControlBounds(2, controlX, controlWidth);
  drawButton(controlX, controlY, controlWidth, controlHeight, "SPACE",
             PANEL_LIGHT, TFT_WHITE, 1);
  keyboardControlBounds(3, controlX, controlWidth);
  drawButton(controlX, controlY, controlWidth, controlHeight, "<-",
             PANEL_LIGHT, TFT_WHITE, 1);
  const int16_t actionY = wideKeyboardLayout() ? 211 : 192;
  const int16_t actionHeight = wideKeyboardLayout() ? 24 : 31;
  const int16_t actionWidth = wideKeyboardLayout() ? (UI_WIDTH - 8) / 2 : 98;
  constexpr int16_t actionGap = 8;
  drawButton(UI_LEFT, actionY, actionWidth, actionHeight, "CANCEL",
             PANEL_LIGHT, TFT_WHITE, 1);
  drawButton(UI_LEFT + actionWidth + actionGap, actionY,
             wideKeyboardLayout() ? actionWidth : 104, actionHeight, "DONE",
             ACCENT, TFT_WHITE, 1);
}

void renderScreen() {
  updateScreenLayout();
  renderTarget = &screenFrameBuffer;
  screenFrameBuffer.fillSprite(backgroundColor);
  if (currentScreen == Screen::Home) {
    screenFrameBuffer.setViewport(CAT_PANEL_WIDTH, 0,
                                  DISPLAY_WIDTH - CAT_PANEL_WIDTH,
                                  DISPLAY_HEIGHT, false);
  }
  switch (currentScreen) {
    case Screen::Home:
      drawHome();
      break;
    case Screen::Settings:
      drawSettings();
      break;
    case Screen::Wifi:
      drawWifiSettings();
      break;
    case Screen::Stations:
      drawStations();
      break;
    case Screen::StationEditor:
      drawStationEditor();
      break;
    case Screen::Keyboard:
      drawKeyboard();
      break;
  }
  if (currentScreen == Screen::Home) {
    screenFrameBuffer.resetViewport();
    drawCatFrame(millis());
  } else {
    screenFrameBuffer.pushSprite(0, 0);
  }
  uiDirty = false;
  renderTarget = &display;
}

void drawCalibrationTarget(uint8_t index) {
  const int16_t targetX[] = {20, DISPLAY_WIDTH - 21, 20, DISPLAY_WIDTH - 21};
  const int16_t targetY[] = {20, 20, DISPLAY_HEIGHT - 21, DISPLAY_HEIGHT - 21};
  display.fillScreen(backgroundColor);
  display.setTextDatum(TL_DATUM);
  display.setTextColor(primaryTextColor(), backgroundColor);
  display.drawString("Touch calibration", 24, 72, 2);
  display.drawString("Touch target " + String(index + 1) + "/4", 24, 101,
                     2);
  display.drawCircle(targetX[index], targetY[index], 13, TFT_CYAN);
  display.drawFastHLine(targetX[index] - 18, targetY[index], 37, TFT_CYAN);
  display.drawFastVLine(targetX[index], targetY[index] - 18, 37, TFT_CYAN);
}

void waitForTouchRelease() {
  while (touch.touched()) {
    delay(10);
  }
  delay(120);
}

void calibrateTouchscreen() {
  const int16_t targetX[] = {20, DISPLAY_WIDTH - 21, 20, DISPLAY_WIDTH - 21};
  const int16_t targetY[] = {20, 20, DISPLAY_HEIGHT - 21, DISPLAY_HEIGHT - 21};

  for (;;) {
    uint16_t measuredX[4]{};
    uint16_t measuredY[4]{};
    for (uint8_t target = 0; target < 4; ++target) {
      drawCalibrationTarget(target);
      waitForTouchRelease();
      while (!touch.touched()) {
        delay(10);
      }
      uint32_t sumX = 0;
      uint32_t sumY = 0;
      uint8_t samples = 0;
      while (samples == 0) {
        while (!touch.touched()) {
          delay(10);
        }
        for (uint8_t sample = 0; sample < 8; ++sample) {
          if (touch.touched()) {
            const TS_Point point = touch.getPoint();
            sumX += point.x;
            sumY += point.y;
            ++samples;
          }
          delay(8);
        }
      }
      measuredX[target] = sumX / samples;
      measuredY[target] = sumY / samples;
      waitForTouchRelease();
    }

    touchCalibration.xLeft = (measuredX[0] + measuredX[2]) / 2;
    touchCalibration.xRight = (measuredX[1] + measuredX[3]) / 2;
    touchCalibration.yTop = (measuredY[0] + measuredY[1]) / 2;
    touchCalibration.yBottom = (measuredY[2] + measuredY[3]) / 2;

    const bool valid =
        abs(static_cast<int>(touchCalibration.xRight) -
            touchCalibration.xLeft) >= 500 &&
        abs(static_cast<int>(touchCalibration.yBottom) -
            touchCalibration.yTop) >= 500;
    if (valid) {
      break;
    }

    display.fillScreen(TFT_RED);
    display.setTextColor(TFT_WHITE, TFT_RED);
    display.drawCentreString("Calibration failed", DISPLAY_WIDTH / 2,
                             DISPLAY_HEIGHT / 2, 2);
    Serial.println("ERROR: touchscreen calibration range is invalid.");
    delay(1500);
  }

  const bool saved =
      preferences.putUShort("xLeft", touchCalibration.xLeft) > 0 &&
      preferences.putUShort("xRight", touchCalibration.xRight) > 0 &&
      preferences.putUShort("yTop", touchCalibration.yTop) > 0 &&
      preferences.putUShort("yBottom", touchCalibration.yBottom) > 0;
  if (!saved) {
    Serial.println("ERROR: could not save touchscreen calibration.");
  }
}

int16_t mapTouchCoordinate(uint16_t raw, uint16_t start, uint16_t end,
                           int16_t screenSize) {
  if (start == end) {
    return 0;
  }
  const int32_t mapped =
      (static_cast<int32_t>(raw) - start) * (screenSize - 1) /
      (static_cast<int32_t>(end) - start);
  return constrain(mapped, 0, screenSize - 1);
}

bool readTouchPoint(int16_t &x, int16_t &y) {
  if (!touch.touched()) {
    touchHeld = false;
    return false;
  }
  if (touchHeld) {
    return false;
  }
  touchHeld = true;

  const uint32_t now = millis();
  if (now - lastTouchAt < TOUCH_DEBOUNCE_MS) {
    return false;
  }
  lastTouchAt = now;

  uint32_t sumX = 0;
  uint32_t sumY = 0;
  uint8_t samples = 0;
  for (uint8_t i = 0; i < 3 && touch.touched(); ++i) {
    const TS_Point point = touch.getPoint();
    sumX += point.x;
    sumY += point.y;
    ++samples;
    delayMicroseconds(1200);
  }
  if (samples == 0) {
    return false;
  }
  x = mapTouchCoordinate(sumX / samples, touchCalibration.xLeft,
                         touchCalibration.xRight, display.width());
  y = mapTouchCoordinate(sumY / samples, touchCalibration.yTop,
                         touchCalibration.yBottom, display.height());
  return true;
}

void setStatus(const String &message) {
  statusMessage = message;
  markUiDirty();
}

uint8_t stationPageCount() {
  return max<uint8_t>(
      1, (stationCount + STATIONS_PER_PAGE - 1) / STATIONS_PER_PAGE);
}

void showStationPageFor(uint8_t index) {
  stationListPage = index / STATIONS_PER_PAGE;
}

bool saveVolume() {
  return preferences.putUChar("volume", volume) > 0;
}

bool saveStationList() {
  bool saved = preferences.putUChar("count", stationCount) > 0;
  for (uint8_t i = 0; i < stationCount; ++i) {
    char nameKey[5];
    char urlKey[5];
    snprintf(nameKey, sizeof(nameKey), "n%u", i);
    snprintf(urlKey, sizeof(urlKey), "u%u", i);
    saved &= preferences.putString(nameKey, stations[i].name) > 0;
    saved &= preferences.putString(urlKey, stations[i].url) > 0;
  }
  if (!saved) {
    setStatus("Could not save station list");
    Serial.println("ERROR: could not persist the station list.");
  }
  return saved;
}

void loadDefaultStations() {
  stationCount = DEFAULT_STATION_COUNT;
  for (uint8_t i = 0; i < stationCount; ++i) {
    strlcpy(stations[i].name, DEFAULT_STATION_NAMES[i],
            sizeof(stations[i].name));
    strlcpy(stations[i].url, DEFAULT_STATION_URLS[i],
            sizeof(stations[i].url));
  }
}

bool isLegacyDefaultPlaylist() {
  if (stationCount != 3) {
    return false;
  }
  return strcmp(stations[0].name, "Radio Paradise") == 0 &&
         strcmp(stations[0].url, "http://stream.radioparadise.com/mp3-128") ==
             0 &&
         strcmp(stations[1].name, "SomaFM Groove Salad") == 0 &&
         strcmp(stations[1].url,
                "https://ice1.somafm.com/groovesalad-128-mp3") == 0 &&
         strcmp(stations[2].name, "Deutschlandfunk") == 0 &&
         strcmp(stations[2].url,
                "https://st01.sslstream.dlf.de/dlf/01/128/mp3/stream.mp3") ==
             0;
}

void loadStations() {
  if (!preferences.isKey("count")) {
    loadDefaultStations();
    saveStationList();
  } else {
    stationCount =
        min<uint8_t>(preferences.getUChar("count", 0), MAX_STATIONS);
    for (uint8_t i = 0; i < stationCount; ++i) {
      char nameKey[5];
      char urlKey[5];
      snprintf(nameKey, sizeof(nameKey), "n%u", i);
      snprintf(urlKey, sizeof(urlKey), "u%u", i);
      preferences.getString(nameKey, stations[i].name,
                            sizeof(stations[i].name));
      preferences.getString(urlKey, stations[i].url,
                            sizeof(stations[i].url));
      if (stations[i].name[0] == '\0' || stations[i].url[0] == '\0') {
        Serial.printf("ERROR: saved station %u is incomplete.\n", i + 1);
        stationCount = i;
        saveStationList();
        break;
      }
    }
    if (isLegacyDefaultPlaylist()) {
      loadDefaultStations();
      saveStationList();
      Serial.println("Migrated default playlist to favorite stations.");
    }
  }
  if (stationCount == 0) {
    setStatus("No stations available");
  }
}

void setVolume(uint8_t newVolume) {
  volume = min<uint8_t>(newVolume, 21);
  radioAudio.setVolume(volume);
  if (!saveVolume()) {
    setStatus("Could not save volume");
  } else {
    markUiDirty();
  }
}

void setAudioBoost(bool enabled) {
  audioBoost = enabled;
  const int8_t gain = audioBoost ? AUDIO_BOOST_DB : 0;
  radioAudio.setTone(gain, gain, gain);
  if (preferences.putBool("boost", audioBoost) == 0) {
    Serial.println("ERROR: could not save audio boost setting.");
    setStatus("Could not save audio boost");
    return;
  }
  setStatus(audioBoost ? "Audio boost enabled" : "Audio boost disabled");
}

void setAudioSpectrum(bool enabled) {
  audioSpectrumEnabled = enabled;
  setAudioSpectrumEnabled(enabled);
  if (preferences.putBool("spectrum", audioSpectrumEnabled) == 0) {
    Serial.println("ERROR: could not save spectrum setting.");
    setStatus("Could not save spectrum setting");
    return;
  }
  markUiDirty();
}

void setAudioCompressor(bool enabled) {
  audioCompressorEnabled = enabled;
  setAudioCompressorEnabled(enabled);
  if (preferences.putBool("compressor", audioCompressorEnabled) == 0) {
    Serial.println("ERROR: could not save audio compression setting.");
    setStatus("Could not save compression setting");
    return;
  }
  setStatus(audioCompressorEnabled ? "Audio compression enabled"
                                   : "Audio compression disabled");
}

void setAudioNoiseShaping(bool enabled) {
  audioNoiseShaping = enabled;
  setAudioNoiseShapingEnabled(enabled);
  if (preferences.putBool("noiseshape", audioNoiseShaping) == 0) {
    Serial.println("ERROR: could not save noise shaping setting.");
    setStatus("Could not save noise shaping setting");
    return;
  }
  setStatus(audioNoiseShaping ? "Noise shaping enabled"
                              : "Noise shaping disabled");
}

void setAudioAmpEnabled(bool enabled) {
  audioAmpEnabled = enabled;
  digitalWrite(AUDIO_AMP_ENABLE_PIN, enabled ? LOW : HIGH);
  pinMode(AUDIO_AMP_ENABLE_PIN, OUTPUT);
  if (preferences.putBool("amp4", audioAmpEnabled) == 0) {
    Serial.println("ERROR: could not save GPIO 4 amplifier setting.");
    setStatus("Could not save GPIO 4 setting");
    return;
  }
  setStatus(enabled ? "GPIO 4 amplifier enabled"
                    : "GPIO 4 amplifier disabled");
}

void startStream() {
  if (stationCount == 0) {
    setStatus("No stations available");
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    setStatus(wifiSsid.isEmpty() ? "Set up Wi-Fi" : "Wi-Fi not connected");
    return;
  }

  streamTitle = "";
  streamStation = "";
  streamRestartPending = false;
  updateGenreAnimal();
  radioAudio.stopSong();
  radioAudio.setVolume(volume);
  startAudioFadeIn();
  if (!radioAudio.connecttohost(stations[selectedStation].url)) {
    streamActive = false;
    setStatus("Could not start stream");
    Serial.printf("ERROR: could not connect to station %u.\n",
                  selectedStation + 1);
    return;
  }
  streamActive = true;
  setStatus("Connecting to stream...");
}

void selectStation(uint8_t index) {
  if (index >= stationCount) {
    return;
  }
  selectedStation = index;
  streamTitle = "";
  streamStation = "";
  updateGenreAnimal();
  if (preferences.putUChar("selected", selectedStation) == 0) {
    Serial.println("ERROR: could not save selected station.");
  }
  currentScreen = Screen::Home;
  if (WiFi.status() == WL_CONNECTED) {
    startStream();
  } else if (!wifiSsid.isEmpty()) {
    startWifiConnection();
  } else {
    setStatus("Set up Wi-Fi");
  }
  markUiDirty();
}

void beginWifiConnection() {
  if (wifiSsid.isEmpty()) {
    setStatus("Enter an SSID first");
    return;
  }
  radioAudio.stopSong();
  streamActive = false;
  startWifiConnection();
}

bool saveWifiCredentials() {
  const bool savedSsid = preferences.putString("ssid", wifiSsid) > 0;
  const bool savedPassword =
      preferences.putString("password", wifiPassword) > 0;
  if (!savedSsid || !savedPassword) {
    Serial.println("ERROR: could not save Wi-Fi credentials.");
    setStatus("Could not save Wi-Fi credentials");
    return false;
  }
  return true;
}

void startWifiConnection() {
  if (wifiPassword.length() < 8 || wifiPassword.length() > 63) {
    wifiConnecting = false;
    playWhenConnected = false;
    setStatus("WPA2 password must be 8-63 characters");
    return;
  }

  wifiDisconnectReason = 0;
  wifiDisconnectReasonPending = false;
  wifiConnecting = true;
  playWhenConnected = true;
  wifiConnectStartedAt = millis();
  setStatus("Connecting to Wi-Fi...");

  const wl_status_t result =
      WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  if (result == WL_CONNECT_FAILED) {
    wifiConnecting = false;
    playWhenConnected = false;
    setStatus("Could not start Wi-Fi");
    Serial.println("ERROR: Wi-Fi rejected the connection request.");
  }
  Serial.printf("Wi-Fi connect requested (SSID length %u, password length "
                "%u, initial status %d).\n",
                static_cast<unsigned>(wifiSsid.length()),
                static_cast<unsigned>(wifiPassword.length()),
                static_cast<int>(result));
}

String wifiFailureMessage(wl_status_t status, uint8_t reason) {
  if (reason == WIFI_REASON_NO_AP_FOUND || status == WL_NO_SSID_AVAIL) {
    return "SSID not found (check 2.4 GHz network)";
  }
  if (reason == WIFI_REASON_AUTH_FAIL ||
      reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
      reason == WIFI_REASON_HANDSHAKE_TIMEOUT ||
      status == WL_CONNECT_FAILED) {
    return "Authentication failed: check WPA2 password";
  }
  if (reason == WIFI_REASON_ASSOC_FAIL) {
    return "Router rejected the connection";
  }
  if (status == WL_CONNECTION_LOST) {
    return "Wi-Fi connection lost";
  }
  return "Wi-Fi error " + String(reason) + " (status " +
         String(static_cast<int>(status)) + ")";
}

void updateNetwork() {
  if (wifiDisconnectReasonPending) {
    const uint8_t reason = wifiDisconnectReason;
    wifiDisconnectReasonPending = false;
    Serial.printf("Wi-Fi disconnected: reason %u (%s), status %d.\n",
                  reason,
                  WiFi.disconnectReasonName(
                      static_cast<wifi_err_reason_t>(reason)),
                  static_cast<int>(WiFi.status()));
  }

  static uint32_t wifiLostAt = 0;
  if (WiFi.status() == WL_CONNECTED) {
    wifiLostAt = 0;
    if (wifiConnecting) {
      wifiConnecting = false;
      wifiDisconnectReason = 0;
      configTzTime(BERLIN_TIMEZONE, "pool.ntp.org", "time.google.com");
      setStatus("Wi-Fi connected");
      if (playWhenConnected) {
        playWhenConnected = false;
        startStream();
      }
    }
    return;
  }

  // Restart the connection (and stream) when Wi-Fi stays down for 30 s.
  if (wifiConnecting || wifiSsid.isEmpty()) {
    wifiLostAt = 0;
  } else if (wifiLostAt == 0) {
    wifiLostAt = millis();
  } else if (millis() - wifiLostAt >= WIFI_RECONNECT_DELAY_MS) {
    wifiLostAt = 0;
    Serial.println("Wi-Fi down for 30 s, reconnecting.");
    WiFi.disconnect();
    startWifiConnection();
    return;
  }

  if (wifiConnecting && millis() - wifiConnectStartedAt > WIFI_TIMEOUT_MS) {
    wifiConnecting = false;
    playWhenConnected = false;
    const wl_status_t status = WiFi.status();
    const uint8_t reason = wifiDisconnectReason;
    setStatus(wifiFailureMessage(status, reason));
    Serial.printf("ERROR: Wi-Fi connection timed out (status %d, reason %u).\n",
                  static_cast<int>(status), reason);
  }
}

void startStationEditor(bool isNew, uint8_t index) {
  stationEditorIsNew = isNew;
  editorIndex = index;
  editName = isNew ? "" : stations[index].name;
  editUrl = isNew ? "" : stations[index].url;
  if (isNew) {
    showStationPageFor(index);
  }
  currentScreen = Screen::StationEditor;
  markUiDirty();
}

void beginKeyboard(KeyboardTarget target) {
  keyboardTarget = target;
  keyboardReturnScreen = currentScreen;
  switch (target) {
    case KeyboardTarget::Ssid:
      keyboardValue = wifiSsid;
      break;
    case KeyboardTarget::Password:
      keyboardValue = wifiPassword;
      break;
    case KeyboardTarget::StationName:
      keyboardValue = editName;
      break;
    case KeyboardTarget::StationUrl:
      keyboardValue = editUrl;
      break;
  }
  keyboardPage = 0;
  keyboardUppercase = false;
  currentScreen = Screen::Keyboard;
  markUiDirty();
}

void finishKeyboard(bool accept) {
  if (accept) {
    switch (keyboardTarget) {
      case KeyboardTarget::Ssid:
        wifiSsid = keyboardValue;
        break;
      case KeyboardTarget::Password:
        wifiPassword = keyboardValue;
        break;
      case KeyboardTarget::StationName:
        editName = keyboardValue;
        break;
      case KeyboardTarget::StationUrl:
        editUrl = keyboardValue;
        break;
    }
  }
  currentScreen = keyboardReturnScreen;
  markUiDirty();
}

uint16_t maximumKeyboardLength() {
  switch (keyboardTarget) {
    case KeyboardTarget::Ssid:
      return 32;
    case KeyboardTarget::Password:
      return 63;
    case KeyboardTarget::StationName:
      return STATION_NAME_SIZE - 1;
    case KeyboardTarget::StationUrl:
      return STATION_URL_SIZE - 1;
  }
  return 0;
}

void appendKeyboardCharacter(char value) {
  if (keyboardValue.length() < maximumKeyboardLength()) {
    keyboardValue += value;
    markUiDirty();
  }
}

void handleKeyboardTouch(int16_t x, int16_t y) {
  const int16_t showWidth = wideKeyboardLayout() ? 54 : 48;
  if (keyboardTarget == KeyboardTarget::Password &&
      pointInRect(x, y, UI_LEFT + UI_WIDTH - showWidth, 36, showWidth,
                  wideKeyboardLayout() ? 22 : 19)) {
    showWifiPassword = !showWifiPassword;
    markUiDirty();
    return;
  }

  for (uint8_t row = 0; row < 3; ++row) {
    const String keys = keyboardRow(row);
    if (keys.isEmpty()) {
      continue;
    }
    const int16_t keyGap = keyboardKeyGap();
    const int16_t keyWidth = keyboardKeyWidth(keys);
    const int16_t rowWidth = keys.length() * keyWidth +
                             (keys.length() - 1) * keyGap;
    const int16_t rowLeft = UI_LEFT + (UI_WIDTH - rowWidth) / 2;
    for (uint8_t column = 0; column < keys.length(); ++column) {
      const int16_t keyX = rowLeft + column * (keyWidth + keyGap);
      const int16_t keyY = keyboardRowY(row);
      if (pointInRect(x, y, keyX, keyY, keyWidth, keyboardKeyHeight())) {
        char value = keys[column];
        if (keyboardPage == 0 && keyboardUppercase) {
          value = static_cast<char>(value - 'a' + 'A');
        }
        appendKeyboardCharacter(value);
        return;
      }
    }
  }

  const int16_t controlY = wideKeyboardLayout() ? 180 : 156;
  const int16_t controlHeight = wideKeyboardLayout() ? 26 : 24;
  int16_t controlX = 0;
  int16_t controlWidth = 0;
  keyboardControlBounds(0, controlX, controlWidth);
  if (pointInRect(x, y, controlX, controlY, controlWidth, controlHeight)) {
    if (keyboardPage == 0) {
      keyboardUppercase = !keyboardUppercase;
    } else {
      keyboardPage = 0;
    }
  } else {
    keyboardControlBounds(1, controlX, controlWidth);
    if (pointInRect(x, y, controlX, controlY, controlWidth, controlHeight)) {
      keyboardPage = keyboardPage == 0 ? 1 : keyboardPage == 1 ? 2 : 1;
    } else {
      keyboardControlBounds(2, controlX, controlWidth);
      if (pointInRect(x, y, controlX, controlY, controlWidth,
                      controlHeight)) {
        appendKeyboardCharacter(' ');
      } else {
        keyboardControlBounds(3, controlX, controlWidth);
        if (pointInRect(x, y, controlX, controlY, controlWidth,
                        controlHeight) &&
            !keyboardValue.isEmpty()) {
          keyboardValue.remove(keyboardValue.length() - 1);
        }
      }
    }
  }

  const int16_t actionY = wideKeyboardLayout() ? 211 : 192;
  const int16_t actionHeight = wideKeyboardLayout() ? 24 : 31;
  const int16_t actionWidth = wideKeyboardLayout() ? (UI_WIDTH - 8) / 2 : 98;
  const int16_t actionGap = 8;
  if (pointInRect(x, y, UI_LEFT, actionY, actionWidth, actionHeight)) {
    finishKeyboard(false);
    return;
  } else if (pointInRect(x, y, UI_LEFT + actionWidth + actionGap, actionY,
                         wideKeyboardLayout() ? actionWidth : 104,
                         actionHeight)) {
    finishKeyboard(true);
    return;
  }
  markUiDirty();
}

void saveStationEditor() {
  editName.trim();
  editUrl.trim();
  if (editName.isEmpty() || editUrl.isEmpty() ||
      !(editUrl.startsWith("http://") || editUrl.startsWith("https://"))) {
    setStatus("Enter a name and valid HTTP(S) URL");
    return;
  }

  const uint8_t index = stationEditorIsNew ? stationCount : editorIndex;
  const bool changedPlayingStation =
      !stationEditorIsNew && streamActive && index == selectedStation;
  if (index >= MAX_STATIONS) {
    setStatus("Station list is full");
    return;
  }
  strlcpy(stations[index].name, editName.c_str(), sizeof(stations[index].name));
  strlcpy(stations[index].url, editUrl.c_str(), sizeof(stations[index].url));
  if (stationEditorIsNew) {
    ++stationCount;
    selectedStation = index;
  }
  const bool saved = saveStationList();
  currentScreen = Screen::Stations;
  if (saved) {
    setStatus("Station saved");
    if (changedPlayingStation) {
      startStream();
    }
  }
}

void deleteStation(uint8_t index) {
  if (index >= stationCount) {
    deleteConfirmation = false;
    return;
  }
  const bool deletedCurrentStation = streamActive && index == selectedStation;
  for (uint8_t i = index; i + 1 < stationCount; ++i) {
    stations[i] = stations[i + 1];
  }
  --stationCount;
  if (stationListPage >= stationPageCount()) {
    stationListPage = stationPageCount() - 1;
  }
  if (stationCount == 0) {
    selectedStation = 0;
    streamActive = false;
    streamTitle = "";
    streamStation = "";
    updateGenreAnimal();
    radioAudio.stopSong();
  } else if (index < selectedStation) {
    --selectedStation;
  } else if (index == selectedStation && selectedStation >= stationCount) {
    selectedStation = stationCount - 1;
  }
  const bool saved = saveStationList();
  if (preferences.putUChar("selected", selectedStation) == 0) {
    Serial.println("ERROR: could not save selected station.");
  }
  deleteConfirmation = false;
  if (saved) {
    setStatus("Station deleted");
  }
  if (deletedCurrentStation && stationCount > 0) {
    startStream();
  }
}

void adjustVolumeFromTouch(int16_t x) {
  const int16_t relativeX = constrain(x - UI_LEFT - 2, 0, UI_WIDTH - 4);
  const uint8_t newVolume =
      static_cast<uint8_t>((relativeX * 21 + (UI_WIDTH - 4) / 2) /
                           (UI_WIDTH - 4));
  setVolume(newVolume);
}

void handleHomeTouch(int16_t x, int16_t y) {
  if (pointInRect(x, y, UI_LEFT, 105, 49, 36)) {
    if (stationCount > 0) {
      selectStation((selectedStation + stationCount - 1) % stationCount);
    }
  } else if (pointInRect(x, y, UI_LEFT + 55, 105, 100, 36)) {
    if (streamActive) {
      radioAudio.stopSong();
      streamActive = false;
      setStatus("Paused");
    } else {
      startStream();
    }
  } else if (pointInRect(x, y, UI_LEFT + 161, 105, 49, 36)) {
    if (stationCount > 0) {
      selectStation((selectedStation + 1) % stationCount);
    }
  } else if (pointInRect(x, y, UI_LEFT - 1, 163, UI_WIDTH + 2, 19)) {
    adjustVolumeFromTouch(x);
  } else if (pointInRect(x, y, UI_LEFT, 190, 99, 36)) {
    currentScreen = Screen::Stations;
    deleteConfirmation = false;
  } else if (pointInRect(x, y, UI_LEFT + 105, 190, 105, 36)) {
    currentScreen = Screen::Settings;
  }
  markUiDirty();
}

void handleStationTouch(int16_t x, int16_t y) {
  if (deleteConfirmation) {
    if (pointInRect(x, y, UI_LEFT + 14, 136, 127, 25)) {
      deleteConfirmation = false;
    } else if (pointInRect(x, y, UI_LEFT + 147, 136, 127, 25)) {
      deleteStation(deleteIndex);
    }
    markUiDirty();
    return;
  }

  const uint8_t pageStart = stationListPage * STATIONS_PER_PAGE;
  const uint8_t visibleStations =
      min<uint8_t>(STATIONS_PER_PAGE, stationCount - pageStart);
  if (y >= 41 && y < 41 + visibleStations * 27) {
    const uint8_t row = (y - 41) / 27;
    const uint8_t index = stationListPage * STATIONS_PER_PAGE + row;
    if (row < STATIONS_PER_PAGE && index < stationCount) {
      if (x >= UI_LEFT + UI_WIDTH - 30) {
        deleteIndex = index;
        deleteConfirmation = true;
      } else if (x >= UI_LEFT + UI_WIDTH - 60) {
        startStationEditor(false, index);
        return;
      } else {
        selectStation(index);
        return;
      }
    }
  } else {
    const int16_t controlsWidth = 36 + 4 + 36 + 4 + 52 + 4 + 100 + 4 + 52;
    const int16_t controlsLeft = UI_LEFT + (UI_WIDTH - controlsWidth) / 2;
    if (pointInRect(x, y, controlsLeft, 204, 36, 25)) {
      if (stationListPage > 0) {
        --stationListPage;
      }
    } else if (pointInRect(x, y, controlsLeft + 40, 204, 36, 25)) {
      if (stationListPage + 1 < stationPageCount()) {
        ++stationListPage;
      }
    } else if (pointInRect(x, y, controlsLeft + 132, 204, 100, 25)) {
      if (stationCount >= MAX_STATIONS) {
        setStatus("Station list is full");
      } else {
        startStationEditor(true, stationCount);
        return;
      }
    } else if (pointInRect(x, y, controlsLeft + 236, 204, 52, 25)) {
      currentScreen = Screen::Home;
    }
  }
  markUiDirty();
}

void handleTouch(int16_t x, int16_t y) {
  updateScreenLayout();
  switch (currentScreen) {
    case Screen::Home:
      if (pointInRect(x, y, 0, 195, CAT_PANEL_WIDTH, 45)) {
        setAudioSpectrum(!audioSpectrumEnabled);
        return;
      }
      if (pointInRect(x, y, 0, 0, CAT_PANEL_WIDTH, 195)) {
        if (currentGenreAnimal == nullptr) {
          chooseDifferentFallbackPet();
          lastFallbackPetChangeAt = millis();
        }
        return;
      }
      handleHomeTouch(x, y);
      break;
    case Screen::Settings:
    {
      const int16_t toggleWidth = (UI_WIDTH - 6) / 2;
      if (pointInRect(x, y, UI_LEFT, 36, toggleWidth, 25)) {
        currentScreen = Screen::Wifi;
      } else if (pointInRect(x, y, UI_LEFT + toggleWidth + 6, 36,
                             toggleWidth, 25)) {
        currentScreen = Screen::Stations;
        deleteConfirmation = false;
      } else if (pointInRect(x, y, UI_LEFT, 63, toggleWidth, 24)) {
        setAudioBoost(!audioBoost);
      } else if (pointInRect(x, y, UI_LEFT + toggleWidth + 6, 63,
                             toggleWidth, 24)) {
        setAudioSpectrum(!audioSpectrumEnabled);
      } else if (pointInRect(x, y, UI_LEFT, 90, toggleWidth, 24)) {
        setAudioCompressor(!audioCompressorEnabled);
      } else if (pointInRect(x, y, UI_LEFT + toggleWidth + 6, 90,
                             toggleWidth, 24)) {
        setAudioAmpEnabled(!audioAmpEnabled);
      } else if (pointInRect(x, y, UI_LEFT, 117, toggleWidth, 22)) {
        calibrateTouchscreen();
      } else if (pointInRect(x, y, UI_LEFT + toggleWidth + 6, 117,
                             toggleWidth, 22)) {
        setAudioNoiseShaping(!audioNoiseShaping);
      } else if (pointInRect(x, y, UI_LEFT, 187, UI_WIDTH,
                             themeSwatchHeight + 2)) {
        for (uint8_t index = 0; index < THEME_COLOR_COUNT; ++index) {
          if (pointInRect(x, y, themeSwatchX(index) - 1, 187,
                          themeSwatchWidth + 2, themeSwatchHeight + 2)) {
            setThemeColor(index);
            break;
          }
        }
      } else if (pointInRect(x, y, UI_LEFT, 159, UI_WIDTH, 18)) {
        adjustVolumeFromTouch(x);
      } else if (pointInRect(x, y, UI_LEFT, 213, UI_WIDTH, 23)) {
        currentScreen = Screen::Home;
      }
      markUiDirty();
      break;
    }
    case Screen::Wifi:
      if (pointInRect(x, y, UI_LEFT, 54, UI_WIDTH, 34)) {
        beginKeyboard(KeyboardTarget::Ssid);
      } else if (pointInRect(x, y, UI_LEFT, 111, UI_WIDTH, 34)) {
        beginKeyboard(KeyboardTarget::Password);
      } else if (pointInRect(x, y, UI_LEFT, 151, UI_WIDTH, 18)) {
        showWifiPassword = !showWifiPassword;
        markUiDirty();
      } else if (pointInRect(x, y, UI_LEFT, 176, (UI_WIDTH - 8) / 2, 32)) {
        if (saveWifiCredentials()) {
          beginWifiConnection();
        }
      } else if (pointInRect(x, y, UI_LEFT + (UI_WIDTH - 8) / 2 + 8, 176,
                             (UI_WIDTH - 8) / 2, 32)) {
        currentScreen = Screen::Settings;
        markUiDirty();
      }
      break;
    case Screen::Stations:
      handleStationTouch(x, y);
      break;
    case Screen::StationEditor:
      if (pointInRect(x, y, UI_LEFT, 54, UI_WIDTH, 32)) {
        beginKeyboard(KeyboardTarget::StationName);
      } else if (pointInRect(x, y, UI_LEFT, 108, UI_WIDTH, 32)) {
        beginKeyboard(KeyboardTarget::StationUrl);
      } else if (pointInRect(x, y, UI_LEFT, 159, (UI_WIDTH - 8) / 2, 36)) {
        saveStationEditor();
      } else if (pointInRect(x, y, UI_LEFT + (UI_WIDTH - 8) / 2 + 8, 159,
                             (UI_WIDTH - 8) / 2, 36)) {
        currentScreen = Screen::Stations;
        markUiDirty();
      }
      break;
    case Screen::Keyboard:
      handleKeyboardTouch(x, y);
      break;
  }
}

}  // namespace

void audio_info(const char *info) {
  Serial.printf("audio: %s\n", info);
}

void audio_showstation(const char *info) {
  streamStation = info;
  updateGenreAnimal();
  audioDirty = true;
  Serial.printf("station: %s\n", info);
}

void audio_showstreamtitle(const char *info) {
  streamTitle = info;
  updateGenreAnimal();
  audioDirty = true;
  Serial.printf("stream title: %s\n", info);
}

void audio_eof_stream(const char *info) {
  streamActive = false;
  streamRestartPending = true;
  streamRestartAt = millis();
  setStatus("Stream ended");
  Serial.printf("audio: stream ended (%s)\n", info);
}

void setup() {
  Serial.begin(115200);
  display.init();
  display.setRotation(1);
  display.fillScreen(TFT_BLACK);
  screenFrameBuffer.setColorDepth(8);
  if (!screenFrameBuffer.createSprite(DISPLAY_WIDTH, DISPLAY_HEIGHT)) {
    Serial.printf("ERROR: could not allocate %u-byte screen frame buffer; "
                  "%u bytes of heap remain.\n",
                  static_cast<unsigned>(DISPLAY_WIDTH * DISPLAY_HEIGHT),
                  static_cast<unsigned>(ESP.getFreeHeap()));
    display.fillScreen(TFT_RED);
    display.setTextColor(TFT_WHITE, TFT_RED);
    display.drawCentreString("Frame buffer unavailable", DISPLAY_WIDTH / 2,
                             DISPLAY_HEIGHT / 2, 2);
    while (true) {
      delay(1000);
    }
  }

  touchSPI.begin(25, 39, 32, 33);
  if (!touch.begin(touchSPI)) {
    Serial.println("ERROR: XPT2046 touch controller initialization failed.");
  }
  touch.setRotation(1);

  if (!preferences.begin("pet-radio", false)) {
    Serial.println("ERROR: could not open preferences storage.");
  }
  wifiSsid = preferences.getString("ssid", "");
  wifiPassword = preferences.getString("password", "");
  volume = min<uint8_t>(preferences.getUChar("volume", 10), 21);
  audioBoost = preferences.getBool("boost", false);
  audioAmpEnabled = preferences.getBool("amp4", false);
  audioSpectrumEnabled = preferences.getBool("spectrum", true);
  audioCompressorEnabled = preferences.getBool("compressor", false);
  setAudioCompressorEnabled(audioCompressorEnabled);
  audioNoiseShaping = preferences.getBool("noiseshape", false);
  setAudioNoiseShapingEnabled(audioNoiseShaping);
  digitalWrite(AUDIO_AMP_ENABLE_PIN, audioAmpEnabled ? LOW : HIGH);
  pinMode(AUDIO_AMP_ENABLE_PIN, OUTPUT);
  themeColorIndex = min<uint8_t>(
      preferences.getUChar("theme", DEFAULT_THEME_COLOR),
      THEME_COLOR_COUNT - 1);
  backgroundColor = THEME_COLORS[themeColorIndex];
  display.fillScreen(backgroundColor);
  setAudioSpectrumEnabled(audioSpectrumEnabled);
  selectedStation = preferences.getUChar("selected", 0);
  loadStations();
  if (selectedStation >= stationCount) {
    selectedStation = 0;
  }

  touchCalibration.xLeft = preferences.getUShort("xLeft", 0);
  touchCalibration.xRight = preferences.getUShort("xRight", 0);
  touchCalibration.yTop = preferences.getUShort("yTop", 0);
  touchCalibration.yBottom = preferences.getUShort("yBottom", 0);
  if (abs(static_cast<int>(touchCalibration.xRight) -
          touchCalibration.xLeft) < 500 ||
      abs(static_cast<int>(touchCalibration.yBottom) -
          touchCalibration.yTop) < 500) {
    calibrateTouchscreen();
  }

  radioAudio.forceMono(true);
  const int8_t audioGain = audioBoost ? AUDIO_BOOST_DB : 0;
  radioAudio.setTone(audioGain, audioGain, audioGain);
  radioAudio.setVolume(volume);
  WiFi.mode(WIFI_STA);
  configTzTime(BERLIN_TIMEZONE, "pool.ntp.org", "time.google.com");
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
      wifiDisconnectReason = info.wifi_sta_disconnected.reason;
      wifiDisconnectReasonPending = true;
    }
  });
  if (!wifiSsid.isEmpty()) {
    startWifiConnection();
  } else {
    statusMessage = "Set up Wi-Fi";
  }
  lastFallbackPetChangeAt = millis();
  markUiDirty();
}

void loop() {
  radioAudio.loop();
  updateNetwork();

  int16_t x = 0;
  int16_t y = 0;
  if (readTouchPoint(x, y)) {
    handleTouch(x, y);
  }

  const uint32_t now = millis();
  if (streamRestartPending && WiFi.status() == WL_CONNECTED &&
      stationCount > 0 &&
      now - streamRestartAt >= STREAM_RETRY_DELAY_MS) {
    Serial.println("audio: retrying ended stream.");
    startStream();
  }
  if (currentGenreAnimal == nullptr &&
      now - lastFallbackPetChangeAt >= RANDOM_PET_INTERVAL_MS) {
    chooseDifferentFallbackPet();
    lastFallbackPetChangeAt = now;
  }
  updatePetAnimation(now);
  updateBubbleText(now);
  if (currentScreen == Screen::Home &&
      now - lastClockUpdateAt >= CLOCK_UPDATE_INTERVAL_MS) {
    lastClockUpdateAt = now;
    markUiDirty();
  }
  if (currentScreen == Screen::Home &&
      now - lastBitrateUpdateAt >= BITRATE_UPDATE_INTERVAL_MS) {
    lastBitrateUpdateAt = now;
    const uint32_t bitrateKbps =
        WiFi.status() == WL_CONNECTED && streamActive
            ? (radioAudio.getBitRate() + 500) / 1000
            : 0;
    if (bitrateKbps != lastDisplayedBitrateKbps) {
      lastDisplayedBitrateKbps = bitrateKbps;
      markUiDirty();
    }
  }
  if (currentScreen == Screen::Home &&
      now - lastCatFrameAt >= CAT_FRAME_INTERVAL_MS) {
    lastCatFrameAt = now;
    if (uiDirty) {
      renderScreen();
    } else {
      drawCatFrame(now);
    }
  } else if (uiDirty) {
    renderScreen();
  }
  if (audioDirty) {
    audioDirty = false;
    markUiDirty();
  }
  delay(1);
}
