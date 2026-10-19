#include "ui_weather.h"
#include "config/config.h"
#include "settings_manager.h"
#include "time_utils.h"
#include "ui.h"
#include "weather_alert.h"

extern SettingsManager settings;

LV_FONT_DECLARE(weather_icons_16);
LV_FONT_DECLARE(weather_icons_24);
LV_FONT_DECLARE(weather_icons_48);
LV_FONT_DECLARE(weather_icons_72);

static const int POINTS = 8, STEP = 3;   // the hourly graph: a point every third hour, the next 24 hours
static const uint32_t CARD = 0x1e1e2e, TEXT = 0xcdd6f4, DIM = 0x9399b2, RAIN = 0x89b4fa, ACCENT = 0xcba6f7;

struct Card { lv_obj_t* box; lv_obj_t* top; lv_obj_t* icon; lv_obj_t* temp; lv_obj_t* low; lv_obj_t* rain; };

// The hourly graph, drawn in one go like Google's: the temperature as a line with a soft fill under it, the degrees
// above each point, the sky and the hour below
struct Point { float temp; int code; bool day; char time[8]; };
static Point pts[POINTS];
static int npts = 0;
static lv_obj_t* graph = nullptr;
static bool graph_large = false;
static const uint32_t LINE = 0xfab387;
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

static void text(lv_draw_ctx_t* ctx, const lv_font_t* f, uint32_t color, int cx, int y, const char* t) {
    lv_draw_label_dsc_t d;
    lv_draw_label_dsc_init(&d);
    d.font = f;
    d.color = lv_color_hex(color);
    d.align = LV_TEXT_ALIGN_CENTER;
    lv_area_t a = {(lv_coord_t)(cx - 30), (lv_coord_t)y, (lv_coord_t)(cx + 30), (lv_coord_t)(y + lv_font_get_line_height(f))};
    lv_draw_label(ctx, &d, &a, t, nullptr);
}

