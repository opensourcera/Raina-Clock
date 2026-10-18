#include "ui_alarm.h"
#include "alarm_manager.h"
#include "audio_player.h"
#include "event_log.h"
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

static int auto_snoozes = 0;       // times in a row the alarm snoozed itself because nobody answered

static void ringingStop() {
    eventLog("alarm: stopped");
    auto_snoozes = 0;
    audio.stop();
    alarmClock.stop();
    hideAlarmRinging();
    refreshAlarmUI();
}

static void ringingSnooze() {
    struct tm now;
    eventLog("alarm: snoozed");
    audio.stop();
    if (localNow(&now)) alarmClock.snooze(now); else alarmClock.stop();
    hideAlarmRinging();
    refreshAlarmUI();
}

void alarmRingingStopNow() { ringingStop(); }
void alarmRingingSnoozeNow() { auto_snoozes = 0; ringingSnooze(); }
static void stop_cb(lv_event_t* e) { (void)e; ringingStop(); }
static void snooze_cb(lv_event_t* e) { (void)e; auto_snoozes = 0; ringingSnooze(); }

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

// ---- the Set popup: the time of day (every day), or a count down (hours and minutes from now). The numbers are typed
// on a big keypad; tap the hour or the minutes to choose which one the keys change.

static bool dlg_countdown = false;      // remembered between openings
static int dlg_cd_hours = 6, dlg_cd_minutes = 0;
static void dialogBuild();

static int ed_h = 7, ed_m = 0;          // what the fields show (12-hour mode: 1..12)
static bool ed_pm = false, ed_12h = false;
static int ed_field = 0;                // 0 the hour, 1 the minutes
static int ed_digits = 0;               // digits typed into the current field so far
static lv_obj_t* box_h = nullptr;
static lv_obj_t* box_m = nullptr;
static lv_obj_t* lbl_h = nullptr;
static lv_obj_t* lbl_m = nullptr;
static lv_obj_t* btn_am = nullptr;
static lv_obj_t* btn_pm = nullptr;

static void dialogClose() {
    if (set_dialog) { lv_obj_del(set_dialog); set_dialog = nullptr; }
    box_h = box_m = lbl_h = lbl_m = btn_am = btn_pm = nullptr;
}
static void dialog_cancel_cb(lv_event_t* e) { (void)e; dialogClose(); }

static void dialog_rebuild_async(void* arg) { (void)arg; dialogClose(); dialogBuild(); }

static void dialog_countdown_cb(lv_event_t* e) {
    dlg_countdown = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    lv_async_call(dialog_rebuild_async, nullptr);       // rebuild outside the checkbox's own callback
}

static int maxHour() { return (ed_12h && !dlg_countdown) ? 12 : 23; }

static void fieldsShow() {
    if (!lbl_h) return;
    char b[8];
    snprintf(b, sizeof(b), (ed_12h && !dlg_countdown) ? "%d" : "%02d", ed_h); lv_label_set_text(lbl_h, b);
    snprintf(b, sizeof(b), "%02d", ed_m); lv_label_set_text(lbl_m, b);
    lv_obj_t* boxes[2] = {box_h, box_m};
    for (int i = 0; i < 2; i++) {
        bool on = i == ed_field;
        lv_obj_set_style_border_color(boxes[i], lv_color_hex(on ? 0xcba6f7 : 0x45475a), LV_PART_MAIN);
        lv_obj_set_style_border_width(boxes[i], on ? 3 : 1, LV_PART_MAIN);
        lv_obj_set_style_text_color(boxes[i], lv_color_hex(on ? 0xcba6f7 : 0xcdd6f4), LV_PART_MAIN);
    }
    if (btn_am) {
        lv_obj_set_style_bg_color(btn_am, lv_color_hex(ed_pm ? 0x313244 : 0xfab387), LV_PART_MAIN);
        lv_obj_set_style_bg_color(btn_pm, lv_color_hex(ed_pm ? 0xfab387 : 0x313244), LV_PART_MAIN);
        lv_obj_set_style_text_color(btn_am, lv_color_hex(ed_pm ? 0xa6adc8 : 0x11111b), LV_PART_MAIN);
        lv_obj_set_style_text_color(btn_pm, lv_color_hex(ed_pm ? 0x11111b : 0xa6adc8), LV_PART_MAIN);
    }
}

static void fieldSelect(int f) { ed_field = f; ed_digits = 0; fieldsShow(); }
static void box_h_cb(lv_event_t* e) { (void)e; fieldSelect(0); }
static void box_m_cb(lv_event_t* e) { (void)e; fieldSelect(1); }
static void am_cb(lv_event_t* e) { (void)e; ed_pm = false; fieldsShow(); }
static void pm_cb(lv_event_t* e) { (void)e; ed_pm = true; fieldsShow(); }

