#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

// MP3 playback on the CYD's own speaker through the ESP32 DAC on GPIO 26, decoded by ESP8266Audio in a FreeRTOS
// task on core 0 so the screen never starves it. Files come from /music on the SD card. Two built-in sounds need no
// card and no decoder: a plain beep (the safe alarm signal) and a short Fuer Elise ringtone. Only DAC channel 2
// (GPIO 26) is enabled: GPIO 25 stays the touch controller's clock.
//
// Every state change (play, stop) happens under one mutex, in the caller's task, and the playback task takes the
// same mutex for each step, so the two never touch the decoder or the file at the same time.

#include <Arduino.h>

class AudioPlayer {
public:
    enum Kind { KIND_FILE = 0, KIND_TUNE = 1, KIND_BEEP = 2 };

    void begin();
    bool playFile(const String& path, bool loop);   // "/music/fur_elise.mp3"; false when the file is not there
    void playBuiltin(bool loop);                    // the Fuer Elise ringtone
    void playBeep(bool loop);                       // beep-beep-beep
    void stop();
    bool isPlaying() const;
    void setVolume(int pct);                        // 0-100
    int getVolume() const { return _volume; }
    String nowPlaying();

    static String musicList();                      // JSON array of the .mp3 files in /music
    static const char* musicDir() { return "/music"; }

    void pump();                                    // the task body, public for the task trampoline

private:
    SemaphoreHandle_t _mx = nullptr;
    String _curPath; bool _curLoop = false; Kind _curKind = KIND_FILE;
    int _volume = 80;
    void* _out = nullptr;      // AudioOutputI2S
    void* _src = nullptr;      // AudioFileSource
    void* _gen = nullptr;      // AudioGenerator
    volatile bool _running = false;
    bool startLocked(Kind kind, const String& path, bool loop);
    bool startCurrent();
    void teardown();
};

extern AudioPlayer audio;

#endif // AUDIO_PLAYER_H
