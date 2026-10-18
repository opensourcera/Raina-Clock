#include "ui_weather.h"
#include "config/config.h"
#include "settings_manager.h"
#include "time_utils.h"
#include "ui.h"

extern SettingsManager settings;

LV_FONT_DECLARE(weather_icons_24);
LV_FONT_DECLARE(weather_icons_48);
LV_FONT_DECLARE(weather_icons_72);

static const int HOUR_CARDS = 6;          // every second hour: the next 12 hours
static const uint32_t CARD = 0x1e1e2e, TEXT = 0xcdd6f4, DIM = 0x9399b2, RAIN = 0x89b4fa, ACCENT = 0xcba6f7;

struct Card { lv_obj_t* box; lv_obj_t* top; lv_obj_t* icon; lv_obj_t* temp; lv_obj_t* low; lv_obj_t* rain; };
static Card hour_cards[HOUR_CARDS];
static Card day_cards[FORECAST_DAYS];
static bool have_page = false;

// the panel beside the clock
static lv_obj_t* now_icon = nullptr;
static lv_obj_t* now_temp = nullptr;

static lv_obj_t* label(lv_obj_t* parent, const lv_font_t* font, uint32_t color, const char* text) {
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(color), LV_PART_MAIN);
    lv_label_set_text(l, text);
    return l;
}

