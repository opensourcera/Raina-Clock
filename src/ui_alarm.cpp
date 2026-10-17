#include "ui_alarm.h"
#include "alarm_manager.h"
#include "audio_player.h"
#include "settings_manager.h"
#include "screensaver_manager.h"
#include "time_utils.h"
#include "catppuccin.h"
#include "config/config.h"

extern SettingsManager settings;
extern ScreenSaverManager screensaver;
int getCurrentThemeFlavor();
LV_FONT_DECLARE(font_ehs_64);

static lv_obj_t* alarm_label = nullptr;
static lv_obj_t* ring_overlay = nullptr;
static lv_obj_t* ring_time = nullptr;
static lv_obj_t* set_dialog = nullptr;
static lv_obj_t* roll_hour = nullptr;
static lv_obj_t* roll_min = nullptr;
static lv_obj_t* roll_ampm = nullptr;
static bool alarm_large = false;

static bool localNow(struct tm* t) { return getLocalTimeWrapper(t, 0); }

// The time on the ringing screen: digits and a colon only (the font has no letters)
static void ringTimeNow() {
    if (ring_time == nullptr) return;
    struct tm t;
    if (!getLocalTimeWrapper(&t, 0)) return;
    bool use24 = settings.getUse24HourFormat();
    int hour = use24 ? t.tm_hour : (t.tm_hour % 12 == 0 ? 12 : t.tm_hour % 12);
    char b[12];
    lv_snprintf(b, sizeof(b), use24 ? "%02d:%02d" : "%d:%02d", hour, t.tm_min);
    lv_label_set_text(ring_time, b);
    lv_obj_align(ring_time, LV_ALIGN_CENTER, 0, -34);
}

static void ringingStop() {
    audio.stop();
    alarmClock.stop();
    hideAlarmRinging();
    refreshAlarmUI();
}

static void ringingSnooze() {
    struct tm now;
    audio.stop();
    if (localNow(&now)) alarmClock.snooze(now); else alarmClock.stop();
    hideAlarmRinging();
    refreshAlarmUI();
}

void alarmRingingStopNow() { ringingStop(); }
void alarmRingingSnoozeNow() { ringingSnooze(); }
static void stop_cb(lv_event_t* e) { (void)e; ringingStop(); }
static void snooze_cb(lv_event_t* e) { (void)e; ringingSnooze(); }

