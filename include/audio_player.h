#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

// MP3 playback on the CYD's own speaker through the ESP32 DAC on GPIO 26, decoded by ESP8266Audio in a FreeRTOS
// task on core 0 so the screen never starves it. Files come from /music on the SD card; without a card or a file
// a built-in Fuer Elise ringtone (RTTTL) plays instead. Only DAC channel 2 (GPIO 26) is enabled: GPIO 25 stays
// the touch controller's clock.

#include <Arduino.h>

class AudioPlayer {
public:
    void begin();
    bool playFile(const String& path, bool loop);   // "/music/fur_elise.mp3"; false when the file is not there
    void playBuiltin(bool loop);
    void stop();
    bool isPlaying() const;
    void setVolume(int pct);                        // 0-100
    int getVolume() const { return _volume; }
    String nowPlaying() const;

    static String musicList();                      // JSON array of the .mp3 files in /music
    static const char* musicDir() { return "/music"; }

    void pump();                                    // the task body, public for the task trampoline

private:
    enum Request { REQ_NONE, REQ_PLAY, REQ_STOP };
    volatile Request _req = REQ_NONE;
    String _reqPath; bool _reqLoop = false; bool _reqBuiltin = false;
    String _curPath; bool _curLoop = false; bool _curBuiltin = false;
    int _volume = 80;
    void* _out = nullptr;      // AudioOutputI2S
    void* _src = nullptr;      // AudioFileSource
    void* _gen = nullptr;      // AudioGenerator
    volatile bool _running = false;
    bool startCurrent();
    void teardown();
};

extern AudioPlayer audio;

#endif // AUDIO_PLAYER_H
