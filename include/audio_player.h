#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

// The alarm signal: four short beeps, then a pause, repeated. Generated as a ringtone by ESP8266Audio in a FreeRTOS
// task on core 0 and played on the CYD's own speaker through the ESP32 DAC on GPIO 26. No SD card and no MP3 decoder
// are involved. Only DAC channel 2 (GPIO 26) is enabled: GPIO 25 stays the touch controller's clock.
//
// Every state change (play, stop) happens under one mutex, in the caller's task, and the playback task takes the
// same mutex for each step, so the two never touch the generator at the same time.

#include <Arduino.h>

class AudioPlayer {
public:
    void begin();
    void playBeep(bool loop);                       // beep-beep-beep-beep
    void stop();
    bool isPlaying() const;
    void setVolume(int pct);                        // 0-100
    int getVolume() const { return _volume; }

    bool takeTouchFix() { bool f = _touchFix; _touchFix = false; return f; }   // true once after the sound changed the DAC pins
    void pump();                                    // the task body, public for the task trampoline

private:
    SemaphoreHandle_t _mx = nullptr;
    bool _curLoop = false;
    int _volume = 80;
    void* _out = nullptr;      // AudioOutputI2S
    void* _src = nullptr;      // AudioFileSource
    void* _gen = nullptr;      // AudioGeneratorRTTTL
    volatile bool _running = false;
    volatile bool _touchFix = false;
    bool _repeatRound = false;   // the next start is a repeat of a looping sound: do not log it
    bool startCurrent();
    void teardown();
};

extern AudioPlayer audio;

#endif // AUDIO_PLAYER_H
