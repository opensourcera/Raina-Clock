#include "audio_player.h"
#include "sd_card_manager.h"
#include "event_log.h"
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

// The opening of Fuer Elise as a ringtone
static const char fur_elise_rtttl[] PROGMEM =
    "FurElise:d=8,o=5,b=125:e6,d#6,e6,d#6,e6,b,d6,c6,4a,p,c,e,a,4b,p,e,g#,b,4c6,p,e,"
    "e6,d#6,e6,d#6,e6,b,d6,c6,4a,p,c,e,a,4b,p,e,c6,b,2a";

// Four short beeps, then a pause: plain, loud and independent of the SD card and the MP3 decoder
static const char beep_rtttl[] PROGMEM = "Beep:d=16,o=7,b=200:c,p,c,p,c,p,c,2p";

// The MP3 file source, with every card access under the shared SD lock
class LockedSdSource : public AudioFileSourceSD {
public:
    LockedSdSource(const char* name) : AudioFileSourceSD() { SdLock g; open(name); }
    uint32_t read(void* data, uint32_t len) override { SdLock g; return AudioFileSourceSD::read(data, len); }
    bool seek(int32_t pos, int dir) override { SdLock g; return AudioFileSourceSD::seek(pos, dir); }
    bool close() override { SdLock g; return AudioFileSourceSD::close(); }
    bool isOpen() override { SdLock g; return AudioFileSourceSD::isOpen(); }
    uint32_t getSize() override { SdLock g; return AudioFileSourceSD::getSize(); }
    uint32_t getPos() override { SdLock g; return AudioFileSourceSD::getPos(); }
};

static uint32_t restartAt = 0;      // looping: when to start the next round (a short breath between repeats)
static uint32_t startedAt = 0;      // millis() when the current round started
static int quickFails = 0;          // rounds in a row that ended within a second: the file or decoder is broken

static void audioTask(void* arg) {
    AudioPlayer* self = (AudioPlayer*)arg;
    for (;;) {
        self->pump();
        vTaskDelay(1);
    }
}

void AudioPlayer::begin() {
    _mx = xSemaphoreCreateRecursiveMutex();
    AudioOutputI2S* out = new AudioOutputI2S(0, AudioOutputI2S::INTERNAL_DAC);
    out->SetOutputModeMono(true);
    out->SetGain(_volume / 100.0f);
    _out = out;
    xTaskCreatePinnedToCore(audioTask, "audio", 8192, this, 2, nullptr, 0);
}

bool AudioPlayer::startLocked(Kind kind, const String& path, bool loop) {
    teardown();
    _curKind = kind; _curPath = path; _curLoop = loop;
    restartAt = 0; quickFails = 0;
    return startCurrent();
}

bool AudioPlayer::playFile(const String& path, bool loop) {
    xSemaphoreTakeRecursive(_mx, portMAX_DELAY);
    teardown();                                              // nothing reads the card while we look at it
    bool ok = false;
    if (!SdCardManager::isMounted()) SdCardManager::begin();   // the base unmounts the card when it has no logging to do
    bool there; { SdLock g; there = SdCardManager::isMounted() && SD.exists(path); }
    if (there) ok = startLocked(KIND_FILE, path, loop);
    else eventLog("audio: %s not found (SD card %s)", path.c_str(), SdCardManager::isMounted() ? "mounted" : "not mounted");
    xSemaphoreGiveRecursive(_mx);
    return ok;
}

void AudioPlayer::playBuiltin(bool loop) {
    xSemaphoreTakeRecursive(_mx, portMAX_DELAY);
    startLocked(KIND_TUNE, "", loop);
    xSemaphoreGiveRecursive(_mx);
}

void AudioPlayer::playBeep(bool loop) {
    xSemaphoreTakeRecursive(_mx, portMAX_DELAY);
    startLocked(KIND_BEEP, "", loop);
    xSemaphoreGiveRecursive(_mx);
}

void AudioPlayer::stop() {
    xSemaphoreTakeRecursive(_mx, portMAX_DELAY);
    if (_running) eventLog("audio: stop");
    restartAt = 0;
    teardown();
    _curLoop = false;
    xSemaphoreGiveRecursive(_mx);
}

bool AudioPlayer::isPlaying() const { return _running; }

void AudioPlayer::setVolume(int pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    _volume = pct;
    if (_out) ((AudioOutputI2S*)_out)->SetGain(pct / 100.0f);
}

