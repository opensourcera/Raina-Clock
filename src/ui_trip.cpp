#include "ui_trip.h"
#include "ui_alarm.h"
#include "ui.h"
#include "time_utils.h"
#include "settings_manager.h"

extern SettingsManager settings;

// The clock tab: black background, lilac digits in Electronic Highway Sign (a square-pixel font from dafont).
// Hours and minutes sit tight in one row, the seconds on the next line, the colon always on. While this tab is showing,
// the header bar is hidden (setClockFullscreen) so the digits can be as large as the screen allows. The alarm bar is at
// the bottom.

LV_FONT_DECLARE(font_ehs_108);
LV_FONT_DECLARE(font_ehs_64);
LV_FONT_DECLARE(font_ehs_56);

static const uint32_t LILAC = 0xC9A9F5;

static lv_obj_t* clock_tab = nullptr;
static lv_obj_t* clock_tabview = nullptr;
static int clock_index = -1;
static lv_obj_t* hour_lbl = nullptr;
static lv_obj_t* min_lbl = nullptr;
static lv_obj_t* sec_lbl = nullptr;
static lv_timer_t* clock_timer = nullptr;
static char hour_text[8] = "";
static char min_text[8] = "";
static char sec_text[8] = "";
static bool fullscreen_on = false;

static void clock_tick(lv_timer_t* t) {
    (void)t;
    if (clock_tab == nullptr || clock_tabview == nullptr) return;
    bool on_clock = (int)lv_tabview_get_tab_act(clock_tabview) == clock_index;
    if (on_clock != fullscreen_on) { fullscreen_on = on_clock; setClockFullscreen(on_clock); }   // header bar away on this tab only
    if (!on_clock) return;

    struct tm ti;
    if (!getLocalTimeWrapper(&ti, 0)) return;
    bool use24 = settings.getUse24HourFormat();
    int hour = use24 ? ti.tm_hour : (ti.tm_hour % 12 == 0 ? 12 : ti.tm_hour % 12);
    char hh[8], mm[8], ss[8];
    lv_snprintf(hh, sizeof(hh), use24 ? "%02d" : "%d", hour);
    lv_snprintf(mm, sizeof(mm), "%02d", ti.tm_min);
    lv_snprintf(ss, sizeof(ss), "%02d", ti.tm_sec);
    if (strcmp(hh, hour_text) != 0) { strcpy(hour_text, hh); lv_label_set_text(hour_lbl, hh); }
    if (strcmp(mm, min_text) != 0)  { strcpy(min_text, mm);  lv_label_set_text(min_lbl, mm); }
    if (strcmp(ss, sec_text) != 0)  { strcpy(sec_text, ss);  lv_label_set_text(sec_lbl, ss); }

    static uint16_t countdown = 399;   // so the first tick with a valid time refreshes the alarm line at once
    if (++countdown >= 400) { countdown = 0; refreshAlarmUI(); }   // the time-left text in the alarm bar, about every 30 s
}

static lv_obj_t* digits(lv_obj_t* parent, const lv_font_t* font, const char* text) {
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(LILAC), LV_PART_MAIN);
    lv_label_set_text(l, text);
    return l;
}

void createTripTab(lv_obj_t* tab, lv_obj_t* tabview, int tabIndex, bool isLandscape, bool isLargeScreen) {
    (void)isLandscape;
    if (clock_timer) { lv_timer_del(clock_timer); clock_timer = nullptr; }
    clock_tab = tab; clock_tabview = tabview; clock_index = tabIndex;
    hour_text[0] = min_text[0] = sec_text[0] = 0;
    fullscreen_on = false;

    lv_obj_set_style_pad_all(tab, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(tab, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(tab, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(tab, LV_SCROLLBAR_MODE_OFF);

    bool wide = lv_disp_get_hor_res(NULL) >= 300;       // portrait (240 px) falls back to the 56 px size
    const lv_font_t* big = wide ? &font_ehs_108 : &font_ehs_56;
    const lv_font_t* small = wide ? &font_ehs_64 : &font_ehs_56;

    lv_obj_t* stack = lv_obj_create(tab);
    lv_obj_set_size(stack, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(stack, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(stack, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(stack, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(stack, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(stack, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(stack, 2, LV_PART_MAIN);
    lv_obj_clear_flag(stack, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(stack, LV_ALIGN_TOP_MID, 0, 2);

    lv_obj_t* row = lv_obj_create(stack);                // hour, colon, minutes: packed, no gaps
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(row, 0, LV_PART_MAIN);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    hour_lbl = digits(row, big, "--");

    // The font's colon sits in a full-width cell; a narrow box with the colon centred and allowed to overflow packs it
    lv_obj_t* colon_box = lv_obj_create(row);
    lv_obj_set_size(colon_box, wide ? 36 : 20, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(colon_box, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(colon_box, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(colon_box, 0, LV_PART_MAIN);
    lv_obj_clear_flag(colon_box, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(colon_box, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_t* colon = digits(colon_box, big, ":");
    lv_obj_align(colon, LV_ALIGN_TOP_MID, 0, 0);

    min_lbl = digits(row, big, "--");

    sec_lbl = digits(stack, small, "--");                // the seconds on the next line

    createAlarmBar(tab, isLargeScreen);

    clock_timer = lv_timer_create(clock_tick, 70, nullptr);
}

void tripSetTime(const char* time_str) {
    if (time_str == nullptr) return;
    alarmRingingSetTime(time_str);   // the ringing view still follows the header clock
}
