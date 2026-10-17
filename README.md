# CYD PET Radio

> **AI disclosure:** The text of this README was created with AI assistance.

A touchscreen webradio for the ESP32-based CYD (Cheap Yellow Display), using
the same basic board configuration as
[CYD of the Dead](https://github.com/miquatsm-source/CYD-of-the-dead). A
round, animated cat dances in the left 30% of the display while radio controls
fill the right side. Both panels are rendered into 8-bit sprite buffers before
being transferred to the display to prevent partial-frame flicker. The lower
left panel shows a pulsing eight-band FFT spectrum from bass to treble, based
on the decoded audio samples.

![CYD PET Radio running on the CYD display](./src/CYD-PET-Radio.png)

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

On first boot, touch the four calibration targets. Open **MENU > WI-FI** to
enter the Wi-Fi network name and password with the touchscreen keyboard, then
tap **CONNECT**. The radio starts its selected station once Wi-Fi connects.
The home screen provides station navigation, play/pause, and a touchscreen
volume slider. The top-right corner shows the detected genre and the current
Europe/Berlin time and date. The clock synchronizes over the internet using
NTP; until synchronization, it shows placeholder dashes.

If playback is too quiet at 100%, open **MENU** and enable **AUDIO BOOST
+6 dB**. This applies digital equalizer gain and is saved across restarts.
Boost can cause distortion on loud audio; switch it off if playback clips.
The regular volume control still sets the decoder level independently.

The Wi-Fi connection scans every 2.4 GHz channel and reports whether the SSID
was not found or WPA2 authentication failed. If the connection still fails,
open the PlatformIO serial monitor at 115200 baud; diagnostic output includes
the Wi-Fi disconnect reason without printing the password. WPA2 Personal
passphrases must contain 8–63 characters.

Open **STATIONS** or **MENU > STATION LIST** to play a station, edit its name or
stream URL, add a station, or remove one. The list is paginated and supports up
to 15 stations. On a fresh setup, `src/playlist.csv` supplies the default
playlist at build time. It is tab-separated with station name and HTTP(S)
stream URL in the first two columns; later columns are ignored. The build
validates field sizes and URL format. Currently unreachable stations should be
removed or updated in the CSV before building.

The imported list has 14 stations: 1LIVE, Antenne Bayern, WDR 2, SWR3, WHTZ
Z100, KIIS-FM, KEXP, WFAN, WLTW, BBC Radio 1, BBC Radio 2, Capital FM, Heart,
and Absolute Radio. Radio Hamburg was left out because its supplied URL
returned HTTP 404 when checked. Freshly flashed devices get this playlist;
existing personalized lists are preserved. The previous built-in three-station
default is automatically migrated to the new list. Station URLs must be direct
`http://` or `https://` MP3/AAC streams supported by the audio decoder.

## Genre animals

The radio checks the configured station name and the station/title metadata
provided by the stream for genre keywords. A match selects the corresponding
dancing animal from `src/genres_animals.cpp`: peacock for Pop, lion for Rock,
kangaroo for Hip-Hop, chameleon for Techno, swan for classical, cat for Jazz,
rhino for Heavy Metal, sloth for Reggae, horse for Country, and budgie for
German Pop. If the metadata contains no recognized genre keyword, the default
dancing cat remains on screen. This is metadata-based matching, not acoustic
music recognition; many streams do not include genre information in their
station name or track titles.

The default cat is named Claudia. Genre animals are named Poldi (Pop), Leopold
(Rock), Hoppe (Hip-Hop), Egon (Techno), Ludwig (classical), Moritz (Jazz),
Siegfried (Heavy Metal), Willi (Reggae), Blitz (Country), and Hansi
(German Pop). The animal's name appears at the top of the left panel. If no genre
can be identified, Claudia is shown first and a different randomly selected
pet appears every 30 seconds until genre metadata is available.
Each pet also displays a random genre-themed English quip in a speech bubble.
Each bubble stays visible for 2 seconds, with a randomized 5-to-15-second
interval between appearances.
Pets use three randomly selected dance animations that change during playback.
When the displayed pet changes, the previous one runs out and the next runs in.
Each appearance also gets a randomly selected outfit: a tie, sunglasses, boots,
or a glitter jacket.
The 8-band audio spectrum updates four times per second to reduce processing
load and can be switched on or off by tapping the spectrum in the lower-left
corner or through the settings menu; the selection is saved.
To leave more internal RAM for MP3 decoding, the pet is drawn directly into the
full-screen frame buffer instead of a second sprite; the decoder uses its
default input-buffer size. If a stream reports end-of-file, the radio retries
it after a short delay. Date and time are shown in compact `DD.MM HH:MM` format;
NTP synchronization is also requested after Wi-Fi connects.

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
