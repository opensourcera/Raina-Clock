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


// ---- the Set popup: the time of day (every day), or a count down (hours and minutes from now). The numbers are typed
// on a big keypad; tap the hour or the minutes to choose which one the keys change.

static bool dlg_countdown = false;      // remembered between openings
static String dlg_name;                 // the alarm's name while the popup is open; kept on Save
static lv_obj_t* name_lbl = nullptr;    // the text on the Info button
static void nameWindowClose();
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
    box_h = box_m = lbl_h = lbl_m = btn_am = btn_pm = name_lbl = nullptr;
    nameWindowClose();
}
static void dialog_cancel_cb(lv_event_t* e) { (void)e; dialogClose(); }
static void dialog_off_cb(lv_event_t* e) {
    (void)e;
    alarmClock.setEnabled(false);
    if (alarmClock.isSnoozed()) alarmClock.stop();
    alarmClock.save();
    dialogClose();
    refreshAlarmUI();
}

static void dialog_rebuild_async(void* arg) { (void)arg; dialogClose(); dialogBuild(); }

// The mode toggle: the down arrow is a count down, the clock a time of day
static void mode_cb(lv_event_t* e) {
    bool cd = (intptr_t)lv_event_get_user_data(e) != 0;
    if (cd == dlg_countdown) return;
    dlg_countdown = cd;
    lv_async_call(dialog_rebuild_async, nullptr);       // rebuild outside the button's own callback
}

// ---- the name: typed on a big keyboard of lower case letters, in a window over the Set popup

static lv_obj_t* name_win = nullptr;
static lv_obj_t* name_ta = nullptr;

static void nameButtonText() {
    if (name_lbl == nullptr) return;
    if (dlg_name.length()) lv_label_set_text(name_lbl, dlg_name.c_str());
    else lv_label_set_text(name_lbl, "Name");
}

static void nameWindowClose() {
    if (name_win) { lv_obj_del_async(name_win); name_win = nullptr; name_ta = nullptr; }
}

static lv_obj_t* winBtn(lv_obj_t* parent, const char* text, lv_event_cb_t cb, uint32_t color, int x, int y, int w, int h) {
    lv_obj_t* b = lv_btn_create(parent);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_radius(b, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(b, 0, LV_PART_MAIN);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, alarm_large ? &lv_font_montserrat_28 : &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(0x11111b), LV_PART_MAIN);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
    return b;
}