// A digit goes into the chosen field. The hour moves on to the minutes as soon as no second digit could follow.
static void typeDigit(int d) {
    if (ed_field == 0) {
        if (ed_digits == 0) {
            ed_h = d; ed_digits = 1;
            if (d * 10 > maxHour()) fieldSelect(1);
        } else {
            int v = ed_h * 10 + d;
            ed_h = v <= maxHour() ? v : d;
            fieldSelect(1);
        }
    } else {
        if (ed_digits == 0) { ed_m = d; ed_digits = d > 5 ? 0 : 1; }
        else { ed_m = ed_m * 10 + d; ed_digits = 0; }
    }
    fieldsShow();
}

static void keypad_cb(lv_event_t* e) {
    lv_obj_t* m = lv_event_get_target(e);
    uint16_t id = lv_btnmatrix_get_selected_btn(m);
    if (id == LV_BTNMATRIX_BTN_NONE) return;
    const char* t = lv_btnmatrix_get_btn_text(m, id);
    if (t == nullptr) return;
    if (t[0] >= '0' && t[0] <= '9' && t[1] == 0) { typeDigit(t[0] - '0'); return; }
    if (strcmp(t, LV_SYMBOL_BACKSPACE) == 0) {
        if (ed_field == 0) ed_h /= 10; else ed_m /= 10;
        ed_digits = 0;
        fieldsShow();
    } else {
        fieldSelect(1 - ed_field);                        // the arrow: the other field
    }
}

static void dialog_save_cb(lv_event_t* e) {
    (void)e;
    if (dlg_countdown) {
        dlg_cd_hours = ed_h; dlg_cd_minutes = ed_m;
        if (ed_h == 0 && ed_m == 0) return;               // nothing to count
        struct tm now;
        if (!localNow(&now)) return;
        alarmClock.setInHours(ed_h + ed_m / 60.0f, now);
    } else {
        int hour = ed_h;
        if (ed_12h) {
            if (hour == 0) hour = 12;
            hour = hour % 12 + (ed_pm ? 12 : 0);
        }
        alarmClock.setTime(hour, ed_m, true);
    }
    alarmClock.save();
    dialogClose();
    refreshAlarmUI();
}

