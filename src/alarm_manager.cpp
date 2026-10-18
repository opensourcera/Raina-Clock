#include "alarm_manager.h"
#include "config_backup.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <string>

AlarmManager alarmClock;

static const char* ALARM_NS = "alarm";

void AlarmManager::begin() {
    Preferences prefs;
    prefs.begin(ALARM_NS, true);
    _c.enabled = prefs.getBool("en", false);
    _c.hour = prefs.getInt("hr", 7);
    _c.minute = prefs.getInt("min", 0);
    _c.repeat = prefs.getBool("rep", true);
    _c.snoozeMinutes = prefs.getInt("snz", 9);
    _c.volume = prefs.getInt("vol", 80);
    _c.name = prefs.getString("nm", "");
    prefs.end();
    _changed = true;
}

void AlarmManager::save() {
    Preferences prefs;
    prefs.begin(ALARM_NS, false);
    prefs.putBool("en", _c.enabled);
    prefs.putInt("hr", _c.hour);
    prefs.putInt("min", _c.minute);
    prefs.putBool("rep", _c.repeat);
    prefs.putInt("snz", _c.snoozeMinutes);
    prefs.putInt("vol", _c.volume);
    prefs.putString("nm", _c.name);
    prefs.end();
    configBackupSave();
}

void AlarmManager::setTime(int hour, int minute, bool repeat) {
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) return;
    _c.hour = hour; _c.minute = minute; _c.repeat = repeat; _c.enabled = true;
    _snoozeKey = -1; _lastFiredKey = -1; _ringing = false;
    _changed = true;
}

void AlarmManager::setInHours(float hours, const struct tm& now) {
    if (hours <= 0 || hours > 24) return;
    long total = now.tm_hour * 60L + now.tm_min + (long)(hours * 60.0f + 0.5f);
    setTime((int)((total / 60) % 24), (int)(total % 60), false);
    _lastFiredKey = keyOf(now);   // never the current minute
}

void AlarmManager::setEnabled(bool on) { _c.enabled = on; if (!on) { _snoozeKey = -1; _ringing = false; } _changed = true; }
void AlarmManager::setSnoozeMinutes(int m) { if (m >= 1 && m <= 60) { _c.snoozeMinutes = m; _changed = true; } }
void AlarmManager::setVolume(int pct) { if (pct >= 0 && pct <= 100) { _c.volume = pct; _changed = true; } }

// Lower case letters and single spaces only, as the keyboard on the clock types them
void AlarmManager::setName(const String& n) {
    String out;
    for (size_t i = 0; i < n.length() && out.length() < 20; i++) {
        char c = n[i];
        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
        if (c >= 'a' && c <= 'z') out += c;
        else if (c == ' ' && out.length() && !out.endsWith(" ")) out += c;
    }
    out.trim();
    if (out != _c.name) { _c.name = out; _changed = true; }
}

bool AlarmManager::isDue(const struct tm& now) {
    if (_ringing) return false;
    long key = keyOf(now);
    if (_snoozeKey >= 0) {
        if (key >= _snoozeKey && key - _snoozeKey < 5) { _snoozeKey = -1; _lastFiredKey = key; _ringing = true; _changed = true; return true; }
        return false;
    }
    if (_c.enabled && now.tm_hour == _c.hour && now.tm_min == _c.minute && _lastFiredKey != key) {
        _lastFiredKey = key; _ringing = true; _changed = true;
        return true;
    }
    return false;
}

void AlarmManager::snooze(const struct tm& now) {
    _ringing = false;
    _snoozeKey = keyOf(now) + _c.snoozeMinutes;
    _changed = true;
}

void AlarmManager::stop() {
    _ringing = false;
    _snoozeKey = -1;
    if (!_c.repeat) { _c.enabled = false; save(); }
    _changed = true;
}

String AlarmManager::formatTime(int hour, int minute, bool use24h) {
    char buf[16];
    if (use24h) snprintf(buf, sizeof(buf), "%02d:%02d", hour, minute);
    else snprintf(buf, sizeof(buf), "%d:%02d %s", hour % 12 == 0 ? 12 : hour % 12, minute, hour < 12 ? "AM" : "PM");
    return String(buf);
}

String AlarmManager::timeText(bool use24h) const { return formatTime(_c.hour, _c.minute, use24h); }

String AlarmManager::nextText(const struct tm& now, bool use24h) const {
    if (_ringing) return "Ringing";
    if (_snoozeKey >= 0) {
        long m = _snoozeKey % 1440;
        return "Snoozed to " + formatTime((int)(m / 60), (int)(m % 60), use24h);
    }
    if (!_c.enabled) return "Alarm off";
    long diff = (_c.hour * 60L + _c.minute) - (now.tm_hour * 60L + now.tm_min);
    if (diff <= 0) diff += 1440;
    char buf[48];
    if (diff >= 60) snprintf(buf, sizeof(buf), ", in %ld h %02ld min", diff / 60, diff % 60);
    else snprintf(buf, sizeof(buf), ", in %ld min", diff);
    return timeText(use24h) + String(buf) + (_c.repeat ? "" : " (once)");
}

String AlarmManager::toJson(const struct tm* now, bool use24h) const {
    DynamicJsonDocument doc(512);
    doc["enabled"] = _c.enabled;
    doc["hour"] = _c.hour;
    doc["minute"] = _c.minute;
    doc["repeat"] = _c.repeat;
    doc["snooze_minutes"] = _c.snoozeMinutes;
    doc["volume"] = _c.volume;
    doc["name"] = _c.name;
    doc["ringing"] = _ringing;
    doc["snoozed"] = _snoozeKey >= 0;
    if (now) doc["next"] = nextText(*now, use24h);   // a String is copied; .c_str() of a temporary would dangle
    std::string out;
    serializeJson(doc, out);
    return String(out.c_str());
}

bool AlarmManager::applyJson(const String& json, const struct tm* now) {
    DynamicJsonDocument doc(512);
    if (deserializeJson(doc, json.c_str())) return false;
    if (doc.containsKey("name")) setName(doc["name"].as<String>());
    if (doc.containsKey("in_hours") && now) { setInHours(doc["in_hours"].as<float>(), *now); return true; }
    int hour = doc["hour"] | _c.hour, minute = doc["minute"] | _c.minute;
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) return false;
    bool timeChanged = hour != _c.hour || minute != _c.minute;
    _c.hour = hour; _c.minute = minute;
    if (doc.containsKey("repeat")) _c.repeat = doc["repeat"].as<bool>();
    if (doc.containsKey("snooze_minutes")) setSnoozeMinutes(doc["snooze_minutes"].as<int>());
    if (doc.containsKey("volume")) setVolume(doc["volume"].as<int>());
    if (doc.containsKey("enabled")) _c.enabled = doc["enabled"].as<bool>();
    if (timeChanged) { _snoozeKey = -1; _lastFiredKey = -1; }
    _changed = true;
    return true;
}

bool AlarmManager::consumeChanged() { bool c = _changed; _changed = false; return c; }