String AudioPlayer::nowPlaying() {
    xSemaphoreTakeRecursive(_mx, portMAX_DELAY);
    String r;
    if (_running) {
        if (_curKind == KIND_BEEP) r = "Beep";
        else if (_curKind == KIND_TUNE) r = "Fuer Elise (built-in tune)";
        else { int slash = _curPath.lastIndexOf('/'); r = slash >= 0 ? _curPath.substring(slash + 1) : _curPath; }
    }
    xSemaphoreGiveRecursive(_mx);
    return r;
}

String AudioPlayer::musicList() {
    // Never read the card while it is being played: stop first
    audio.stop();
    SdLock sdGuard;
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

// ---- the playing side; all of it runs with the mutex held

bool AudioPlayer::startCurrent() {
    AudioOutputI2S* out = (AudioOutputI2S*)_out;
    startedAt = millis();
    if (_curKind != KIND_FILE) {
        const char* tune = _curKind == KIND_BEEP ? beep_rtttl : fur_elise_rtttl;
        _src = new AudioFileSourcePROGMEM(tune, strlen_P(tune));
        _gen = new AudioGeneratorRTTTL();
        if (!((AudioGeneratorRTTTL*)_gen)->begin((AudioFileSource*)_src, out)) { eventLog("audio: the built-in sound did not start"); teardown(); return false; }
    } else {
        LockedSdSource* src = new LockedSdSource(_curPath.c_str());
        if (!src->isOpen()) {
            eventLog("audio: cannot open %s, using the beep instead", _curPath.c_str());
            delete src;
            _curKind = KIND_BEEP;
            return startCurrent();
        }
        _src = src;
        _gen = new AudioGeneratorMP3();
        if (!((AudioGeneratorMP3*)_gen)->begin(src, out)) {
            eventLog("audio: the MP3 decoder refused %s (heap %u, largest block %u), using the beep instead", _curPath.c_str(),
                     (unsigned)ESP.getFreeHeap(), (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
            teardown();
            _curKind = KIND_BEEP;
            return startCurrent();
        }
    }
    i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN);   // GPIO 26 only; GPIO 25 stays a digital pin for the touch panel
    ampOn();
    _running = true;
    eventLog("audio: playing %s at volume %d%s (heap %u)", _curKind == KIND_BEEP ? "beep" : (_curKind == KIND_TUNE ? "built-in tune" : _curPath.c_str()),
             _volume, _curLoop ? ", looping" : "", (unsigned)ESP.getFreeHeap());
    return true;
}

void AudioPlayer::teardown() {
    if (_gen) {
        if (_curKind != KIND_FILE) { AudioGeneratorRTTTL* g = (AudioGeneratorRTTTL*)_gen; if (g->isRunning()) g->stop(); delete g; }
        else { AudioGeneratorMP3* g = (AudioGeneratorMP3*)_gen; if (g->isRunning()) g->stop(); delete g; }
        _gen = nullptr;
    }
    if (_src) {
        if (_curKind != KIND_FILE) delete (AudioFileSourcePROGMEM*)_src; else delete (LockedSdSource*)_src;
        _src = nullptr;
    }
    if (_running) {
        if (_out) ((AudioOutputI2S*)_out)->stop();
        ampOff();
    }
    _running = false;
}

void AudioPlayer::pump() {
    if (xSemaphoreTakeRecursive(_mx, 0) != pdTRUE) return;   // someone is starting or stopping: try again in a moment
    if (!_running) {
        if (restartAt != 0 && (int32_t)(millis() - restartAt) >= 0) { restartAt = 0; startCurrent(); }
    } else if (_gen) {
        bool more = _curKind != KIND_FILE ? ((AudioGeneratorRTTTL*)_gen)->loop() : ((AudioGeneratorMP3*)_gen)->loop();
        if (!more) {
            Kind kind = _curKind;
            bool quick = millis() - startedAt < 1000;
            teardown();
            if (_curLoop) {
                if (kind == KIND_FILE && quick && ++quickFails >= 2) {
                    eventLog("audio: %s ends at once, switching to the beep", _curPath.c_str());
                    _curKind = KIND_BEEP;
                }
                restartAt = millis() + (_curKind == KIND_FILE ? 200 : 600);
                if (restartAt == 0) restartAt = 1;
            }
        }
    }
    xSemaphoreGiveRecursive(_mx);
}