static Card makeCard(lv_obj_t* parent, int x, int y, int w, int h, bool withLow, bool big) {
    Card c;
    c.box = lv_obj_create(parent);
    lv_obj_set_pos(c.box, x, y);
    lv_obj_set_size(c.box, w, h);
    lv_obj_set_style_bg_color(c.box, lv_color_hex(CARD), LV_PART_MAIN);
    lv_obj_set_style_border_width(c.box, 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(c.box, lv_color_hex(ACCENT), LV_PART_MAIN);
    lv_obj_set_style_radius(c.box, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(c.box, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_row(c.box, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(c.box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(c.box, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(c.box, LV_OBJ_FLAG_SCROLLABLE);
    const lv_font_t* f = big ? &lv_font_montserrat_20 : &lv_font_montserrat_14;
    const lv_font_t* fs = big ? &lv_font_montserrat_14 : &lv_font_montserrat_10;
    c.top = label(c.box, f, DIM, "--");
    c.icon = label(c.box, big ? &weather_icons_48 : &weather_icons_24, TEXT, "");
    c.temp = label(c.box, f, TEXT, "--");
    c.low = withLow ? label(c.box, fs, DIM, "") : nullptr;
    c.rain = label(c.box, fs, RAIN, "");
    return c;
}

void createWeatherPage(lv_obj_t* tab, bool isLargeScreen) {
    // the page this replaces stays alive (its labels are still updated) but out of sight
    for (uint32_t i = 0; i < lv_obj_get_child_cnt(tab); i++) lv_obj_add_flag(lv_obj_get_child(tab, i), LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_ROW);   // no layout: the cards are placed by hand
    lv_obj_set_layout(tab, 0);
    lv_obj_set_style_pad_all(tab, 0, LV_PART_MAIN);
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_update_layout(tab);

    int W = lv_obj_get_content_width(tab), H = lv_obj_get_content_height(tab);
    if (W <= 0) W = lv_disp_get_hor_res(nullptr);
    int g = isLargeScreen ? 6 : 4;
    int hourH = (H - 3 * g) * 45 / 100, dayH = H - 3 * g - hourH;

    int hw = (W - (HOUR_CARDS + 1) * g) / HOUR_CARDS;
    for (int i = 0; i < HOUR_CARDS; i++) hour_cards[i] = makeCard(tab, g + i * (hw + g), g, hw, hourH, false, isLargeScreen);
    int dw = (W - (FORECAST_DAYS + 1) * g) / FORECAST_DAYS;
    for (int i = 0; i < FORECAST_DAYS; i++) day_cards[i] = makeCard(tab, g + i * (dw + g), 2 * g + hourH, dw, dayH, true, isLargeScreen);
    have_page = true;
}

// Beside the clock only the sky and the temperature: the icon says clear, cloudy or rainy, the number the rest
void createClockWeatherPanel(lv_obj_t* tab, int x, int w, bool isLargeScreen) {
    lv_obj_t* p = lv_obj_create(tab);
    lv_obj_set_size(p, w, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(p, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(p, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(p, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(p, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(p, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(p, isLargeScreen ? 10 : 6, LV_PART_MAIN);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(p, LV_ALIGN_LEFT_MID, x, -16);
    now_icon = label(p, &weather_icons_72, TEXT, "");
    now_temp = label(p, isLargeScreen ? &lv_font_montserrat_48 : &lv_font_montserrat_28, 0xfab387, "--");
}

static void deg(char* b, size_t n, float v) { snprintf(b, n, "%.0f\xC2\xB0", v); }

static void fillCard(Card& c, const char* top, int code, float temp, const float* low, int rain) {
    char b[16];
    lv_label_set_text(c.top, top);
    lv_label_set_text(c.icon, getIconGlyph(code));
    lv_obj_set_style_text_color(c.icon, lv_color_hex(getIconColor(code)), LV_PART_MAIN);
    deg(b, sizeof(b), temp); lv_label_set_text(c.temp, b);
    if (c.low) { if (low) { deg(b, sizeof(b), *low); lv_label_set_text(c.low, b); } else lv_label_set_text(c.low, ""); }
    snprintf(b, sizeof(b), "%d%%", rain);
    lv_label_set_text(c.rain, b);
    lv_obj_set_style_text_color(c.rain, lv_color_hex(rain >= 30 ? RAIN : 0x585b70), LV_PART_MAIN);   // dim when rain is unlikely
}

void weatherPanelsUpdate(const WeatherData& data) {
    bool use24 = settings.getUse24HourFormat();
    char b[48];

    if (now_temp) {
        lv_label_set_text(now_icon, getIconGlyph(data.weatherCode));
        lv_obj_set_style_text_color(now_icon, lv_color_hex(getIconColor(data.weatherCode)), LV_PART_MAIN);
        deg(b, sizeof(b), data.temperature); lv_label_set_text(now_temp, b);
    }
    setHeaderCity(data.cityName.c_str());
    if (!have_page) return;

    for (int i = 0; i < HOUR_CARDS; i++) {
        const HourlyForecast& h = data.hourly[i * 2];
        char t[12];
        if (h.hour < 0) snprintf(t, sizeof(t), "+%dh", i * 2);
        else if (use24) snprintf(t, sizeof(t), "%02d:00", h.hour);
        else snprintf(t, sizeof(t), "%d%s", h.hour % 12 == 0 ? 12 : h.hour % 12, h.hour < 12 ? "am" : "pm");
        if (i == 0) snprintf(t, sizeof(t), "Now");
        fillCard(hour_cards[i], t, h.weatherCode, h.temperature, nullptr, h.precipitationProbability);
    }

    static const char* wd[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    struct tm now;
    bool have_time = getLocalTimeWrapper(&now, 0);
    for (int i = 0; i < FORECAST_DAYS; i++) {
        const ForecastDay& d = data.forecast[i];
        Card& c = day_cards[i];
        if (d.weatherCode < 0) { lv_obj_add_flag(c.box, LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_clear_flag(c.box, LV_OBJ_FLAG_HIDDEN);
        const char* name = have_time ? wd[(now.tm_wday + i) % 7] : d.dayName.c_str();
        fillCard(c, i == 0 ? "Today" : name, d.weatherCode, d.tempMax, &d.tempMin, d.rainProb);
        lv_obj_set_style_border_width(c.box, i == 0 ? 2 : 0, LV_PART_MAIN);   // today is outlined
    }
}
