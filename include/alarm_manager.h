#ifndef ALARM_MANAGER_H
#define ALARM_MANAGER_H

// One alarm, kept in NVS (namespace "alarm"). Set to an exact time (repeats daily) or as "wake me in N hours"
// (one shot). Snooze pushes it by a few minutes. The main loop asks isDue() once a second with the local time.

#include <Arduino.h>
#include <time.h>

struct AlarmConfig {
    bool enabled = false;
    int hour = 7;              // local time, 0-23
    int minute = 0;
    bool repeat = true;        // daily; false: one shot, switched off after it rings
    int snoozeMinutes = 9;
    int volume = 80;           // 0-100
    String song = "fur_elise.mp3";   // file in /music on the SD card; the built-in tune plays when it is missing
};

class AlarmManager {
public:
    void begin();
    const AlarmConfig& cfg() const { return _c; }

    void setTime(int hour, int minute, bool repeat);        // enables the alarm
    void setInHours(float hours, const struct tm& now);     // one shot, from now
    void setEnabled(bool on);
    void setSnoozeMinutes(int m);
    void setVolume(int pct);
    void setSong(const String& file);

    bool isDue(const struct tm& now);                       // true once when it should start ringing
    void snooze(const struct tm& now);
    void stop();                                            // ringing over; a one-shot alarm switches itself off
    bool isRinging() const { return _ringing; }
    bool isSnoozed() const { return _snoozeKey >= 0; }

    String nextText(const struct tm& now, bool use24h) const;   // "6:30 AM, in 7 h 12 min", "Snoozed to 6:39 AM", "Off"
    String timeText(bool use24h) const;                         // "6:30 AM"

    String toJson(const struct tm* now, bool use24h) const;
    bool applyJson(const String& json, const struct tm* now);   // partial updates allowed
    void save();
    bool consumeChanged();

    static long keyOf(const struct tm& t) { return (long)t.tm_yday * 1440L + t.tm_hour * 60L + t.tm_min; }
    static String formatTime(int hour, int minute, bool use24h);

private:
    AlarmConfig _c;
    long _snoozeKey = -1;      // minute key when the snooze ends, -1 none
    long _lastFiredKey = -1;   // so one minute fires once
    bool _ringing = false;
    bool _changed = false;
};

extern AlarmManager alarmClock;

#endif // ALARM_MANAGER_H
