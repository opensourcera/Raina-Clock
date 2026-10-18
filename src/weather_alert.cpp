#include "weather_alert.h"
#include "ui_weather.h"
#include "settings_manager.h"
#include "time_utils.h"
#include "config/config.h"
#include "event_log.h"

extern SettingsManager settings;
LV_FONT_DECLARE(weather_icons_24);

// The messages, most urgent first. %d in "rain soon" is the minutes until the rainy hour begins.
enum Kind { STORM_NOW, STORM_SOON, SNOW_SOON, RAIN_NOW, RAIN_SOON, RAIN_LATER, FREEZING, COLDER, WARMER, HOT, FOG, NICE, KINDS };
struct Msg { const char* text; int icon; uint32_t bubble; bool blink; };
static const Msg MSGS[KINDS] = {
    {"Storm now. Stay inside!",       95, 0x6e2a3c, true},
    {"Storm coming. Stay inside",     95, 0x6e2a3c, true},
    {"Snow soon. Dress warm",         73, 0x2b4a78, true},
    {"Raining. Take an umbrella",     63, 0x2b5278, false},
    {"Rain in ~%d min. Umbrella!",    61, 0x2b5278, true},
    {"Rain later. Pack an umbrella",  61, 0x2b5278, false},
    {"Freezing night. Dress warm",    71, 0x2b4a78, true},
    {"Getting colder. Grab a jacket",  3, 0x3a3f5c, false},
    {"Warming up. Dress light",        1, 0x5c4a2b, false},
    {"Hot today. Drink water",         0, 0x6e4a2b, false},
    {"Foggy. Drive carefully",        45, 0x3a3f5c, false},
    {"Nice weather. Enjoy the day",    0, 0x2b5c45, false},
};

static bool isStorm(int c) { return c >= 95 && c <= 99; }
static bool isSnow(int c) { return (c >= 71 && c <= 77) || c == 85 || c == 86; }
static bool isRain(int c) { return (c >= 51 && c <= 67) || (c >= 80 && c <= 82); }
static bool isFog(int c) { return c == 45 || c == 48; }

// The next 12 hours, kept from the last forecast so the message can follow the clock between updates
static const int HOURS = 12;
static HourlyForecast hours[24];
static int cur_code = -1;
static bool have_data = false;
static int demo = -1;

static lv_obj_t* bubble = nullptr;
static lv_obj_t* icon = nullptr;
static lv_obj_t* text = nullptr;
static bool blinking = false;

// Index of the current hour in hours[] (the forecast may be an hour or two old), -1 when it is too old
static int nowIndex(const struct tm& now) {
    for (int i = 0; i < 24 - HOURS; i++) if (hours[i].hour == now.tm_hour) return i;
    return hours[0].hour < 0 ? 0 : -1;
}

static int pick(int* minutes) {
    struct tm now;
    if (!getLocalTimeWrapper(&now, 0)) return -1;
    int s = nowIndex(now);
    if (s < 0) return -1;
    const HourlyForecast* h = hours + s;
    bool imperial = settings.getUnitSystem() == UNIT_IMPERIAL;
    float freezing = imperial ? 32 : 0, swing = imperial ? 10 : 6, hot = imperial ? 95 : 35;
    auto wet = [&](int i) { return isRain(h[i].weatherCode) || h[i].precipitationProbability >= 60; };
    int code0 = cur_code >= 0 ? cur_code : h[0].weatherCode;

    if (isStorm(code0)) return STORM_NOW;
    for (int i = 1; i <= 3; i++) if (isStorm(h[i].weatherCode)) return STORM_SOON;
    for (int i = 0; i <= 3; i++) if (isSnow(h[i].weatherCode)) return SNOW_SOON;
    if (isRain(code0)) return RAIN_NOW;
    for (int i = 1; i <= 2; i++) if (wet(i)) { *minutes = (60 - now.tm_min) + (i - 1) * 60; return RAIN_SOON; }
    for (int i = 3; i <= 8; i++) if (wet(i)) return RAIN_LATER;
    for (int i = 0; i < HOURS; i++) {
        int hr = h[i].hour;
        if ((hr >= 18 || hr <= 7) && h[i].temperature <= freezing) return FREEZING;
    }
    float d = h[3].temperature - h[0].temperature;
    if (d <= -swing) return COLDER;
    if (d >= swing) return WARMER;
    for (int i = 0; i <= 8; i++) if (h[i].temperature >= hot) return HOT;
    if (isFog(code0)) return FOG;
    return NICE;
}