static lv_obj_t* smallButton(lv_obj_t* parent, const char* text, lv_event_cb_t cb, uint32_t color) {
    lv_obj_t* b = lv_btn_create(parent);
    lv_obj_set_height(b, alarm_large ? 40 : 30);
    lv_obj_set_style_bg_color(b, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_radius(b, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(b, alarm_large ? 14 : 8, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(b, 0, LV_PART_MAIN);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, alarm_large ? &lv_font_montserrat_20 : &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(0x11111b), LV_PART_MAIN);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
    return b;
}

// ---- quick buttons

static void inHours(float h) {
    struct tm now;
    if (!localNow(&now)) return;
    alarmClock.setInHours(h, now);
    alarmClock.save();
    refreshAlarmUI();
}
static void six_cb(lv_event_t* e) { (void)e; inHours(6); }
static void seven_cb(lv_event_t* e) { (void)e; inHours(7); }
static void off_cb(lv_event_t* e) { (void)e; alarmClock.setEnabled(false); alarmClock.save(); refreshAlarmUI(); }

// ---- the Set popup: the time of day (every day), or a count down (hours and minutes from now)

static bool dlg_countdown = false;      // remembered between openings
static int dlg_cd_hours = 6, dlg_cd_minutes = 0;
static void dialogBuild();

static void dialogClose() { if (set_dialog) { lv_obj_del(set_dialog); set_dialog = nullptr; roll_hour = roll_min = roll_ampm = nullptr; } }
static void dialog_cancel_cb(lv_event_t* e) { (void)e; dialogClose(); }

static void dialog_rebuild_async(void* arg) { (void)arg; dialogClose(); dialogBuild(); }

static void dialog_countdown_cb(lv_event_t* e) {
    dlg_countdown = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    lv_async_call(dialog_rebuild_async, nullptr);       // rebuild outside the checkbox's own callback
}

static void dialog_save_cb(lv_event_t* e) {
    (void)e;
    if (!roll_hour || !roll_min) return;
    int first = (int)lv_roller_get_selected(roll_hour);
    int minute = (int)lv_roller_get_selected(roll_min);
    if (dlg_countdown) {
        dlg_cd_hours = first; dlg_cd_minutes = minute;
        if (first == 0 && minute == 0) return;           // nothing to count
        struct tm now;
        if (!localNow(&now)) return;
        alarmClock.setInHours(first + minute / 60.0f, now);
    } else {
        int hour = first;
        if (roll_ampm) {                                 // 12-hour rollers: 1..12 plus AM/PM
            hour = (first + 1) % 12;
            if (lv_roller_get_selected(roll_ampm) == 1) hour += 12;
        }
        alarmClock.setTime(hour, minute, true);
    }
    alarmClock.save();
    dialogClose();
    refreshAlarmUI();
}

static lv_obj_t* roller(lv_obj_t* parent, const char* options, int selected, int width) {
    lv_obj_t* r = lv_roller_create(parent);
    lv_roller_set_options(r, options, LV_ROLLER_MODE_INFINITE);
    lv_roller_set_visible_row_count(r, 3);
    lv_obj_set_width(r, width);
    lv_obj_set_style_text_font(r, alarm_large ? &lv_font_montserrat_28 : &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_font(r, alarm_large ? &lv_font_montserrat_28 : &lv_font_montserrat_20, LV_PART_SELECTED);
    lv_obj_set_style_bg_color(r, lv_color_hex(0x313244), LV_PART_MAIN);
    lv_obj_set_style_text_color(r, lv_color_hex(0xa6adc8), LV_PART_MAIN);
    lv_obj_set_style_bg_color(r, lv_color_hex(0xfab387), LV_PART_SELECTED);
    lv_obj_set_style_text_color(r, lv_color_hex(0x11111b), LV_PART_SELECTED);
    lv_obj_set_style_border_width(r, 0, LV_PART_MAIN);
    lv_roller_set_selected(r, selected, LV_ANIM_OFF);
    return r;
}

static lv_obj_t* unitLabel(lv_obj_t* parent, const char* text) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, alarm_large ? &lv_font_montserrat_28 : &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(0xcdd6f4), LV_PART_MAIN);
    return l;
}

static void dialogBuild() {
    if (set_dialog) return;
    bool use24 = settings.getUse24HourFormat();
    const AlarmConfig& c = alarmClock.cfg();

    set_dialog = lv_obj_create(lv_layer_top());
    lv_obj_set_size(set_dialog, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(set_dialog, lv_color_hex(0x11111b), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(set_dialog, 245, LV_PART_MAIN);
    lv_obj_set_style_border_width(set_dialog, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(set_dialog, 0, LV_PART_MAIN);
    lv_obj_clear_flag(set_dialog, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(set_dialog);
    lv_label_set_text(title, dlg_countdown ? "Alarm in" : "Alarm at");
    lv_obj_set_style_text_font(title, alarm_large ? &lv_font_montserrat_28 : &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(0xfab387), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 8, 8);

    lv_obj_t* cd = lv_checkbox_create(set_dialog);
    lv_checkbox_set_text(cd, "Count down");
    lv_obj_set_style_text_font(cd, alarm_large ? &lv_font_montserrat_20 : &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(cd, lv_color_hex(0xcdd6f4), LV_PART_MAIN);
    lv_obj_set_style_pad_all(cd, 4, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(cd, lv_color_hex(0x313244), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(cd, lv_color_hex(0xa6e3a1), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(cd, lv_color_hex(0xa6e3a1), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_ext_click_area(cd, 10);
    if (dlg_countdown) lv_obj_add_state(cd, LV_STATE_CHECKED);
    lv_obj_add_event_cb(cd, dialog_countdown_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_align(cd, LV_ALIGN_TOP_RIGHT, -8, 8);

    lv_obj_t* row = lv_obj_create(set_dialog);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(row, 8, LV_PART_MAIN);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(row, LV_ALIGN_CENTER, 0, -2);

    static char hours24[24 * 4], hours12[12 * 4], mins[60 * 4];
    hours24[0] = hours12[0] = mins[0] = 0;
    for (int i = 0; i < 24; i++) { char b[8]; snprintf(b, sizeof(b), "%d%s", i, i < 23 ? "\n" : ""); strcat(hours24, b); }
    for (int i = 1; i <= 12; i++) { char b[8]; snprintf(b, sizeof(b), "%d%s", i, i < 12 ? "\n" : ""); strcat(hours12, b); }
    for (int i = 0; i < 60; i++) { char b[8]; snprintf(b, sizeof(b), "%02d%s", i, i < 59 ? "\n" : ""); strcat(mins, b); }

    int w = alarm_large ? 84 : 62;
    roll_ampm = nullptr;
    if (dlg_countdown) {                                // hours and minutes from now
        roll_hour = roller(row, hours24, dlg_cd_hours, w);
        unitLabel(row, "h");
        roll_min = roller(row, mins, dlg_cd_minutes, w);
        unitLabel(row, "min");
    } else {                                            // the time of day
        if (use24) {
            roll_hour = roller(row, hours24, c.hour, w);
        } else {
            int h12 = c.hour % 12; if (h12 == 0) h12 = 12;
            roll_hour = roller(row, hours12, h12 - 1, w);
        }
        unitLabel(row, ":");
        roll_min = roller(row, mins, c.minute, w);
        if (!use24) roll_ampm = roller(row, "AM\nPM", c.hour >= 12 ? 1 : 0, alarm_large ? 78 : 56);
    }

    lv_obj_t* btns = lv_obj_create(set_dialog);
    lv_obj_set_size(btns, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btns, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_bg_opa(btns, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(btns, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btns, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(btns, 12, LV_PART_MAIN);
    lv_obj_clear_flag(btns, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(btns, LV_ALIGN_BOTTOM_MID, 0, -6);
    smallButton(btns, "Cancel", dialog_cancel_cb, 0x6c7086);
    smallButton(btns, dlg_countdown ? "Start" : "Save, every day", dialog_save_cb, 0xa6e3a1);
}

static void set_cb(lv_event_t* e) {
    (void)e;
    if (!dlg_countdown && alarmClock.cfg().enabled && !alarmClock.cfg().repeat) dlg_countdown = true;   // a count down is running: stay in that mode
    dialogBuild();
}

// ---- the bar on the clock tab

void createAlarmBar(lv_obj_t* parent, bool isLargeScreen) {
    alarm_large = isLargeScreen;
    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_set_size(bar, LV_PCT(96), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x11111b), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, 140, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bar, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_row(bar, 3, LV_PART_MAIN);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, -3);

    alarm_label = lv_label_create(bar);
    lv_obj_set_style_text_font(alarm_label, isLargeScreen ? &lv_font_montserrat_20 : &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(alarm_label, lv_color_hex(0xf9e2af), LV_PART_MAIN);
    lv_label_set_text(alarm_label, "Alarm off");

    lv_obj_t* row = lv_obj_create(bar);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(row, 6, LV_PART_MAIN);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    smallButton(row, "6 h", six_cb, 0xa6e3a1);
    smallButton(row, "7 h", seven_cb, 0x94e2d5);
    smallButton(row, "Set", set_cb, 0xfab387);
    smallButton(row, "Off", off_cb, 0x6c7086);
    refreshAlarmUI();
}

void refreshAlarmUI() {
    if (alarm_label == nullptr) return;
    struct tm now;
    String text = localNow(&now) ? alarmClock.nextText(now, settings.getUse24HourFormat()) : alarmClock.timeText(settings.getUse24HourFormat());
    lv_label_set_text(alarm_label, text.c_str());
}

// ---- ringing

void showAlarmRinging() {
    if (ring_overlay) return;
    ring_overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(ring_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(ring_overlay, lv_color_hex(0x1e1e2e), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(ring_overlay, lv_color_hex(0x45243a), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(ring_overlay, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ring_overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(ring_overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(ring_overlay, 0, LV_PART_MAIN);
    lv_obj_clear_flag(ring_overlay, LV_OBJ_FLAG_SCROLLABLE);

    ring_time = lv_label_create(ring_overlay);
    lv_obj_set_style_text_font(ring_time, &font_ehs_64, LV_PART_MAIN);
    lv_obj_set_style_text_color(ring_time, lv_color_hex(0xf9e2af), LV_PART_MAIN);
    lv_label_set_text(ring_time, " ");
    lv_obj_align(ring_time, LV_ALIGN_CENTER, 0, -34);
    ringTimeNow();

    lv_obj_t* song = lv_label_create(ring_overlay);
    String what = audio.nowPlaying();
    lv_label_set_text(song, what.length() ? what.c_str() : "Wake up");
    lv_obj_set_style_text_font(song, alarm_large ? &lv_font_montserrat_20 : &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(song, lv_color_hex(0xcdd6f4), LV_PART_MAIN);
    lv_obj_align(song, LV_ALIGN_CENTER, 0, 10);

    lv_obj_t* btns = lv_obj_create(ring_overlay);
    lv_obj_set_size(btns, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btns, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_bg_opa(btns, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(btns, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(btns, 16, LV_PART_MAIN);
    lv_obj_clear_flag(btns, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(btns, LV_ALIGN_BOTTOM_MID, 0, -10);
    char snz[24];
    snprintf(snz, sizeof(snz), "Snooze %d min", alarmClock.cfg().snoozeMinutes);
    lv_obj_t* b1 = smallButton(btns, snz, snooze_cb, 0x89b4fa);
    lv_obj_t* b2 = smallButton(btns, "Stop", stop_cb, 0xf38ba8);
    lv_obj_set_height(b1, alarm_large ? 56 : 44);
    lv_obj_set_height(b2, alarm_large ? 56 : 44);
    lv_obj_set_width(b2, alarm_large ? 140 : 100);
}

void hideAlarmRinging() {
    if (ring_overlay) { lv_obj_del(ring_overlay); ring_overlay = nullptr; ring_time = nullptr; }
}

bool isAlarmRingingShown() { return ring_overlay != nullptr; }

void alarmRingingSetTime(const char* time_str) {
    (void)time_str;                      // the header text carries "AM"/"PM", which the clock font has no letters for
    ringTimeNow();
}

void startAlarmRinging() {
    const AlarmConfig& c = alarmClock.cfg();
    screensaver.wake();
    audio.setVolume(c.volume);
    String path = String(AudioPlayer::musicDir()) + "/" + c.song;
    if (!audio.playFile(path, true)) audio.playBuiltin(true);
    delay(30);   // let the task open the file so the ringing view can name it
    showAlarmRinging();
    refreshAlarmUI();
}

// ---- Setup, Net tab: a Test start/stop button and the alarm volume

static lv_obj_t* test_btn = nullptr;
static lv_obj_t* test_lbl = nullptr;
static lv_obj_t* vol_lbl = nullptr;
static lv_timer_t* test_timer = nullptr;
static bool test_on = false;
static uint32_t test_t0 = 0;

static void test_update_label() {
    if (test_lbl == nullptr || test_btn == nullptr) return;
    lv_label_set_text(test_lbl, test_on ? LV_SYMBOL_STOP "  Stop" : LV_SYMBOL_PLAY "  Test sound");
    lv_obj_set_style_bg_color(test_btn, lv_color_hex(test_on ? 0xf38ba8 : 0xa6e3a1), LV_PART_MAIN);
}

static void test_cb(lv_event_t* e) {
    (void)e;
    if (test_on) {
        audio.stop();
        test_on = false;
    } else {
        audio.setVolume(alarmClock.cfg().volume);
        String path = String(AudioPlayer::musicDir()) + "/" + alarmClock.cfg().song;
        if (!audio.playFile(path, false)) audio.playBuiltin(false);   // the built-in tune when the file is not on the card
        test_on = true;
        test_t0 = millis();
    }
    test_update_label();
}

static void test_timer_cb(lv_timer_t* t) {
    (void)t;
    if (test_on && millis() - test_t0 > 1500 && !audio.isPlaying()) { test_on = false; test_update_label(); }   // the song ended by itself
}

static void vol_text() {
    if (vol_lbl == nullptr) return;
    char b[24];
    snprintf(b, sizeof(b), "Volume %d%%", alarmClock.cfg().volume);
    lv_label_set_text(vol_lbl, b);
}

static void vol_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        int v = lv_slider_get_value(lv_event_get_target(e));
        alarmClock.setVolume(v);
        audio.setVolume(v);          // live, while a test is playing
        vol_text();
    } else if (code == LV_EVENT_RELEASED) {
        alarmClock.save();
    }
}

void createAlarmTestControls(lv_obj_t* parent, bool isLargeScreen) {
    alarm_large = isLargeScreen;
    if (test_timer) { lv_timer_del(test_timer); test_timer = nullptr; }
    test_on = audio.isPlaying();

    test_btn = lv_btn_create(parent);
    lv_obj_set_size(test_btn, lv_pct(100), isLargeScreen ? 52 : 38);
    lv_obj_set_style_radius(test_btn, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(test_btn, 0, LV_PART_MAIN);
    test_lbl = lv_label_create(test_btn);
    lv_obj_set_style_text_font(test_lbl, isLargeScreen ? &lv_font_montserrat_20 : &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(test_lbl, lv_color_hex(0x11111b), LV_PART_MAIN);
    lv_obj_center(test_lbl);
    lv_obj_add_event_cb(test_btn, test_cb, LV_EVENT_CLICKED, nullptr);
    test_update_label();

    lv_obj_t* box = lv_obj_create(parent);
    lv_obj_set_size(box, lv_pct(100), isLargeScreen ? 52 : 38);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(box, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(box, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(box, 4, LV_PART_MAIN);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    vol_lbl = lv_label_create(box);
    lv_obj_set_style_text_font(vol_lbl, isLargeScreen ? &lv_font_montserrat_20 : &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(vol_lbl, lv_color_hex(0xcdd6f4), LV_PART_MAIN);
    vol_text();

    lv_obj_t* sl = lv_slider_create(box);
    lv_obj_set_size(sl, lv_pct(92), isLargeScreen ? 16 : 12);
    lv_slider_set_range(sl, 0, 100);
    lv_slider_set_value(sl, alarmClock.cfg().volume, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(sl, lv_color_hex(0x45475a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sl, lv_color_hex(0xcba6f7), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sl, lv_color_hex(0xf5c2e7), LV_PART_KNOB);
    lv_obj_set_style_pad_all(sl, 4, LV_PART_KNOB);
    lv_obj_set_ext_click_area(sl, 12);
    lv_obj_add_event_cb(sl, vol_cb, LV_EVENT_ALL, nullptr);

    test_timer = lv_timer_create(test_timer_cb, 500, nullptr);
}
