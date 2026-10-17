#include "ui_trip.h"
#include "ui_alarm.h"
#include "time_utils.h"
#include "settings_manager.h"
#include <math.h>

extern SettingsManager settings;

// The clock tab: a Bitcount Single LED-style time over a slowly turning colour field, with the alarm bar below.

#define TRIP_BLOBS 4
#define TRIP_PERIOD_MS 70

LV_FONT_DECLARE(font_bitcount_56);

static lv_obj_t* trip_tab = nullptr;
static lv_obj_t* trip_tabview = nullptr;
static int trip_index = -1;
static lv_obj_t* blobs[TRIP_BLOBS] = {nullptr};
static lv_obj_t* time_shadow = nullptr;
static lv_obj_t* time_lbl = nullptr;
static lv_obj_t* date_lbl = nullptr;
static lv_timer_t* trip_timer = nullptr;
static uint16_t hue = 0;
static uint32_t last_date_ms = 0;
static bool trip_large = false;
static int time_y = -28;
static char time_text[16] = "";   // what the big label shows now: HH:MM:SS, colons blinking

static lv_color_t hsv(int h, uint8_t s, uint8_t v) {
    h %= 360; if (h < 0) h += 360;
    return lv_color_hsv_to_rgb((uint16_t)h, s, v);
}

static void trip_tick(lv_timer_t* t) {
    (void)t;
    if (trip_tab == nullptr || trip_tabview == nullptr) return;
    if ((int)lv_tabview_get_tab_act(trip_tabview) != trip_index) return;   // not on screen: nothing to animate

    hue = (hue + 2) % 360;
    lv_obj_set_style_bg_color(trip_tab, hsv(hue, 85, 55), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(trip_tab, hsv(hue + 70, 90, 40), LV_PART_MAIN);

    int w = lv_obj_get_content_width(trip_tab), h = lv_obj_get_content_height(trip_tab);
    if (w <= 0 || h <= 0) return;
    float s = millis() / 1000.0f;
    for (int k = 0; k < TRIP_BLOBS; k++) {
        if (blobs[k] == nullptr) continue;
        int size = lv_obj_get_width(blobs[k]);
        float a = s * (0.35f + 0.13f * k) + k * 1.9f;
        float b = s * (0.27f + 0.11f * k) + k * 0.7f;
        int x = (int)((w - size) * (0.5f + 0.5f * sinf(a)));
        int y = (int)((h - size) * (0.5f + 0.5f * cosf(b)));
        lv_obj_set_pos(blobs[k], x, y);
        lv_obj_set_style_bg_color(blobs[k], hsv(hue + 90 * k + 45, 95, 100), LV_PART_MAIN);
    }
    if (time_lbl) lv_obj_set_style_text_color(time_lbl, hsv(hue + 180, 20, 100), LV_PART_MAIN);
    if (date_lbl) lv_obj_set_style_text_color(date_lbl, hsv(hue + 180, 20, 95), LV_PART_MAIN);

    uint32_t now = millis();
    struct tm ti;
    bool have_time = getLocalTimeWrapper(&ti, 0);
    if (have_time && time_lbl) {
        bool use24 = settings.getUse24HourFormat();
        int hour = use24 ? ti.tm_hour : (ti.tm_hour % 12 == 0 ? 12 : ti.tm_hour % 12);
        char colon = ((now / 500) & 1) ? ' ' : ':';   // blinks once a second; same width as a digit in Bitcount
        char buf[16];
        lv_snprintf(buf, sizeof(buf), use24 ? "%02d%c%02d%c%02d" : "%2d%c%02d%c%02d", hour, colon, ti.tm_min, colon, ti.tm_sec);
        if (strcmp(buf, time_text) != 0) {
            strcpy(time_text, buf);
            lv_label_set_text(time_lbl, buf);
            lv_label_set_text(time_shadow, buf);
        }
    }
    if (date_lbl && now - last_date_ms > 1000) {
        last_date_ms = now;
        if (have_time) {
            static const char* days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
            static const char* months[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
            char buf[48];
            if (settings.getUse24HourFormat()) lv_snprintf(buf, sizeof(buf), "%s, %s %d", days[ti.tm_wday % 7], months[ti.tm_mon % 12], ti.tm_mday);
            else lv_snprintf(buf, sizeof(buf), "%s  -  %s, %s %d", ti.tm_hour < 12 ? "AM" : "PM", days[ti.tm_wday % 7], months[ti.tm_mon % 12], ti.tm_mday);
            lv_label_set_text(date_lbl, buf);
        }
        static uint8_t minute_tick = 0;
        if (++minute_tick >= 30) { minute_tick = 0; refreshAlarmUI(); }   // the countdown in the alarm bar
    }
}

void createTripTab(lv_obj_t* tab, lv_obj_t* tabview, int tabIndex, bool isLandscape, bool isLargeScreen) {
    (void)isLandscape;
    if (trip_timer) { lv_timer_del(trip_timer); trip_timer = nullptr; }
    trip_tab = tab; trip_tabview = tabview; trip_index = tabIndex; trip_large = isLargeScreen;
    time_y = isLargeScreen ? -40 : -28;

    lv_obj_set_style_pad_all(tab, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(tab, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(tab, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(tab, LV_SCROLLBAR_MODE_OFF);

    int blob_size = isLargeScreen ? 150 : 90;
    for (int k = 0; k < TRIP_BLOBS; k++) {
        blobs[k] = lv_obj_create(tab);
        lv_obj_set_size(blobs[k], blob_size, blob_size);
        lv_obj_set_style_radius(blobs[k], LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_border_width(blobs[k], 0, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(blobs[k], 150, LV_PART_MAIN);
        lv_obj_clear_flag(blobs[k], LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    }

    // The time in Bitcount Single: a dark copy 3 px behind gives it depth
    time_shadow = lv_label_create(tab);
    lv_obj_set_style_text_font(time_shadow, &font_bitcount_56, LV_PART_MAIN);
    lv_obj_set_style_text_color(time_shadow, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_opa(time_shadow, 130, LV_PART_MAIN);
    lv_label_set_text(time_shadow, "--:--:--");
    lv_obj_align(time_shadow, LV_ALIGN_CENTER, 3, time_y + 3);

    time_lbl = lv_label_create(tab);
    lv_obj_set_style_text_font(time_lbl, &font_bitcount_56, LV_PART_MAIN);
    lv_obj_set_style_text_color(time_lbl, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(time_lbl, "--:--:--");
    lv_obj_align(time_lbl, LV_ALIGN_CENTER, 0, time_y);

    date_lbl = lv_label_create(tab);
    lv_obj_set_style_text_font(date_lbl, isLargeScreen ? &lv_font_montserrat_20 : &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(date_lbl, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(date_lbl, "");
    lv_obj_align(date_lbl, LV_ALIGN_CENTER, 0, time_y + (isLargeScreen ? 52 : 40));

    createAlarmBar(tab, isLargeScreen);

    last_date_ms = 0;
    trip_timer = lv_timer_create(trip_tick, TRIP_PERIOD_MS, nullptr);
}

void tripSetTime(const char* time_str) {
    if (time_str == nullptr) return;
    alarmRingingSetTime(time_str);   // the big label keeps its own seconds in trip_tick()
}
