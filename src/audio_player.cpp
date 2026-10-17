#include "audio_player.h"
#include "sd_card_manager.h"
#include <SD.h>
#include <AudioFileSourceSD.h>
#include <AudioFileSourcePROGMEM.h>
#include <AudioGeneratorMP3.h>
#include <AudioGeneratorRTTTL.h>
#include <AudioOutputI2S.h>
#include <driver/i2s.h>
#include <ArduinoJson.h>
#include <string>

AudioPlayer audio;

// GPIO 4 is the RGB LED's red channel (PWM, active low) and, as in the SleepCYD firmware on this board family, the
// speaker amplifier's enable (LOW = on). While playing, take the pin away from the LED and hold it low.
#define AMP_EN_PIN 4
#define LED_RED_CHANNEL 5
static void ampOn()  { ledcDetachPin(AMP_EN_PIN); pinMode(AMP_EN_PIN, OUTPUT); digitalWrite(AMP_EN_PIN, LOW); }
static void ampOff() { pinMode(AMP_EN_PIN, OUTPUT); digitalWrite(AMP_EN_PIN, HIGH); ledcAttachPin(AMP_EN_PIN, LED_RED_CHANNEL); ledcWrite(LED_RED_CHANNEL, 255); }

// The opening of Fuer Elise as a ringtone, for when there is no SD card or no file
static const char fur_elise_rtttl[] PROGMEM =
    "FurElise:d=8,o=5,b=125:e6,d#6,e6,d#6,e6,b,d6,c6,4a,p,c,e,a,4b,p,e,g#,b,4c6,p,e,"
    "e6,d#6,e6,d#6,e6,b,d6,c6,4a,p,c,e,a,4b,p,e,c6,b,2a";

static void audioTask(void* arg) {
    AudioPlayer* self = (AudioPlayer*)arg;
    for (;;) {
        self->pump();
        vTaskDelay(1);
    }
}

void AudioPlayer::begin() {
    AudioOutputI2S* out = new AudioOutputI2S(0, AudioOutputI2S::INTERNAL_DAC);
    out->SetOutputModeMono(true);
    out->SetGain(_volume / 100.0f);
    _out = out;
    xTaskCreatePinnedToCore(audioTask, "audio", 8192, this, 2, nullptr, 0);
}

bool AudioPlayer::playFile(const String& path, bool loop) {
    if (!SdCardManager::isMounted()) SdCardManager::begin();   // the base unmounts the card when it has no logging to do
    if (!SdCardManager::isMounted() || !SD.exists(path)) return false;
    _reqPath = path; _reqLoop = loop; _reqBuiltin = false;
    _req = REQ_PLAY;
    return true;
}

void AudioPlayer::playBuiltin(bool loop) {
    _reqPath = ""; _reqLoop = loop; _reqBuiltin = true;
    _req = REQ_PLAY;
}

void AudioPlayer::stop() {
    _req = REQ_STOP;
    for (int i = 0; i < 50 && _running; i++) delay(4);   // let the task tear down
}

bool AudioPlayer::isPlaying() const { return _running; }

void AudioPlayer::setVolume(int pct) {
    if (pct < 0) pct = 0; if (pct > 100) pct = 100;
    _volume = pct;
    if (_out) ((AudioOutputI2S*)_out)->SetGain(pct / 100.0f);
}

String AudioPlayer::nowPlaying() const {
    if (!_running) return "";
    if (_curBuiltin) return "Fuer Elise (built-in tune)";
    int slash = _curPath.lastIndexOf('/');
    return slash >= 0 ? _curPath.substring(slash + 1) : _curPath;
}

String AudioPlayer::musicList() {
    DynamicJsonDocument doc(2048);
    JsonArray arr = doc.to<JsonArray>();
    if (!SdCardManager::isMounted()) SdCardManager::begin();
    if (SdCardManager::isMounted()) {
        File dir = SD.open(musicDir());
        if (dir && dir.isDirectory()) {
            for (File f = dir.openNextFile(); f && arr.size() < 40; f = dir.openNextFile()) {
                String name = f.name();
                int slash = name.lastIndexOf('/');
                if (slash >= 0) name = name.substring(slash + 1);
                String lower = name; lower.toLowerCase();
                if (!f.isDirectory() && lower.endsWith(".mp3")) arr.add(name);
                f.close();
            }
            dir.close();
        }
    }
    std::string out;
    serializeJson(doc, out);
    return String(out.c_str());
}

// ---- the task side

bool AudioPlayer::startCurrent() {
    AudioOutputI2S* out = (AudioOutputI2S*)_out;
    if (_curBuiltin) {
        _src = new AudioFileSourcePROGMEM(fur_elise_rtttl, strlen_P(fur_elise_rtttl));
        _gen = new AudioGeneratorRTTTL();
        if (!((AudioGeneratorRTTTL*)_gen)->begin((AudioFileSource*)_src, out)) { teardown(); return false; }
    } else {
        AudioFileSourceSD* src = new AudioFileSourceSD(_curPath.c_str());
        if (!src->isOpen()) { Serial.println(String("[audio] cannot open ") + _curPath + ", using the built-in tune"); delete src; _curBuiltin = true; return startCurrent(); }   // fall back to the tune
        _src = src;
        _gen = new AudioGeneratorMP3();
        if (!((AudioGeneratorMP3*)_gen)->begin(src, out)) { Serial.println("[audio] the MP3 decoder refused the file"); teardown(); return false; }
    }
    i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN);   // GPIO 26 only; GPIO 25 stays a digital pin for the touch panel
    ampOn();
    _running = true;
    Serial.println(String("[audio] playing ") + (_curBuiltin ? String("built-in tune") : _curPath) + " at volume " + _volume);
    return true;
}

void AudioPlayer::teardown() {
    if (_gen) {
        if (_curBuiltin) { AudioGeneratorRTTTL* g = (AudioGeneratorRTTTL*)_gen; if (g->isRunning()) g->stop(); delete g; }
        else { AudioGeneratorMP3* g = (AudioGeneratorMP3*)_gen; if (g->isRunning()) g->stop(); delete g; }
        _gen = nullptr;
    }
    if (_src) {
        if (_curBuiltin) delete (AudioFileSourcePROGMEM*)_src; else delete (AudioFileSourceSD*)_src;
        _src = nullptr;
    }
    if (_out) ((AudioOutputI2S*)_out)->stop();
    if (_running) ampOff();
    _running = false;
}

void AudioPlayer::pump() {
    if (_req == REQ_STOP) {
        teardown();
        _req = REQ_NONE;
        return;
    }
    if (_req == REQ_PLAY) {
        teardown();
        _curPath = _reqPath; _curLoop = _reqLoop; _curBuiltin = _reqBuiltin;
        _req = REQ_NONE;
        startCurrent();
        return;
    }
    if (!_running || !_gen) return;
    bool more = _curBuiltin ? ((AudioGeneratorRTTTL*)_gen)->loop() : ((AudioGeneratorMP3*)_gen)->loop();
    if (!more) {
        bool builtin = _curBuiltin;
        teardown();
        if (_curLoop) {
            if (builtin) delay(600);   // a breath between repeats of the tune
            _curBuiltin = builtin;
            startCurrent();
        }
    }
}