static void graph_draw_cb(lv_event_t* e) {
    if (npts < 2) return;
    lv_obj_t* obj = lv_event_get_target(e);
    lv_draw_ctx_t* ctx = lv_event_get_draw_ctx(e);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    const lv_font_t* ft = graph_large ? &lv_font_montserrat_20 : &lv_font_montserrat_14;
    const lv_font_t* fh = graph_large ? &lv_font_montserrat_14 : &lv_font_montserrat_10;
    const lv_font_t* fi = graph_large ? &weather_icons_24 : &weather_icons_16;
    int w = lv_area_get_width(&a), h = lv_area_get_height(&a);
    int tH = lv_font_get_line_height(ft), hH = lv_font_get_line_height(fh), iH = lv_font_get_line_height(fi);
    // bands from the top: degrees, the line, the fill down to the icons, the icons, the hours
    int lineTop = a.y1 + tH + 4, timeY = a.y2 - hH - 2, iconY = timeY - iH, fillBot = iconY - 2;
    int lineBot = fillBot - (fillBot - lineTop) / 5;           // most of the height goes to the line, a strip of fill stays under its lowest point
    float lo = pts[0].temp, hi = pts[0].temp;
    for (int i = 1; i < npts; i++) { if (pts[i].temp < lo) lo = pts[i].temp; if (pts[i].temp > hi) hi = pts[i].temp; }
    float span = hi - lo < 4 ? 4 : hi - lo;
    lv_point_t p[POINTS];
    float colW = (float)w / npts;
    for (int i = 0; i < npts; i++) {
        p[i].x = a.x1 + (lv_coord_t)(colW * (i + 0.5f));
        p[i].y = lineBot - (lv_coord_t)((pts[i].temp - lo) / span * (lineBot - lineTop));
    }
    // the fill: one-pixel columns fading into the card, flat past the first and the last point
    lv_draw_rect_dsc_t r;
    lv_draw_rect_dsc_init(&r);
    r.bg_color = lv_color_mix(lv_color_hex(LINE), lv_color_hex(CARD), 110);
    r.bg_grad.dir = LV_GRAD_DIR_VER;
    r.bg_grad.stops_count = 2;
    r.bg_grad.stops[0].color = r.bg_color; r.bg_grad.stops[0].frac = 0;
    r.bg_grad.stops[1].color = lv_color_hex(CARD); r.bg_grad.stops[1].frac = 255;
    int seg = 0;
    for (int x = a.x1 + 1; x < a.x2; x++) {
        while (seg < npts - 2 && x > p[seg + 1].x) seg++;
        int y;
        if (x <= p[0].x) y = p[0].y;
        else if (x >= p[npts - 1].x) y = p[npts - 1].y;
        else y = p[seg].y + (p[seg + 1].y - p[seg].y) * (x - p[seg].x) / (p[seg + 1].x - p[seg].x);
        lv_area_t col = {(lv_coord_t)x, (lv_coord_t)y, (lv_coord_t)x, (lv_coord_t)fillBot};
        lv_draw_rect(ctx, &r, &col);
    }
    // the line and its dots
    lv_draw_line_dsc_t l;
    lv_draw_line_dsc_init(&l);
    l.color = lv_color_hex(LINE);
    l.width = graph_large ? 3 : 2;
    l.round_start = l.round_end = 1;
    for (int i = 0; i + 1 < npts; i++) lv_draw_line(ctx, &l, &p[i], &p[i + 1]);
    lv_draw_rect_dsc_t dot;
    lv_draw_rect_dsc_init(&dot);
    dot.bg_color = lv_color_hex(LINE);
    dot.radius = LV_RADIUS_CIRCLE;
    int dr = graph_large ? 4 : 3;
    char b[12];
    for (int i = 0; i < npts; i++) {
        lv_area_t d = {(lv_coord_t)(p[i].x - dr), (lv_coord_t)(p[i].y - dr), (lv_coord_t)(p[i].x + dr), (lv_coord_t)(p[i].y + dr)};
        lv_draw_rect(ctx, &dot, &d);
        snprintf(b, sizeof(b), "%.0f\xC2\xB0", pts[i].temp);
        text(ctx, ft, i == 0 ? 0xffffff : TEXT, p[i].x, p[i].y - tH - 2, b);
        text(ctx, fi, getIconColor(pts[i].code, pts[i].day), p[i].x, iconY, getIconGlyph(pts[i].code, pts[i].day));
        text(ctx, fh, i == 0 ? TEXT : DIM, p[i].x, timeY, pts[i].time);
    }
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

    graph_large = isLargeScreen;
    graph = lv_obj_create(tab);
    lv_obj_set_pos(graph, g, g);
    lv_obj_set_size(graph, W - 2 * g, hourH);
    lv_obj_set_style_bg_color(graph, lv_color_hex(CARD), LV_PART_MAIN);
    lv_obj_set_style_border_width(graph, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(graph, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_all(graph, 0, LV_PART_MAIN);
    lv_obj_clear_flag(graph, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);   // a tap falls through to the page menu
    lv_obj_add_event_cb(graph, graph_draw_cb, LV_EVENT_DRAW_MAIN_END, nullptr);
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
    lv_obj_set_style_pad_row(p, 0, LV_PART_MAIN);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(p, LV_ALIGN_LEFT_MID, x, isLargeScreen ? -56 : -48);
    now_icon = label(p, &weather_icons_72, TEXT, "");
    now_temp = label(p, isLargeScreen ? &lv_font_montserrat_48 : &lv_font_montserrat_28, 0xfab387, "--");
}

static void deg(char* b, size_t n, float v) { snprintf(b, n, "%.0f\xC2\xB0", v); }

static void fillCard(Card& c, const char* top, int code, float temp, const float* low, int rain, bool isDay = true) {
    char b[16];
    lv_label_set_text(c.top, top);
    lv_label_set_text(c.icon, getIconGlyph(code, isDay));
    lv_obj_set_style_text_color(c.icon, lv_color_hex(getIconColor(code, isDay)), LV_PART_MAIN);
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
        lv_label_set_text(now_icon, getIconGlyph(data.weatherCode, data.isDay));
        lv_obj_set_style_text_color(now_icon, lv_color_hex(getIconColor(data.weatherCode, data.isDay)), LV_PART_MAIN);
        deg(b, sizeof(b), data.temperature); lv_label_set_text(now_temp, b);
    }
    setHeaderCity(data.cityName.c_str());
    weatherAlertUpdate(data);
    if (!have_page) return;

    npts = 0;
    for (int i = 0; i < POINTS; i++) {
        const HourlyForecast& h = data.hourly[i * STEP];
        if (h.weatherCode < 0) break;
        Point& q = pts[npts++];
        q.temp = h.temperature; q.code = h.weatherCode; q.day = h.isDay;
        if (i == 0) snprintf(q.time, sizeof(q.time), "Now");
        else if (h.hour < 0) snprintf(q.time, sizeof(q.time), "+%dh", i * STEP);
        else if (use24) snprintf(q.time, sizeof(q.time), "%02d:00", h.hour);
        else snprintf(q.time, sizeof(q.time), "%d%s", h.hour % 12 == 0 ? 12 : h.hour % 12, h.hour < 12 ? "am" : "pm");
    }
    if (graph) lv_obj_invalidate(graph);

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
