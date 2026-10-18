#include "audio_player.h"
#include "event_log.h"
#include <AudioFileSourcePROGMEM.h>
#include <AudioGeneratorRTTTL.h>
#include <AudioOutputI2S.h>
#include <driver/i2s.h>
#include <driver/dac.h>

AudioPlayer audio;

// GPIO 4 is the RGB LED's red channel (PWM, active low) and, as in the SleepCYD firmware on this board family, the
// speaker amplifier's enable (LOW = on). While playing, take the pin away from the LED and hold it low.
#define AMP_EN_PIN 4
#define LED_RED_CHANNEL 5
static void ampOn()  { ledcDetachPin(AMP_EN_PIN); pinMode(AMP_EN_PIN, OUTPUT); digitalWrite(AMP_EN_PIN, LOW); }
static void ampOff() { pinMode(AMP_EN_PIN, OUTPUT); digitalWrite(AMP_EN_PIN, HIGH); ledcAttachPin(AMP_EN_PIN, LED_RED_CHANNEL); ledcWrite(LED_RED_CHANNEL, 255); }

// Four short beeps, then a pause: plain, loud and independent of the SD card and the MP3 decoder
static const char beep_rtttl[] PROGMEM = "Beep:d=16,o=7,b=200:c,p,c,p,c,p,c,2p";

static uint32_t restartAt = 0;      // looping: when to start the next round (a short breath between repeats)

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
    xTaskCreatePinnedToCore(audioTask, "audio", 4096, this, 2, nullptr, 0);
}

void AudioPlayer::playBeep(bool loop) {
    xSemaphoreTakeRecursive(_mx, portMAX_DELAY);
    teardown();
    _curLoop = loop;
    restartAt = 0;
    startCurrent();
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

// ---- the playing side; all of it runs with the mutex held

bool AudioPlayer::startCurrent() {
    AudioOutputI2S* out = (AudioOutputI2S*)_out;
    _src = new AudioFileSourcePROGMEM(beep_rtttl, strlen_P(beep_rtttl));
    _gen = new AudioGeneratorRTTTL();
    if (!((AudioGeneratorRTTTL*)_gen)->begin((AudioFileSource*)_src, out)) { eventLog("audio: the beep did not start"); teardown(); return false; }
    i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN);   // GPIO 26 only; GPIO 25 stays a digital pin for the touch panel
    dac_output_disable(DAC_CHANNEL_1);          // the I2S driver also switches on GPIO 25, the touch panel's clock line
    _touchFix = true;
    ampOn();
    _running = true;
    if (!_repeatRound) eventLog("audio: beep at volume %d%s (heap %u)", _volume, _curLoop ? ", looping" : "", (unsigned)ESP.getFreeHeap());
    _repeatRound = false;
    return true;
}

void AudioPlayer::teardown() {
    if (_gen) { AudioGeneratorRTTTL* g = (AudioGeneratorRTTTL*)_gen; if (g->isRunning()) g->stop(); delete g; _gen = nullptr; }
    if (_src) { delete (AudioFileSourcePROGMEM*)_src; _src = nullptr; }
    if (_running) {
        if (_out) ((AudioOutputI2S*)_out)->stop();
        ampOff();
        dac_output_disable(DAC_CHANNEL_1);
        _touchFix = true;
    }
    _running = false;
}

void AudioPlayer::pump() {
    if (xSemaphoreTakeRecursive(_mx, 0) != pdTRUE) return;   // someone is starting or stopping: try again in a moment
    if (!_running) {
        if (restartAt != 0 && (int32_t)(millis() - restartAt) >= 0) { restartAt = 0; _repeatRound = true; startCurrent(); }
    } else if (_gen) {
        if (!((AudioGeneratorRTTTL*)_gen)->loop()) {
            teardown();
            if (_curLoop) { restartAt = millis() + 600; if (restartAt == 0) restartAt = 1; }
        }
    }
    xSemaphoreGiveRecursive(_mx);
}
