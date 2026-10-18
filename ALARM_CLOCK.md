# CYD Alarm Clock

An alarm clock for the Cheap Yellow Display (ESP32-2432S028R), built on
[cyd-weather-station](https://github.com/nicholaswilde/cyd-weather-station) (Apache-2.0). This file documents what was
added on top; the upstream README.md covers the weather station itself (Wi-Fi setup, settings, MQTT, OTA).

## Tabs on the screen

| Tab | What it shows |
|---|---|
| Now, Days, Hours | Weather for the configured zip code (30062), as upstream |
| To-do | The local list, one checkbox per item, tap to tick |
| Clock | Black screen, large lilac digits (Electronic Highway Sign font): hours and minutes side by side, seconds on the next line, no blinking; the alarm bar at the bottom: **6 h**, **7 h**, **Set**, **Off** |
| Setup | Seven small pages (Net, Look, Disp, Light, Night, Data, Sens), no scrolling. Net: Wi-Fi, zip code, Test sound, alarm volume, alarm sound (Music or Beep). Disp: orientation, time zone |

## Alarm

- **6 h / 7 h**: one-shot alarm that many hours from now, for going to sleep. Switches itself off after ringing.
- **Set**: a popup with hour and minute rollers; rings every day. Tick **Count down** to ring that many hours and minutes from now instead.
- When it rings, the screen shows the time with **Snooze** and **Stop**. Snooze is 9 minutes by default.
- The sound is `/music/<song>.mp3` on the SD card, default `fur_elise.mp3`, or **Beep** (Setup, Net, Alarm sound). The beep
  needs no card and no decoder. If the card or the file is missing, or the decoder will not start, the alarm beeps.
- An alarm nobody answers snoozes itself after 5 minutes, and stops for good after the third time.
- Web page `http://cyd-alarm-clock.local/alarm`: wake-in buttons, exact time, every-day switch, song, volume, snooze,
  test and stop, and **MP3 upload to the SD card**. API: `GET/POST /api/alarm`, `POST /api/alarm/test`,
  `POST /api/alarm/stop`, `GET /api/music`, `POST /api/music/upload`.

## To-do list

- Lives on the clock (NVS), nothing in the cloud. Up to 24 items of 64 characters.
- Web page `http://cyd-alarm-clock.local/todo`: add, tick, reorder, edit (double-click), delete, clear done.
- API: `GET /api/todo`, `POST /api/todo` with `{"items":[{"text":"...","done":false}]}`.

## Putting Für Elise on the card

`assets/music/fur_elise.mp3` is a recording by JMC Han from Wikimedia Commons, licensed CC BY-SA 4.0, re-encoded to mono
128 kbps. Either copy it to a folder named `music` on the SD card with a card reader, or upload it from the `/alarm`
page. The composition is Beethoven's WoO 59, public domain.

## Reliability

- **Log**: events (boot with the reset reason, Wi-Fi up and down, alarm ringing, snooze, stop, sound start and stop, any
  part of the main loop that took over 2 s, a loop stuck for 8 s) are kept on the clock's flash, about 48 KB, and shown at
  `http://cyd-alarm-clock.local/log`. A reset shows as `HANG (task watchdog)`, `CRASH`, `BROWNOUT` or `power on`.
- **Watchdog**: if the main loop stops for 60 s the clock restarts itself and logs it.
- **Config file**: Wi-Fi name and password, zip code, time zone and the alarm are also written to `/config.json` on the
  flash. If the preferences store is ever empty at start (erased or corrupted), the clock restores them from the file.
  `http://cyd-alarm-clock.local/api/backup` shows what the copy holds (the password length, not the password).
- **SD card lock**: the MP3 player and the weather cache never touch the card at the same time.
- Weather is fetched every 15 minutes (not while the alarm sounds), with short network time-outs.
- `/reset` (factory reset) only works with `?confirm=yes`, so a stray visit cannot wipe the settings.
- Bench commands over USB serial (115200): `p` play the alarm sound, `b` beep, `s` stop, `l` print the log, `h` heap.

## Audio notes

- Playback uses ESP8266Audio through the ESP32 DAC on GPIO 26 (the CYD speaker connector), decoded in a task on core 0.
- Only DAC channel 2 is enabled, so GPIO 25 stays the touch controller's clock.
- The CYD's own speaker is small; a 4 ohm speaker on the SPEAK connector sounds far better.

## Credits

- cyd-weather-station by Nicholas Wilde, Apache-2.0.
- Electronic Highway Sign font, from dafont.com, free per its author, no formal license text (`assets/fonts/ELECTRONIC-HIGHWAY-SIGN-NOTE.txt`).
- Für Elise recording by JMC Han, CC BY-SA 4.0, via Wikimedia Commons.
- ESP8266Audio by Earle F. Philhower, III, GPL-3.0.

## Build and flash

    pio run -e cyd_28r
    pio run -e cyd_28r -t upload --upload-port COM5
    pio test -e native -f test_todo

Wi-Fi: on first boot the clock opens an access point named `cyd-alarm-clock-XXXX`. Join it from a phone and enter the
home network and the zip code in the portal. Time comes from NTP, so the clock needs Wi-Fi once per boot.