static lv_obj_t* plainBtn(lv_obj_t* parent, const char* text, lv_event_cb_t cb, uint32_t color, int x, int y, int w, int h, const lv_font_t* f) {
    lv_obj_t* b = lv_btn_create(parent);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_radius(b, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(b, 0, LV_PART_MAIN);
    lv_obj_set_style_text_color(b, lv_color_hex(0x11111b), LV_PART_MAIN);
    lv_obj_set_style_text_font(b, f, LV_PART_MAIN);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
    return b;
}

static lv_obj_t* fieldBox(lv_obj_t* parent, lv_obj_t** label, lv_event_cb_t cb, int x, int y, int w, int h) {
    lv_obj_t* b = lv_obj_create(parent);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x1e1e2e), LV_PART_MAIN);
    lv_obj_set_style_radius(b, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(b, 0, LV_PART_MAIN);
    lv_obj_set_style_text_font(b, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
    *label = lv_label_create(b);
    lv_obj_center(*label);
    return b;
}

static void dialogBuild() {
    if (set_dialog) return;
    const AlarmConfig& c = alarmClock.cfg();
    ed_12h = !settings.getUse24HourFormat();
    if (dlg_countdown) { ed_h = dlg_cd_hours; ed_m = dlg_cd_minutes; }
    else if (ed_12h) { ed_h = c.hour % 12; if (ed_h == 0) ed_h = 12; ed_m = c.minute; ed_pm = c.hour >= 12; }
    else { ed_h = c.hour; ed_m = c.minute; }
    ed_field = 0; ed_digits = 0;

    const lv_font_t* fs = alarm_large ? &lv_font_montserrat_20 : &lv_font_montserrat_14;
    const lv_font_t* fm = alarm_large ? &lv_font_montserrat_28 : &lv_font_montserrat_20;
    int W = lv_disp_get_hor_res(nullptr), H = lv_disp_get_ver_res(nullptr);

    set_dialog = lv_obj_create(lv_layer_top());
    lv_obj_set_size(set_dialog, W, H);
    lv_obj_set_pos(set_dialog, 0, 0);
    lv_obj_set_style_bg_color(set_dialog, lv_color_hex(0x11111b), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(set_dialog, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(set_dialog, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(set_dialog, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(set_dialog, 0, LV_PART_MAIN);
    lv_obj_clear_flag(set_dialog, LV_OBJ_FLAG_SCROLLABLE);

    // Landscape: the fields and buttons on the left, the keypad on the right. Portrait: fields on top, keypad below.
    bool tall = H > W;
    int g = 6;
    int colW = tall ? W - 2 * g : W * 49 / 100;          // the left column (or the top block)
    int fieldW = (colW - 14) / 2, fieldH = alarm_large ? 72 : 56;

    lv_obj_t* title = lv_label_create(set_dialog);
    lv_label_set_text(title, dlg_countdown ? "Alarm in" : "Alarm at");
    lv_obj_set_style_text_font(title, fm, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(0xfab387), LV_PART_MAIN);
    lv_obj_set_pos(title, g + 2, g);

    int fy = g + (alarm_large ? 36 : 28);
    box_h = fieldBox(set_dialog, &lbl_h, box_h_cb, g, fy, fieldW, fieldH);
    lv_obj_t* colon = lv_label_create(set_dialog);
    lv_label_set_text(colon, ":");
    lv_obj_set_style_text_font(colon, fm, LV_PART_MAIN);
    lv_obj_set_style_text_color(colon, lv_color_hex(0xcdd6f4), LV_PART_MAIN);
    lv_obj_set_pos(colon, g + fieldW + 3, fy + fieldH / 2 - (alarm_large ? 16 : 12));
    box_m = fieldBox(set_dialog, &lbl_m, box_m_cb, g + fieldW + 14, fy, fieldW, fieldH);

    int ry = fy + fieldH + g + 2;                       // the row under the fields
    int rowH = alarm_large ? 50 : 40;
    btn_am = btn_pm = nullptr;
    if (dlg_countdown) {                                // name the two numbers
        const char* names[2] = {"hours", "minutes"};
        for (int i = 0; i < 2; i++) {
            lv_obj_t* u = lv_label_create(set_dialog);
            lv_label_set_text(u, names[i]);
            lv_obj_set_style_text_font(u, fs, LV_PART_MAIN);
            lv_obj_set_style_text_color(u, lv_color_hex(0xa6adc8), LV_PART_MAIN);
            lv_obj_set_width(u, fieldW);
            lv_obj_set_style_text_align(u, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
            lv_obj_set_pos(u, g + i * (fieldW + 14), ry + 2);
        }
    } else if (ed_12h) {
        btn_am = plainBtn(set_dialog, "AM", am_cb, 0x313244, g, ry, fieldW, rowH, fm);
        btn_pm = plainBtn(set_dialog, "PM", pm_cb, 0x313244, g + fieldW + 14, ry, fieldW, rowH, fm);
    }

    lv_obj_t* cd = lv_checkbox_create(set_dialog);
    lv_checkbox_set_text(cd, "Count down");
    lv_obj_set_style_text_font(cd, fs, LV_PART_MAIN);
    lv_obj_set_style_text_color(cd, lv_color_hex(0xcdd6f4), LV_PART_MAIN);
    lv_obj_set_style_pad_all(cd, 4, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(cd, lv_color_hex(0x313244), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(cd, lv_color_hex(0xa6e3a1), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(cd, lv_color_hex(0xa6e3a1), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_ext_click_area(cd, 10);
    if (dlg_countdown) lv_obj_add_state(cd, LV_STATE_CHECKED);
    lv_obj_add_event_cb(cd, dialog_countdown_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_set_pos(cd, g + 2, ry + rowH + g + 2);

    int kx, ky, kw, kh, by;                             // keypad box, and the y of Cancel / Save
    if (tall) {
        by = ry + rowH + g + (alarm_large ? 44 : 34);
        kx = g; ky = by + rowH + g; kw = W - 2 * g; kh = H - ky - g;
    } else {
        by = H - rowH - g;
        kx = colW + 2 * g; ky = g; kw = W - kx - g; kh = H - 2 * g;
    }
    plainBtn(set_dialog, "Cancel", dialog_cancel_cb, 0x6c7086, g, by, fieldW, rowH, fs);
    plainBtn(set_dialog, dlg_countdown ? "Start" : "Save", dialog_save_cb, 0xa6e3a1, g + fieldW + 14, by, fieldW, rowH, fm);

    static const char* keys[] = {"1", "2", "3", "\n", "4", "5", "6", "\n", "7", "8", "9", "\n",
                                 LV_SYMBOL_BACKSPACE, "0", LV_SYMBOL_RIGHT, ""};
    lv_obj_t* pad = lv_btnmatrix_create(set_dialog);
    lv_btnmatrix_set_map(pad, keys);
    lv_obj_set_pos(pad, kx, ky);
    lv_obj_set_size(pad, kw, kh);
    lv_obj_set_style_bg_opa(pad, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(pad, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(pad, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(pad, alarm_large ? 8 : 6, LV_PART_MAIN);
    lv_obj_set_style_pad_column(pad, alarm_large ? 8 : 6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(pad, lv_color_hex(0x313244), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(pad, lv_color_hex(0xcba6f7), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(pad, lv_color_hex(0xcdd6f4), LV_PART_ITEMS);
    lv_obj_set_style_text_color(pad, lv_color_hex(0x11111b), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_font(pad, &lv_font_montserrat_28, LV_PART_ITEMS);
    lv_obj_set_style_radius(pad, 10, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(pad, 0, LV_PART_ITEMS);
    lv_obj_set_style_border_width(pad, 0, LV_PART_ITEMS);
    lv_obj_add_event_cb(pad, keypad_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    fieldsShow();
}

static void set_cb(lv_event_t* e) {
    (void)e;
    if (!dlg_countdown && alarmClock.cfg().enabled && !alarmClock.cfg().repeat) dlg_countdown = true;   // a count down is running: stay in that mode
    dialogBuild();
}

// For layout checks over the web API
void showAlarmSetDialog(bool countdown) { dialogClose(); dlg_countdown = countdown; dialogBuild(); }
void closeAlarmSetDialog() { dialogClose(); }

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
    lv_label_set_text(song, "Wake up");
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

// The alarm signal is the beep: no card, no decoder, so it cannot fail.
static void playAlarmSound(bool loop) {
    audio.setVolume(alarmClock.cfg().volume);
    audio.playBeep(loop);
}

void testPlayAlarmSound(bool loop) { playAlarmSound(loop); }

// An alarm nobody answers snoozes itself after 5 minutes, and stops for good after the third time.
static uint32_t ring_t0 = 0;
static lv_timer_t* ring_guard = nullptr;
static void ring_guard_cb(lv_timer_t* t) {
    (void)t;
    if (!isAlarmRingingShown() || millis() - ring_t0 < 5UL * 60UL * 1000UL) return;
    if (++auto_snoozes >= 3) { eventLog("alarm: nobody answered three times, stopped"); ringingStop(); }
    else { eventLog("alarm: nobody answered for 5 min, snoozing"); ringingSnooze(); }
}

void startAlarmRinging() {
    const AlarmConfig& c = alarmClock.cfg();
    eventLog("alarm: ringing (volume %d%%)", c.volume);
    screensaver.wake();
    playAlarmSound(true);
    ring_t0 = millis();
    if (!ring_guard) ring_guard = lv_timer_create(ring_guard_cb, 2000, nullptr);
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
        eventLog("test sound: start");
        playAlarmSound(false);
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
    const lv_font_t* f = isLargeScreen ? &lv_font_montserrat_20 : &lv_font_montserrat_14;

    // One full-width card: the volume with its slider above, the test button below
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(card, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(card, isLargeScreen ? 10 : 6, LV_PART_MAIN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* head = lv_obj_create(card);   // "Alarm volume" on the left, the percentage on the right
    lv_obj_set_size(head, lv_pct(96), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(head, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(head, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(head, 0, LV_PART_MAIN);
    lv_obj_clear_flag(head, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t* title = lv_label_create(head);
    lv_label_set_text(title, "Alarm volume");
    lv_obj_set_style_text_font(title, f, LV_PART_MAIN);
    lv_obj_set_style_text_color(title, lv_color_hex(0xcdd6f4), LV_PART_MAIN);
    vol_lbl = lv_label_create(head);
    lv_obj_set_style_text_font(vol_lbl, f, LV_PART_MAIN);
    lv_obj_set_style_text_color(vol_lbl, lv_color_hex(0xcba6f7), LV_PART_MAIN);
    vol_text();

    lv_obj_t* sl = lv_slider_create(card);
    lv_obj_set_size(sl, lv_pct(92), isLargeScreen ? 16 : 12);
    lv_slider_set_range(sl, 0, 100);
    lv_slider_set_value(sl, alarmClock.cfg().volume, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(sl, lv_color_hex(0x45475a), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sl, lv_color_hex(0xcba6f7), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sl, lv_color_hex(0xf5c2e7), LV_PART_KNOB);
    lv_obj_set_style_pad_all(sl, 4, LV_PART_KNOB);
    lv_obj_set_ext_click_area(sl, 12);
    lv_obj_add_event_cb(sl, vol_cb, LV_EVENT_ALL, nullptr);

    test_btn = lv_btn_create(card);
    lv_obj_set_size(test_btn, lv_pct(100), isLargeScreen ? 52 : 38);
    lv_obj_set_style_radius(test_btn, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(test_btn, 0, LV_PART_MAIN);
    test_lbl = lv_label_create(test_btn);
    lv_obj_set_style_text_font(test_lbl, f, LV_PART_MAIN);
    lv_obj_set_style_text_color(test_lbl, lv_color_hex(0x11111b), LV_PART_MAIN);
    lv_obj_center(test_lbl);
    lv_obj_add_event_cb(test_btn, test_cb, LV_EVENT_CLICKED, nullptr);
    test_update_label();

    test_timer = lv_timer_create(test_timer_cb, 500, nullptr);
}
