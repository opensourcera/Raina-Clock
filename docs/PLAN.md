# CYD Alarm Clock: project plan

Goal: an alarm clock for the Cheap Yellow Display (ESP32-2432S028R) that wakes you with music, shows a clean clock,
the weather for 30062 (Marietta, GA), and a to-do list.

## What exists online (October 2026)

No single open source firmware does all of this on the CYD. The closest projects:

| Project | Has | Missing | License |
|---|---|---|---|
| nicholaswilde/cyd-weather-station | Clock, weather by zip code (Open-Meteo, no key), 3-day and hourly forecast, Catppuccin theme, LVGL 8, web settings, OTA, SD card, captive portal Wi-Fi, PlatformIO, tests, active (last push Oct 2026) | Alarm, music, to-do | Apache-2.0 |
| Garionhk/CYD-2.8-Wifi-Weather-Clock | Clock, weather, sun/moon, air quality, web settings | Alarm, music, to-do, LVGL | Apache-2.0 (file only) |
| echo-cool/cyd-world-clock | World clock, weather, countdown with alarm sound, web settings, OTA | Music, to-do, bedside alarm | GPL-3.0 |
| anticolon/BedsideClock | Alarm with MP3 from SD, snooze, weekly schedule, web upload of songs | Different board (ESP32-C6, I2S amp), to-do; non-commercial license | CC BY-NC 4.0 |

Decision: start from **cyd-weather-station** (Apache-2.0, maintained, best looking, already supports the zip code and
the SD card) and add the three missing pieces. BedsideClock is a reference for the alarm logic only; its license does
not allow reuse of its code here.

## This folder

A clone of cyd-weather-station v0.1.39 with the upstream remote named `upstream`. Changes so far:

- `config/config.h`: zip code 30062, time zone America/New_York, hostname cyd-alarm-clock.
- `platformio.ini`: default board `cyd_28r` (2.8" resistive, the ESP32-2432S028R).
- `config/secrets.h`: copied from the example, Wi-Fi credentials still to fill in (the file is git-ignored).

Build and flash:

    pio run -e cyd_28r
    pio run -e cyd_28r -t upload --upload-port COM5

## Additions, in order

### 1. Clock face (first screen)

A new LVGL tab before "Current": time in a large font, date, next alarm, and a one-line weather summary (icon,
temperature, high/low). Night mode dims it after the sleep schedule the base already has.

### 2. Alarm with music

- Alarms stored in NVS: time, weekdays, enabled, song, volume, snooze minutes. Up to four.
- Playback: MP3 files from the SD card folder `/music`, decoded with the ESP8266Audio library and played on the CYD's
  own speaker through the ESP32 DAC (GPIO 26, amplifier enable GPIO 4). The base already mounts the card.
- Ringing screen: big time, song name, Snooze and Stop buttons; volume ramps up over 30 s.
- Settings: on the device (Alarms tab) and on the web page `/settings` (time pickers, song list from the card, upload
  of MP3 files to the card).
- Fallback: a built-in tone if the card or the file is missing.

### 3. To-do list

- Source: a Todoist project (free API token) or a local list kept on the device. The local list is edited on the web
  page and stored in NVS; Todoist is polled every 10 minutes when a token is set.
- Screen: a "To-do" tab with the open items, a tap marks an item done (synced back to Todoist when used).
- The clock face shows the top three items.

### 4. Polish

- Theme: keep Catppuccin, pick a flavor per daytime (Latte by day, Mocha at night).
- Screenshot set for the README, release workflow from upstream kept.
- Keep upstream mergeable: new code in new files (`alarm_manager`, `audio_player`, `todo_manager`, `ui_clock`,
  `ui_alarm`, `ui_todo`), minimal edits to `ui.cpp` and `main.cpp`.

## Open questions

- Speaker: the CYD's built-in speaker is small. An external 4 ohm speaker on the SPEAK connector sounds much better.
- To-do source: Todoist, a local list, or both? Todoist is the simplest "nice" option; Google Tasks needs OAuth.
- Music: MP3 files copied to the SD card, or internet radio streams as well?