static void nameWindowOpen() {
    if (name_win) return;
    int W = lv_disp_get_hor_res(nullptr), H = lv_disp_get_ver_res(nullptr);
    name_win = lv_obj_create(lv_layer_top());
    lv_obj_set_size(name_win, W, H);
    lv_obj_set_pos(name_win, 0, 0);
    lv_obj_set_style_bg_color(name_win, lv_color_hex(0x1e1e2e), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(name_win, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(name_win, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(name_win, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(name_win, 0, LV_PART_MAIN);
    lv_obj_clear_flag(name_win, LV_OBJ_FLAG_SCROLLABLE);

    // The box on top, beside a back arrow; the letters in the middle; Space, Delete and OK on their own row below
    int th = alarm_large ? 48 : 36, ch = alarm_large ? 58 : 46, gap = 6;
    int backW = th + 8;
    winBtn(name_win, LV_SYMBOL_LEFT, [](lv_event_t*) { nameWindowClose(); }, 0x6c7086, 4, 3, backW, th);
    name_ta = lv_textarea_create(name_win);
    lv_obj_set_size(name_ta, W - backW - 12, th);
    lv_obj_set_pos(name_ta, backW + 8, 3);
    lv_textarea_set_one_line(name_ta, true);
    lv_textarea_set_max_length(name_ta, 20);
    lv_textarea_set_accepted_chars(name_ta, "abcdefghijklmnopqrstuvwxyz ");
    lv_textarea_set_placeholder_text(name_ta, "Alarm name, e.g. school");
    lv_textarea_set_text(name_ta, dlg_name.c_str());
    lv_obj_set_style_text_font(name_ta, alarm_large ? &lv_font_montserrat_20 : &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_add_state(name_ta, LV_STATE_FOCUSED);        // shows the cursor

    static const char* letters[] = {"q","w","e","r","t","y","u","i","o","p","\n","a","s","d","f","g","h","j","k","l","\n","z","x","c","v","b","n","m",""};
    static lv_btnmatrix_ctrl_t ctrl[28];
    for (int i = 0; i < 28; i++) ctrl[i] = (lv_btnmatrix_ctrl_t)1;
    int ky = th + 3 + gap, kh = H - ky - ch - 2 * gap;
    lv_obj_t* kb = lv_keyboard_create(name_win);
    lv_obj_set_size(kb, W, kh);
    lv_obj_align(kb, LV_ALIGN_TOP_LEFT, 0, ky);       // lv_keyboard_create aligns it to the bottom
    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_USER_1, letters, ctrl);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_USER_1);
    lv_obj_set_style_bg_color(kb, lv_color_hex(0x11111b), LV_PART_MAIN);
    lv_obj_set_style_pad_all(kb, 3, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(kb, 4, LV_PART_MAIN);
    lv_obj_set_style_border_width(kb, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(kb, lv_color_hex(0x45475a), LV_PART_ITEMS);
    lv_obj_set_style_text_color(kb, lv_color_hex(0xcdd6f4), LV_PART_ITEMS);
    lv_obj_set_style_text_font(kb, alarm_large ? &lv_font_montserrat_28 : &lv_font_montserrat_20, LV_PART_ITEMS);
    lv_obj_set_style_radius(kb, 8, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(kb, lv_color_hex(0xfab387), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(kb, lv_color_hex(0x11111b), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_keyboard_set_textarea(kb, name_ta);

    // Space (wide), Delete, then OK set apart from Delete so a tap on one never lands on the other
    int cy = H - ch - gap, gapS = 8, gapOk = 22;
    int bw = (W - 8 - gapS - gapOk) / 6;
    winBtn(name_win, "Space", [](lv_event_t*) { if (name_ta) lv_textarea_add_char(name_ta, ' '); }, 0xcdd6f4, 4, cy, bw * 3, ch);
    winBtn(name_win, LV_SYMBOL_BACKSPACE, [](lv_event_t*) { if (name_ta) lv_textarea_del_char(name_ta); }, 0xfab387, 4 + bw * 3 + gapS, cy, bw, ch);
    winBtn(name_win, LV_SYMBOL_OK, [](lv_event_t*) {
        if (name_ta) { dlg_name = String(lv_textarea_get_text(name_ta)); dlg_name.trim(); }
        nameButtonText();
        nameWindowClose();
    }, 0xa6e3a1, 4 + bw * 4 + gapS + gapOk, cy, bw * 2, ch);
}

static void name_cb(lv_event_t* e) { (void)e; nameWindowOpen(); }

// A small clock face for the toggle (the symbol font has none)
static void clockFace(lv_obj_t* parent, int d, uint32_t color) {
    lv_obj_t* c = lv_obj_create(parent);
    lv_obj_set_size(c, d, d);
    lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(c, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(c, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_pad_all(c, 0, LV_PART_MAIN);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(c);
    static lv_point_t hands[3];                         // the points must outlive the line; every face is the same size
    int m = d / 2 - 2;                                  // the centre, inside the border
    hands[0].x = m; hands[0].y = 3; hands[1].x = m; hands[1].y = m; hands[2].x = m + d / 4; hands[2].y = m;
    lv_obj_t* l = lv_line_create(c);
    lv_line_set_points(l, hands, 3);
    lv_obj_set_style_line_width(l, 2, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(l, true, LV_PART_MAIN);
    lv_obj_set_style_line_color(l, lv_color_hex(color), LV_PART_MAIN);
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
    alarmClock.setName(dlg_name);
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
    if (alarmClock.cfg().enabled || alarmClock.isSnoozed()) {
        int ow = alarm_large ? 64 : 50;
        plainBtn(set_dialog, "Off", dialog_off_cb, 0xf38ba8, colW + g - ow, g, ow, alarm_large ? 32 : 24, fs);
    }

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

    // The mode toggle (down arrow: count down | clock: time of day), then the Info button that names the alarm
    int ty = ry + rowH + g, tH = alarm_large ? 40 : 32, segW = alarm_large ? 52 : 40;
    for (int i = 0; i < 2; i++) {
        bool cd = i == 0, on = cd == dlg_countdown;
        uint32_t fg = on ? 0x11111b : 0xa6adc8;
        lv_obj_t* b = lv_btn_create(set_dialog);
        lv_obj_set_pos(b, g + i * segW, ty);
        lv_obj_set_size(b, segW, tH);
        lv_obj_set_style_bg_color(b, lv_color_hex(on ? 0xcba6f7 : 0x313244), LV_PART_MAIN);
        lv_obj_set_style_radius(b, 8, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(b, 0, LV_PART_MAIN);
        lv_obj_set_ext_click_area(b, 4);
        if (cd) {
            lv_obj_t* l = lv_label_create(b);
            lv_label_set_text(l, LV_SYMBOL_DOWN);
            lv_obj_set_style_text_font(l, fm, LV_PART_MAIN);
            lv_obj_set_style_text_color(l, lv_color_hex(fg), LV_PART_MAIN);
            lv_obj_center(l);
        } else {
            clockFace(b, tH - 12, fg);
        }
        lv_obj_add_event_cb(b, mode_cb, LV_EVENT_CLICKED, (void*)(intptr_t)(cd ? 1 : 0));
    }
    int nx = g + 2 * segW + g;
    lv_obj_t* nb = lv_btn_create(set_dialog);
    lv_obj_set_pos(nb, nx, ty);
    lv_obj_set_size(nb, colW + g - nx, tH);
    lv_obj_set_style_bg_color(nb, lv_color_hex(0x89b4fa), LV_PART_MAIN);
    lv_obj_set_style_radius(nb, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(nb, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(nb, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(nb, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(nb, name_cb, LV_EVENT_CLICKED, nullptr);
    name_lbl = lv_label_create(nb);
    lv_label_set_long_mode(name_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(name_lbl, lv_pct(100));
    lv_obj_set_height(name_lbl, lv_font_get_line_height(fs));     // one line, cut with dots
    lv_obj_set_style_text_align(name_lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(name_lbl, fs, LV_PART_MAIN);
    lv_obj_set_style_text_color(name_lbl, lv_color_hex(0x11111b), LV_PART_MAIN);
    lv_obj_center(name_lbl);
    nameButtonText();

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
    dlg_name = alarmClock.cfg().name;
    dialogBuild();
}

// For layout checks over the web API
void showAlarmSetDialog(bool countdown) { dialogClose(); dlg_countdown = countdown; dlg_name = alarmClock.cfg().name; dialogBuild(); }
void showAlarmNameWindow() { if (set_dialog) nameWindowOpen(); }
void closeAlarmSetDialog() { dialogClose(); }

// ---- the bar on the clock tab

static lv_obj_t* bell_lbl = nullptr;
static lv_obj_t* bell_slash = nullptr;

// The Clock page shows only a bell at the bottom left: slashed when no alarm is set, otherwise with the alarm time and
// the time left beside it in small type. A tap on it opens the Set popup.
void createAlarmBar(lv_obj_t* parent, bool isLargeScreen) {
    alarm_large = isLargeScreen;
    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_set_size(bar, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bar, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_column(bar, 8, LV_PART_MAIN);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_ext_click_area(bar, 12);
    lv_obj_add_event_cb(bar, set_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_LEFT, 2, -2);

    lv_obj_t* bell_box = lv_obj_create(bar);             // the bell, with the slash drawn over it
    int bs = isLargeScreen ? 40 : 32;
    lv_obj_set_size(bell_box, bs, bs);
    lv_obj_set_style_bg_opa(bell_box, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(bell_box, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(bell_box, 0, LV_PART_MAIN);
    lv_obj_clear_flag(bell_box, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t* bell = lv_label_create(bell_box);
    lv_label_set_text(bell, LV_SYMBOL_BELL);
    lv_obj_set_style_text_font(bell, &lv_font_montserrat_28, LV_PART_MAIN);
    lv_obj_center(bell);
    static lv_point_t slash_pts[2];
    slash_pts[0].x = 3; slash_pts[0].y = 3; slash_pts[1].x = bs - 3; slash_pts[1].y = bs - 3;
    bell_slash = lv_line_create(bell_box);
    lv_line_set_points(bell_slash, slash_pts, 2);
    lv_obj_set_style_line_width(bell_slash, 3, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(bell_slash, true, LV_PART_MAIN);
    lv_obj_set_style_line_color(bell_slash, lv_color_hex(0xf38ba8), LV_PART_MAIN);
    bell_lbl = bell;

    alarm_label = lv_label_create(bar);
    lv_obj_set_style_text_font(alarm_label, isLargeScreen ? &lv_font_montserrat_20 : &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(alarm_label, lv_color_hex(0xbac2de), LV_PART_MAIN);
    lv_label_set_text(alarm_label, "");
    refreshAlarmUI();
}

void refreshAlarmUI() {
    if (alarm_label == nullptr || bell_lbl == nullptr) return;
    const AlarmConfig& c = alarmClock.cfg();
    bool use24 = settings.getUse24HourFormat();
    int snz = alarmClock.snoozeMinuteOfDay();
    bool on = c.enabled || snz >= 0;
    if (on) lv_obj_add_flag(bell_slash, LV_OBJ_FLAG_HIDDEN); else lv_obj_clear_flag(bell_slash, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_color(bell_lbl, lv_color_hex(on ? 0xf9e2af : 0x6c7086), LV_PART_MAIN);
    if (!on) { lv_label_set_text(alarm_label, ""); return; }
    int target = snz >= 0 ? snz : c.hour * 60 + c.minute;
    String text = AlarmManager::formatTime(target / 60, target % 60, use24);
    if (snz >= 0) text = "Snoozed " + text;
    struct tm now;
    if (localNow(&now)) {
        int left = target - (now.tm_hour * 60 + now.tm_min);
        if (left <= 0) left += 1440;
        char b[16];
        snprintf(b, sizeof(b), "  in %d:%02d", left / 60, left % 60);
        text += b;
    }
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

    // The alarm's name in bold (the same text twice, a pixel apart: there is no bold font), or a plain "Wake up"
    const String& nm = alarmClock.cfg().name;
    if (nm.length()) {
        int W = lv_disp_get_hor_res(nullptr);
        for (int i = 0; i < 2; i++) {
            lv_obj_t* l = lv_label_create(ring_overlay);
            lv_label_set_text(l, nm.c_str());
            lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
            lv_obj_set_width(l, W - 20);
            lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
            lv_obj_set_style_text_font(l, alarm_large ? &lv_font_montserrat_48 : &lv_font_montserrat_28, LV_PART_MAIN);
            lv_obj_set_style_text_color(l, lv_color_hex(0xf5e0dc), LV_PART_MAIN);
            lv_obj_align(l, LV_ALIGN_CENTER, i, alarm_large ? 36 : 20);
        }
    } else {
        lv_obj_t* song = lv_label_create(ring_overlay);
        lv_label_set_text(song, "Wake up");
        lv_obj_set_style_text_font(song, alarm_large ? &lv_font_montserrat_20 : &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(song, lv_color_hex(0xcdd6f4), LV_PART_MAIN);
        lv_obj_align(song, LV_ALIGN_CENTER, 0, 10);
    }

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
    snprintf(b, sizeof(b), "%d%%", alarmClock.cfg().volume);
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
    lv_obj_set_size(sl, lv_pct(86), isLargeScreen ? 16 : 12);
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