static void blink_cb(lv_timer_t* t) {
    (void)t;
    if (!icon) return;
    static bool dim = false;
    dim = blinking && !dim;
    lv_obj_set_style_opa(icon, dim ? LV_OPA_20 : LV_OPA_COVER, LV_PART_MAIN);
}

static void show() {
    if (!bubble) return;
    int minutes = 0;
    int k = demo >= 0 ? demo : (have_data ? pick(&minutes) : -1);
    if (demo == RAIN_SOON) minutes = 40;
    static int last = -2;
    if (k < 0) {
        if (last != k) eventLog("weather message: none (data %d, hour0 %d)", have_data, hours[0].hour);
        last = k;
        lv_obj_add_flag(bubble, LV_OBJ_FLAG_HIDDEN); blinking = false; return;
    }
    const Msg& m = MSGS[k];
    char b[48];
    snprintf(b, sizeof(b), m.text, minutes);
    if (k == RAIN_SOON && minutes >= 60) snprintf(b, sizeof(b), "Rain in ~%d h. Umbrella!", (minutes + 30) / 60);
    lv_label_set_text(text, b);
    if (last != k) eventLog("weather message: %s", b);
    last = k;
    int ic = (k == NICE && cur_code > 2) ? cur_code : m.icon;   // a calm but cloudy day shows its clouds
    if (k == NICE && cur_code > 2) lv_label_set_text(text, "Calm weather today");
    lv_label_set_text(icon, getIconGlyph(ic));
    lv_obj_set_style_text_color(icon, lv_color_hex(getIconColor(ic)), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bubble, lv_color_hex(m.bubble), LV_PART_MAIN);
    blinking = m.blink;
    if (!blinking) lv_obj_set_style_opa(icon, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(bubble, LV_OBJ_FLAG_HIDDEN);
}

static void recheck_cb(lv_timer_t* t) { (void)t; show(); }

void createWeatherAlert(lv_obj_t* tab, bool isLargeScreen) {
    bubble = lv_obj_create(tab);
    lv_obj_set_size(bubble, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bubble, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bubble, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_radius(bubble, isLargeScreen ? 18 : 14, LV_PART_MAIN);
    lv_obj_set_style_border_width(bubble, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(bubble, isLargeScreen ? 14 : 10, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(bubble, isLargeScreen ? 6 : 3, LV_PART_MAIN);
    lv_obj_set_style_pad_column(bubble, 8, LV_PART_MAIN);
    lv_obj_clear_flag(bubble, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(bubble, LV_ALIGN_BOTTOM_MID, 0, isLargeScreen ? -50 : -38);   // above the alarm bell
    icon = lv_label_create(bubble);
    lv_obj_set_style_text_font(icon, &weather_icons_24, LV_PART_MAIN);
    text = lv_label_create(bubble);
    lv_obj_set_style_text_font(text, isLargeScreen ? &lv_font_montserrat_20 : &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(text, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_add_flag(bubble, LV_OBJ_FLAG_HIDDEN);
    lv_timer_create(recheck_cb, 15000, nullptr);       // soon after the clock first learns the time, then the minutes left
    lv_timer_create(blink_cb, 600, nullptr);
}

void weatherAlertUpdate(const WeatherData& data) {
    for (int i = 0; i < 24; i++) hours[i] = data.hourly[i];
    cur_code = data.weatherCode;
    have_data = true;
    show();
}

void weatherAlertDemo(int kind) { demo = (kind >= 0 && kind < KINDS) ? kind : -1; show(); }
