# CYD PET Radio

A touchscreen webradio for the ESP32-based CYD (Cheap Yellow Display), using
the same basic board configuration as
[CYD of the Dead](https://github.com/miquatsm-source/CYD-of-the-dead). A
round, animated cat dances in the left 30% of the display while radio controls
fill the right side.

## Hardware configuration

- ESP32 CYD with a 2.8-inch, 240 x 320 ST7789 display
- XPT2046 touchscreen
- Display and touch pin configuration in `include/cyd_user_setup.h`
- Built-in ESP32 DAC on GPIO 26 for the CYD speaker output; audio is mixed to
  mono for the single speaker
- Wi-Fi credentials and station list are saved in ESP32 non-volatile storage
- PlatformIO environments `esp32dev` (40 MHz display SPI) and
  `esp32dev-fast` (80 MHz display SPI)

The fast environment uses an overclock and may be unstable on some boards.
Use `esp32dev` if the display shows artifacts.

## Radio controls

On first boot, touch the four calibration targets. Open **MENUE > WLAN** to
enter the Wi-Fi network name and password with the touchscreen keyboard, then
tap **VERBINDEN**. The radio starts its selected station once Wi-Fi connects.
The home screen provides station navigation, play/pause, and a touchscreen
volume slider.

The Wi-Fi connection scans every 2.4 GHz channel and reports whether the SSID
was not found or WPA2 authentication failed. If the connection still fails,
open the PlatformIO serial monitor at 115200 baud; diagnostic output includes
the Wi-Fi disconnect reason without printing the password. WPA2 Personal
passphrases must contain 8–63 characters.

Open **SENDER** or **MENUE > SENDERLISTE** to play a station, edit its name or
stream URL, add a station, or remove one. The list starts with Radio Paradise,
SomaFM Groove Salad, and Deutschlandfunk; replace these with streams available
in your region. Station URLs must be direct `http://` or `https://` MP3/AAC
streams supported by the audio decoder.

The audio decoder is pinned to
[ESP32-audioI2S 2.0.6](https://github.com/schreibfaul1/ESP32-audioI2S/tree/2.0.6)
because this release supports the classic ESP32's built-in DAC. The decoder
is licensed under GPL-3.0; check its license and the requirements that apply
when distributing firmware.

The radio playback flow and stream-metadata handling take inspiration from
[yoRadio](https://github.com/e2002/yoradio); the touchscreen interface,
settings, and cat animation are implemented for this CYD project.

## Install from a browser

After enabling GitHub Pages for this repository with **Settings > Pages >
Build and deployment > GitHub Actions**, install the latest firmware from the
[web flasher](https://miquatsm-source.github.io/CYD-PET-Radio/). Every push
to `main` builds the firmware and deploys the flasher automatically.

Use a desktop browser that supports Web Serial, such as Chrome or Edge, and a
USB data cable. If the flasher offers to erase the device, erasing also
removes the saved Wi-Fi credentials, station list, volume, and touchscreen
calibration.

## Build and upload

Install Visual Studio Code with the PlatformIO IDE extension, then open this
project folder. Connect the CYD over USB and build or upload with PlatformIO,
or run:

```sh
pio run -e esp32dev-fast
pio run -e esp32dev-fast -t upload
```

The volume, Wi-Fi credentials, touchscreen calibration, and station list are
stored on the CYD and persist across reboots.
