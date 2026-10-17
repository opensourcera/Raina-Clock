#ifndef UI_ALARM_H
#define UI_ALARM_H

#include <lvgl.h>

// Alarm controls on the clock tab (6 h / 7 h / Set / Off), the set-time dialog and the full-screen ringing view.
void createAlarmBar(lv_obj_t* parent, bool isLargeScreen);
void createAlarmTestControls(lv_obj_t* parent, bool isLargeScreen);   // Setup, Net tab: Test start/stop and the alarm volume
void refreshAlarmUI();                       // label text from AlarmManager; call after any change and once a minute
void showAlarmRinging();
void hideAlarmRinging();
bool isAlarmRingingShown();
void alarmRingingSetTime(const char* time_str);
void startAlarmRinging();                    // ringing view + sound, from the main loop when the alarm is due

#endif // UI_ALARM_H
