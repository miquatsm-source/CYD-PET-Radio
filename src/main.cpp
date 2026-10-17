#include <Arduino.h>
#include <Preferences.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <WiFi.h>
#include <Audio.h>

#include "fat_cat.h"

namespace {

constexpr int16_t DISPLAY_WIDTH = 320;
constexpr int16_t DISPLAY_HEIGHT = 240;
constexpr int16_t CAT_PANEL_WIDTH = FAT_CAT_PANEL_WIDTH;
constexpr int16_t UI_LEFT = 103;
constexpr int16_t UI_WIDTH = 210;
constexpr uint8_t MAX_STATIONS = 6;
constexpr uint8_t STATION_NAME_SIZE = 25;
constexpr uint8_t STATION_URL_SIZE = 192;
constexpr uint16_t CAT_FRAME_INTERVAL_MS = FAT_CAT_FRAME_INTERVAL_MS;
constexpr uint16_t TOUCH_DEBOUNCE_MS = 180;
constexpr uint32_t WIFI_TIMEOUT_MS = 25000;

constexpr uint16_t BACKGROUND = 0x1082;
constexpr uint16_t PANEL = 0x18E3;
constexpr uint16_t PANEL_LIGHT = 0x2945;
constexpr uint16_t ACCENT = 0x05BF;
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

TFT_eSPI display;
TFT_eSprite catFrameBuffer(&display);
SPIClass touchSPI(HSPI);
XPT2046_Touchscreen touch(33, 36);
Preferences preferences;
// DAC channel 2 is GPIO 26, the CYD speaker output.
Audio radioAudio(true, I2S_DAC_CHANNEL_LEFT_EN);

Station stations[MAX_STATIONS]{};
uint8_t stationCount = 0;
uint8_t selectedStation = 0;
uint8_t volume = 10;
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
String statusMessage = "Wi-Fi einrichten";

bool touchHeld = false;
bool wifiConnecting = false;
bool playWhenConnected = false;
bool streamActive = false;
bool showWifiPassword = false;
bool deleteConfirmation = false;
bool stationEditorIsNew = false;
bool keyboardUppercase = false;
uint8_t keyboardPage = 0;
bool uiDirty = true;
bool audioDirty = false;
bool displayReady = false;
volatile uint8_t wifiDisconnectReason = 0;
volatile bool wifiDisconnectReasonPending = false;
uint32_t wifiConnectStartedAt = 0;
uint32_t lastTouchAt = 0;
uint32_t lastCatFrameAt = 0;

void startWifiConnection();

void drawCatFrame(uint32_t now) {
  drawFatCat(catFrameBuffer, now);
  catFrameBuffer.pushSprite(0, 0);
}

bool pointInRect(int16_t x, int16_t y, int16_t left, int16_t top,
                 int16_t width, int16_t height) {
  return x >= left && x < left + width && y >= top && y < top + height;
}

void markUiDirty() {
  uiDirty = true;
}

String fittedText(const String &source, int16_t maxWidth, uint8_t font = 2) {
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

void drawText(const String &text, int16_t x, int16_t y, uint8_t font = 2,
              uint16_t colour = TFT_WHITE) {
  display.setTextDatum(TL_DATUM);
  display.setTextColor(colour, BACKGROUND);
  display.drawString(text, x, y, font);
}

void drawButton(int16_t x, int16_t y, int16_t width, int16_t height,
                const String &label, uint16_t fill = PANEL_LIGHT,
                uint16_t colour = TFT_WHITE, uint8_t font = 2) {
  display.fillRoundRect(x, y, width, height, 7, fill);
  display.drawRoundRect(x, y, width, height, 7, 0x4208);
  display.setTextDatum(MC_DATUM);
  display.setTextColor(colour, fill);
  display.drawString(label, x + width / 2, y + height / 2, font);
}

void drawPanelHeader(const String &title) {
  display.fillRect(CAT_PANEL_WIDTH, 0, DISPLAY_WIDTH - CAT_PANEL_WIDTH,
                   DISPLAY_HEIGHT, BACKGROUND);
  display.setTextDatum(TL_DATUM);
  display.setTextColor(TFT_CYAN, BACKGROUND);
  display.drawString(title, UI_LEFT, 9, 2);
  display.drawFastHLine(UI_LEFT, 31, UI_WIDTH, 0x4208);
}

String wifiStatusText() {
  if (wifiConnecting) {
    return "Verbinde mit WLAN...";
  }
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.localIP().toString();
  }
  if (wifiSsid.isEmpty()) {
    return "WLAN nicht eingerichtet";
  }
  return statusMessage;
}

void drawHome() {
  drawPanelHeader("PET RADIO");
  const bool connected = WiFi.status() == WL_CONNECTED;
  display.fillCircle(UI_LEFT + 5, 43, 3, connected ? SUCCESS : ERROR_COLOUR);
  drawText(connected ? "WLAN verbunden" : wifiStatusText(), UI_LEFT + 13, 37,
           1, connected ? SUCCESS : TEXT_MUTED);

  display.fillRoundRect(UI_LEFT, 53, UI_WIDTH, 45, 7, PANEL);
  display.setTextDatum(TL_DATUM);
  display.setTextColor(TFT_WHITE, PANEL);
  const String stationName =
      stationCount == 0 ? "Keine Sender" : stations[selectedStation].name;
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
  display.setTextColor(TFT_WHITE, BACKGROUND);
  display.drawString("LAUTSTAERKE", UI_LEFT, 151, 1);
  display.setTextDatum(TR_DATUM);
  display.drawString(String(volume * 100 / 21) + "%", UI_LEFT + UI_WIDTH,
                     151, 1);
  display.fillRoundRect(UI_LEFT, 168, UI_WIDTH, 8, 4, PANEL_LIGHT);
  const int16_t sliderWidth =
      static_cast<int16_t>((UI_WIDTH - 4) * volume / 21);
  display.fillRoundRect(UI_LEFT + 2, 170, sliderWidth, 4, 2, ACCENT);
  display.fillCircle(UI_LEFT + 2 + sliderWidth, 172, 6, TFT_WHITE);

  drawButton(UI_LEFT, 190, 99, 36, "SENDER", PANEL_LIGHT);
  drawButton(UI_LEFT + 105, 190, 105, 36, "MENUE", PANEL_LIGHT);
}

void drawSettings() {
  drawPanelHeader("EINSTELLUNGEN");
  drawButton(UI_LEFT, 43, UI_WIDTH, 42, "WLAN", PANEL_LIGHT);
  drawButton(UI_LEFT, 94, UI_WIDTH, 42, "SENDERLISTE", PANEL_LIGHT);
  drawButton(UI_LEFT, 145, UI_WIDTH, 42, "ZURUECK", PANEL_LIGHT);
  display.setTextDatum(TL_DATUM);
  display.setTextColor(TEXT_MUTED, BACKGROUND);
  display.drawString("LAUTSTAERKE", UI_LEFT, 195, 1);
  display.setTextDatum(TR_DATUM);
  display.drawString(String(volume * 100 / 21) + "%", UI_LEFT + UI_WIDTH,
                     195, 1);
  display.fillRoundRect(UI_LEFT, 213, UI_WIDTH, 8, 4, PANEL_LIGHT);
  const int16_t sliderWidth =
      static_cast<int16_t>((UI_WIDTH - 4) * volume / 21);
  display.fillRoundRect(UI_LEFT + 2, 215, sliderWidth, 4, 2, ACCENT);
  display.fillCircle(UI_LEFT + 2 + sliderWidth, 217, 6, TFT_WHITE);
}

void drawWifiSettings() {
  drawPanelHeader("WLAN EINSTELLEN");
  display.setTextDatum(TL_DATUM);
  display.setTextColor(TEXT_MUTED, BACKGROUND);
  display.drawString("NETZWERKNAME (SSID)", UI_LEFT, 40, 1);
  drawButton(UI_LEFT, 54, UI_WIDTH, 34,
             fittedText(wifiSsid.isEmpty() ? "(SSID eingeben)" : wifiSsid,
                        UI_WIDTH - 16, 1),
             PANEL);
  display.setTextColor(TEXT_MUTED, BACKGROUND);
  display.drawString("PASSWORT", UI_LEFT, 97, 1);
  String passwordPreview = passwordForDisplay(wifiPassword);
  if (wifiPassword.isEmpty()) {
    passwordPreview = "(Passwort eingeben)";
  }
  drawButton(UI_LEFT, 111, UI_WIDTH, 34,
             fittedText(passwordPreview, UI_WIDTH - 16, 1), PANEL);

  drawButton(UI_LEFT, 151, UI_WIDTH, 18,
             String("PASSWORT ANZEIGEN: ") + (showWifiPassword ? "AN" : "AUS"),
             showWifiPassword ? 0x2147 : PANEL, TFT_WHITE, 1);
  drawButton(UI_LEFT, 176, 101, 32, "VERBINDEN",
             wifiConnecting ? 0x3186 : ACCENT, TFT_WHITE, 1);
  drawButton(UI_LEFT + 109, 176, 101, 32, "ZURUECK", PANEL_LIGHT, TFT_WHITE,
             1);
  drawText(fittedText(wifiStatusText(), UI_WIDTH, 1), UI_LEFT, 216, 1,
           WiFi.status() == WL_CONNECTED ? SUCCESS : TEXT_MUTED);
}

void drawStations() {
  drawPanelHeader("SENDERLISTE");
  drawText(fittedText(statusMessage, UI_WIDTH, 1), UI_LEFT, 32, 1,
           TEXT_MUTED);
  if (deleteConfirmation) {
    for (uint8_t i = 0; i < stationCount; ++i) {
      const int16_t rowY = 41 + i * 27;
      drawButton(UI_LEFT, rowY, UI_WIDTH, 24,
                 fittedText(stations[i].name, UI_WIDTH - 10, 1),
                 i == selectedStation ? 0x2147 : PANEL, TFT_WHITE, 1);
    }
    display.fillRoundRect(UI_LEFT + 9, 83, UI_WIDTH - 18, 83, 8, PANEL_LIGHT);
    display.drawRoundRect(UI_LEFT + 9, 83, UI_WIDTH - 18, 83, 8, TFT_CYAN);
    display.setTextDatum(MC_DATUM);
    display.setTextColor(TFT_WHITE, PANEL_LIGHT);
    display.drawString("Sender loeschen?", 208, 101, 2);
    display.drawString(
        fittedText(stations[deleteIndex].name, 170, 1), 208, 124, 1);
    drawButton(UI_LEFT + 14, 136, 88, 25, "ABBRECHEN", PANEL, TFT_WHITE, 1);
    drawButton(UI_LEFT + 108, 136, 88, 25, "LOESCHEN", ERROR_COLOUR,
               TFT_WHITE, 1);
    return;
  }

  for (uint8_t i = 0; i < stationCount; ++i) {
    const int16_t rowY = 41 + i * 27;
    const uint16_t fill = i == selectedStation ? 0x2147 : PANEL;
    display.fillRoundRect(UI_LEFT, rowY, UI_WIDTH, 24, 5, fill);
    display.setTextDatum(ML_DATUM);
    display.setTextColor(TFT_WHITE, fill);
    display.drawString(fittedText(stations[i].name, 130, 1), UI_LEFT + 7,
                       rowY + 12, 1);
    display.setTextDatum(MC_DATUM);
    display.setTextColor(TEXT_MUTED, fill);
    display.drawString("E", UI_LEFT + 165, rowY + 12, 1);
    display.drawString("x", UI_LEFT + 195, rowY + 12, 2);
  }
  drawButton(UI_LEFT, 204, 101, 25, "+ HINZUFUEGEN", ACCENT, TFT_WHITE, 1);
  drawButton(UI_LEFT + 109, 204, 101, 25, "ZURUECK", PANEL_LIGHT, TFT_WHITE,
             1);
}

void drawStationEditor() {
  drawPanelHeader(stationEditorIsNew ? "NEUER SENDER" : "SENDER AENDERN");
  display.setTextDatum(TL_DATUM);
  display.setTextColor(TEXT_MUTED, BACKGROUND);
  display.drawString("NAME", UI_LEFT, 40, 1);
  drawButton(UI_LEFT, 54, UI_WIDTH, 32,
             fittedText(editName, UI_WIDTH - 14, 1), PANEL);
  display.setTextColor(TEXT_MUTED, BACKGROUND);
  display.drawString("STREAM-URL", UI_LEFT, 94, 1);
  drawButton(UI_LEFT, 108, UI_WIDTH, 32,
             fittedText(editUrl, UI_WIDTH - 14, 1), PANEL);
  drawButton(UI_LEFT, 159, 101, 36, "SPEICHERN", ACCENT, TFT_WHITE, 1);
  drawButton(UI_LEFT + 109, 159, 101, 36, "ZURUECK", PANEL_LIGHT, TFT_WHITE,
             1);
  drawText("MP3/AAC-Stream (http/https)", UI_LEFT, 210, 1, TEXT_MUTED);
}

void drawKeyboard() {
  drawPanelHeader(keyboardTarget == KeyboardTarget::Ssid
                      ? "SSID EINGEBEN"
                  : keyboardTarget == KeyboardTarget::Password
                      ? "PASSWORT EINGEBEN"
                  : keyboardTarget == KeyboardTarget::StationName
                      ? "SENDERNAME EINGEBEN"
                      : "STREAM-URL EINGEBEN");
  const bool editingPassword = keyboardTarget == KeyboardTarget::Password;
  const String visibleValue =
      editingPassword ? passwordForDisplay(keyboardValue) : keyboardValue;
  const int16_t valueWidth = editingPassword && !showWifiPassword
                                 ? UI_WIDTH - 53
                                 : UI_WIDTH;
  drawText(fittedText(visibleValue, valueWidth, 1), UI_LEFT, 40, 1, TFT_WHITE);
  if (editingPassword) {
    drawButton(UI_LEFT + UI_WIDTH - 48, 36, 48, 19,
               showWifiPassword ? "VERB." : "ANZEIG.", PANEL_LIGHT,
               TFT_WHITE, 1);
  }
  display.drawFastHLine(UI_LEFT, 58, UI_WIDTH, 0x4208);

  for (uint8_t row = 0; row < 3; ++row) {
    const String keys = keyboardRow(row);
    if (keys.isEmpty()) {
      continue;
    }
    const int16_t keyWidth = min<int16_t>(
        32, (UI_WIDTH - (keys.length() - 1)) / keys.length());
    const int16_t rowWidth =
        keys.length() * keyWidth + keys.length() - 1;
    const int16_t rowLeft = UI_LEFT + (UI_WIDTH - rowWidth) / 2;
    for (uint8_t column = 0; column < keys.length(); ++column) {
      const int16_t x = rowLeft + column * (keyWidth + 1);
      const int16_t y = 66 + row * 29;
      String label(keys[column]);
      if (keyboardPage == 0 && keyboardUppercase) {
        label.toUpperCase();
      }
      drawButton(x, y, keyWidth, 25, label, PANEL_LIGHT, TFT_WHITE, 2);
    }
  }

  drawButton(UI_LEFT, 156, 39, 24,
             keyboardPage == 0 && !keyboardUppercase ? "abc" : "ABC",
             PANEL_LIGHT, TFT_WHITE, 1);
  drawButton(UI_LEFT + 43, 156, 48, 24,
             keyboardPage == 0 ? "?123"
                               : keyboardPage == 1 ? "SYM2" : "SYM1",
             PANEL_LIGHT, TFT_WHITE, 1);
  drawButton(UI_LEFT + 95, 156, 66, 24, "LEER", PANEL_LIGHT, TFT_WHITE, 1);
  drawButton(UI_LEFT + 165, 156, 45, 24, "<-", PANEL_LIGHT, TFT_WHITE, 1);
  drawButton(UI_LEFT, 192, 98, 31, "ABBRUCH", PANEL_LIGHT, TFT_WHITE, 1);
  drawButton(UI_LEFT + 106, 192, 104, 31, "FERTIG", ACCENT, TFT_WHITE, 1);
}

void renderScreen() {
  drawCatFrame(millis());
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
  uiDirty = false;
}

void drawCalibrationTarget(uint8_t index) {
  const int16_t targetX[] = {20, DISPLAY_WIDTH - 21, 20, DISPLAY_WIDTH - 21};
  const int16_t targetY[] = {20, 20, DISPLAY_HEIGHT - 21, DISPLAY_HEIGHT - 21};
  display.fillScreen(BACKGROUND);
  display.setTextDatum(TL_DATUM);
  display.setTextColor(TFT_WHITE, BACKGROUND);
  display.drawString("Touch-Kalibrierung", 24, 72, 2);
  display.drawString("Ziel beruehren " + String(index + 1) + "/4", 24, 101,
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
    display.drawCentreString("Kalibrierung fehlgeschlagen", DISPLAY_WIDTH / 2,
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
    setStatus("Fehler beim Speichern der Sender");
    Serial.println("ERROR: could not persist the station list.");
  }
  return saved;
}

void loadStations() {
  stationCount = min<uint8_t>(preferences.getUChar("count", 0), MAX_STATIONS);
  if (!preferences.isKey("count")) {
    const char *defaultNames[] = {"Radio Paradise", "SomaFM Groove Salad",
                                  "Deutschlandfunk"};
    const char *defaultUrls[] = {
        "http://stream.radioparadise.com/mp3-128",
        "https://ice1.somafm.com/groovesalad-128-mp3",
        "https://st01.sslstream.dlf.de/dlf/01/128/mp3/stream.mp3"};
    stationCount = 3;
    for (uint8_t i = 0; i < stationCount; ++i) {
      strlcpy(stations[i].name, defaultNames[i], sizeof(stations[i].name));
      strlcpy(stations[i].url, defaultUrls[i], sizeof(stations[i].url));
    }
    saveStationList();
  } else {
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
  }
  if (stationCount == 0) {
    setStatus("Keine Sender vorhanden");
  }
}

void setVolume(uint8_t newVolume) {
  volume = min<uint8_t>(newVolume, 21);
  radioAudio.setVolume(volume);
  if (!saveVolume()) {
    setStatus("Lautstaerke nicht gespeichert");
  } else {
    markUiDirty();
  }
}

void startStream() {
  if (stationCount == 0) {
    setStatus("Keine Sender vorhanden");
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    setStatus(wifiSsid.isEmpty() ? "Wi-Fi einrichten" : "WLAN nicht verbunden");
    return;
  }

  streamTitle = "";
  streamStation = "";
  radioAudio.stopSong();
  radioAudio.setVolume(volume);
  if (!radioAudio.connecttohost(stations[selectedStation].url)) {
    streamActive = false;
    setStatus("Stream konnte nicht starten");
    Serial.printf("ERROR: could not connect to station %u.\n",
                  selectedStation + 1);
    return;
  }
  streamActive = true;
  setStatus("Stream verbindet...");
}

void selectStation(uint8_t index) {
  if (index >= stationCount) {
    return;
  }
  selectedStation = index;
  if (preferences.putUChar("selected", selectedStation) == 0) {
    Serial.println("ERROR: could not save selected station.");
  }
  currentScreen = Screen::Home;
  if (WiFi.status() == WL_CONNECTED) {
    startStream();
  } else if (!wifiSsid.isEmpty()) {
    startWifiConnection();
  } else {
    setStatus("Wi-Fi einrichten");
  }
  markUiDirty();
}

void beginWifiConnection() {
  if (wifiSsid.isEmpty()) {
    setStatus("Bitte zuerst eine SSID eingeben");
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
    setStatus("WLAN-Daten konnten nicht gespeichert werden");
    return false;
  }
  return true;
}

void startWifiConnection() {
  if (wifiPassword.length() < 8 || wifiPassword.length() > 63) {
    wifiConnecting = false;
    playWhenConnected = false;
    setStatus("WPA2-Passwort muss 8-63 Zeichen haben");
    return;
  }

  wifiDisconnectReason = 0;
  wifiDisconnectReasonPending = false;
  wifiConnecting = true;
  playWhenConnected = true;
  wifiConnectStartedAt = millis();
  setStatus("Verbinde mit WLAN...");

  const wl_status_t result =
      WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  if (result == WL_CONNECT_FAILED) {
    wifiConnecting = false;
    playWhenConnected = false;
    setStatus("WLAN-Start fehlgeschlagen");
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
    return "SSID nicht gefunden (2,4-GHz-Netz pruefen)";
  }
  if (reason == WIFI_REASON_AUTH_FAIL ||
      reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
      reason == WIFI_REASON_HANDSHAKE_TIMEOUT ||
      status == WL_CONNECT_FAILED) {
    return "Anmeldung fehlgeschlagen: WPA2-Passwort pruefen";
  }
  if (reason == WIFI_REASON_ASSOC_FAIL) {
    return "Router lehnt die Anmeldung ab";
  }
  if (status == WL_CONNECTION_LOST) {
    return "WLAN-Verbindung abgebrochen";
  }
  return "WLAN-Fehler " + String(reason) + " (Status " +
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

  if (WiFi.status() == WL_CONNECTED) {
    if (wifiConnecting) {
      wifiConnecting = false;
      wifiDisconnectReason = 0;
      setStatus("WLAN verbunden");
      if (playWhenConnected) {
        playWhenConnected = false;
        startStream();
      }
    }
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
  if (keyboardTarget == KeyboardTarget::Password &&
      pointInRect(x, y, UI_LEFT + UI_WIDTH - 48, 36, 48, 19)) {
    showWifiPassword = !showWifiPassword;
    markUiDirty();
    return;
  }

  for (uint8_t row = 0; row < 3; ++row) {
    const String keys = keyboardRow(row);
    if (keys.isEmpty()) {
      continue;
    }
    const int16_t keyWidth = min<int16_t>(
        32, (UI_WIDTH - (keys.length() - 1)) / keys.length());
    const int16_t rowWidth =
        keys.length() * keyWidth + keys.length() - 1;
    const int16_t rowLeft = UI_LEFT + (UI_WIDTH - rowWidth) / 2;
    for (uint8_t column = 0; column < keys.length(); ++column) {
      const int16_t keyX = rowLeft + column * (keyWidth + 1);
      const int16_t keyY = 66 + row * 29;
      if (pointInRect(x, y, keyX, keyY, keyWidth, 25)) {
        char value = keys[column];
        if (keyboardPage == 0 && keyboardUppercase) {
          value = static_cast<char>(value - 'a' + 'A');
        }
        appendKeyboardCharacter(value);
        return;
      }
    }
  }

  if (pointInRect(x, y, UI_LEFT, 156, 39, 24)) {
    if (keyboardPage == 0) {
      keyboardUppercase = !keyboardUppercase;
    } else {
      keyboardPage = 0;
    }
  } else if (pointInRect(x, y, UI_LEFT + 43, 156, 48, 24)) {
    keyboardPage = keyboardPage == 0 ? 1 : keyboardPage == 1 ? 2 : 1;
  } else if (pointInRect(x, y, UI_LEFT + 95, 156, 66, 24)) {
    appendKeyboardCharacter(' ');
  } else if (pointInRect(x, y, UI_LEFT + 165, 156, 45, 24)) {
    if (!keyboardValue.isEmpty()) {
      keyboardValue.remove(keyboardValue.length() - 1);
    }
  } else if (pointInRect(x, y, UI_LEFT, 192, 98, 31)) {
    finishKeyboard(false);
    return;
  } else if (pointInRect(x, y, UI_LEFT + 106, 192, 104, 31)) {
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
    setStatus("Name und gueltige http(s)-URL eingeben");
    return;
  }

  const uint8_t index = stationEditorIsNew ? stationCount : editorIndex;
  const bool changedPlayingStation =
      !stationEditorIsNew && streamActive && index == selectedStation;
  if (index >= MAX_STATIONS) {
    setStatus("Senderliste ist voll");
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
    setStatus("Sender gespeichert");
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
  if (stationCount == 0) {
    selectedStation = 0;
    streamActive = false;
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
    setStatus("Sender geloescht");
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
      setStatus("Pausiert");
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
    if (pointInRect(x, y, UI_LEFT + 14, 136, 88, 25)) {
      deleteConfirmation = false;
    } else if (pointInRect(x, y, UI_LEFT + 108, 136, 88, 25)) {
      deleteStation(deleteIndex);
    }
    markUiDirty();
    return;
  }

  if (y >= 41 && y < 41 + stationCount * 27) {
    const uint8_t index = (y - 41) / 27;
    if (index < stationCount) {
      if (x >= UI_LEFT + 182) {
        deleteIndex = index;
        deleteConfirmation = true;
      } else if (x >= UI_LEFT + 145) {
        startStationEditor(false, index);
        return;
      } else {
        selectStation(index);
        return;
      }
    }
  } else if (pointInRect(x, y, UI_LEFT, 204, 101, 25)) {
    if (stationCount >= MAX_STATIONS) {
      setStatus("Senderliste ist voll");
    } else {
      startStationEditor(true, stationCount);
      return;
    }
  } else if (pointInRect(x, y, UI_LEFT + 109, 204, 101, 25)) {
    currentScreen = Screen::Home;
  }
  markUiDirty();
}

void handleTouch(int16_t x, int16_t y) {
  switch (currentScreen) {
    case Screen::Home:
      handleHomeTouch(x, y);
      break;
    case Screen::Settings:
      if (pointInRect(x, y, UI_LEFT, 43, UI_WIDTH, 42)) {
        currentScreen = Screen::Wifi;
      } else if (pointInRect(x, y, UI_LEFT, 94, UI_WIDTH, 42)) {
        currentScreen = Screen::Stations;
        deleteConfirmation = false;
      } else if (pointInRect(x, y, UI_LEFT, 145, UI_WIDTH, 42)) {
        currentScreen = Screen::Home;
      } else if (pointInRect(x, y, UI_LEFT, 210, UI_WIDTH, 18)) {
        adjustVolumeFromTouch(x);
      }
      markUiDirty();
      break;
    case Screen::Wifi:
      if (pointInRect(x, y, UI_LEFT, 54, UI_WIDTH, 34)) {
        beginKeyboard(KeyboardTarget::Ssid);
      } else if (pointInRect(x, y, UI_LEFT, 111, UI_WIDTH, 34)) {
        beginKeyboard(KeyboardTarget::Password);
      } else if (pointInRect(x, y, UI_LEFT, 151, UI_WIDTH, 18)) {
        showWifiPassword = !showWifiPassword;
        markUiDirty();
      } else if (pointInRect(x, y, UI_LEFT, 176, 101, 32)) {
        if (saveWifiCredentials()) {
          beginWifiConnection();
        }
      } else if (pointInRect(x, y, UI_LEFT + 109, 176, 101, 32)) {
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
      } else if (pointInRect(x, y, UI_LEFT, 159, 101, 36)) {
        saveStationEditor();
      } else if (pointInRect(x, y, UI_LEFT + 109, 159, 101, 36)) {
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
  audioDirty = true;
  Serial.printf("station: %s\n", info);
}

void audio_showstreamtitle(const char *info) {
  streamTitle = info;
  audioDirty = true;
  Serial.printf("stream title: %s\n", info);
}

void setup() {
  Serial.begin(115200);
  display.init();
  display.setRotation(1);
  display.fillScreen(TFT_BLACK);
  catFrameBuffer.setColorDepth(8);
  if (!catFrameBuffer.createSprite(FAT_CAT_PANEL_WIDTH, DISPLAY_HEIGHT)) {
    Serial.printf("ERROR: could not allocate %u-byte cat frame buffer; "
                  "%u bytes of heap remain.\n",
                  static_cast<unsigned>(FAT_CAT_PANEL_WIDTH * DISPLAY_HEIGHT),
                  static_cast<unsigned>(ESP.getFreeHeap()));
    display.fillScreen(TFT_RED);
    display.setTextColor(TFT_WHITE, TFT_RED);
    display.drawCentreString("Katzenpuffer fehlt", DISPLAY_WIDTH / 2,
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
  radioAudio.setVolume(volume);
  WiFi.mode(WIFI_STA);
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
    statusMessage = "Wi-Fi einrichten";
  }
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
  if (now - lastCatFrameAt >= CAT_FRAME_INTERVAL_MS) {
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
