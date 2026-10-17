#include "ui_wifi.h"
#include "settings_manager.h"
#include "config/config.h"
#include <WiFi.h>
#include <vector>

extern SettingsManager settings;

static lv_obj_t* overlay = nullptr;      // everything lives in here, rebuilt per page
static lv_timer_t* scan_timer = nullptr;
static lv_obj_t* list = nullptr;
static lv_obj_t* status_lbl = nullptr;
static lv_obj_t* ta = nullptr;
static lv_obj_t* settings_btn_lbl = nullptr;
static std::vector<String> found;        // SSIDs of the last scan, same order as the list buttons
static std::vector<bool> found_open;
static String chosen;
static bool large = false;

static const lv_font_t* font() { return large ? &lv_font_montserrat_20 : &lv_font_montserrat_14; }

static void pageList();
static void pagePassword(const String& ssid);
static void pageZip();

static void closeAll() {
    if (scan_timer) { lv_timer_del(scan_timer); scan_timer = nullptr; }
    if (overlay) { lv_obj_del(overlay); overlay = nullptr; }
    list = status_lbl = ta = nullptr;
}

static void clearPage() {
    if (scan_timer) { lv_timer_del(scan_timer); scan_timer = nullptr; }
    lv_obj_clean(overlay);
    list = status_lbl = ta = nullptr;
}

static lv_obj_t* button(lv_obj_t* parent, const char* text, lv_event_cb_t cb, uint32_t color, int w, int h) {
    lv_obj_t* b = lv_btn_create(parent);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_radius(b, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, font(), LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(0x11111b), LV_PART_MAIN);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
    return b;
}

static lv_obj_t* title(const char* text) {
    lv_obj_t* t = lv_label_create(overlay);
    lv_label_set_text(t, text);
    lv_obj_set_style_text_font(t, font(), LV_PART_MAIN);
    lv_obj_set_style_text_color(t, lv_color_hex(0xfab387), LV_PART_MAIN);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 8, 6);
    return t;
}

// ---- page 1: the list of networks

static void scan_tick(lv_timer_t* t) {
    (void)t;
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) return;
    if (scan_timer) { lv_timer_del(scan_timer); scan_timer = nullptr; }
    if (!list) return;
    lv_obj_clean(list);
    found.clear(); found_open.clear();
    if (n <= 0) {
        lv_label_set_text(status_lbl, "No networks found. Tap Rescan.");
        WiFi.scanDelete();
        return;
    }
    char buf[80];
    for (int i = 0; i < n && found.size() < 14; i++) {
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) continue;
        bool dup = false;
        for (auto& s : found) if (s == ssid) dup = true;
        if (dup) continue;
        bool open = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
        found.push_back(ssid); found_open.push_back(open);
        snprintf(buf, sizeof(buf), "%s%s", ssid.c_str(), open ? "  (open)" : "");
        lv_obj_t* b = lv_list_add_btn(list, open ? LV_SYMBOL_WIFI : LV_SYMBOL_WIFI, buf);
        lv_obj_set_style_bg_color(b, lv_color_hex(0x313244), LV_PART_MAIN);
        lv_obj_set_style_text_color(b, lv_color_hex(0xcdd6f4), LV_PART_MAIN);
        lv_obj_set_style_text_font(b, font(), LV_PART_MAIN);
        lv_obj_set_user_data(b, (void*)(intptr_t)(found.size() - 1));
        lv_obj_add_event_cb(b, [](lv_event_t* e) {
            size_t idx = (size_t)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
            if (idx >= found.size()) return;
            chosen = found[idx];
            if (found_open[idx]) {
                settings.setWifiSSID(chosen); settings.setWifiPassword("");
                clearPage(); lv_obj_t* l = lv_label_create(overlay);
                lv_label_set_text(l, "Saved. Restarting..."); lv_obj_set_style_text_font(l, font(), LV_PART_MAIN);
                lv_obj_set_style_text_color(l, lv_color_hex(0xa6e3a1), LV_PART_MAIN); lv_obj_center(l);
                lv_refr_now(NULL); delay(700); ESP.restart();
            } else pagePassword(chosen);
        }, LV_EVENT_CLICKED, nullptr);
    }
    snprintf(buf, sizeof(buf), "%d networks. Tap yours.", (int)found.size());
    lv_label_set_text(status_lbl, buf);
    WiFi.scanDelete();
}

static void startScan() {
    if (status_lbl) lv_label_set_text(status_lbl, "Scanning...");
    if (list) lv_obj_clean(list);
    WiFi.scanDelete();
    WiFi.scanNetworks(true, false, false, 300);
    if (!scan_timer) scan_timer = lv_timer_create(scan_tick, 500, nullptr);
}

static void pageList() {
    clearPage();
    title("Wi-Fi: choose your network");
    status_lbl = lv_label_create(overlay);
    lv_label_set_text(status_lbl, "");
    lv_obj_set_style_text_font(status_lbl, large ? &lv_font_montserrat_14 : &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(status_lbl, lv_color_hex(0xa6adc8), LV_PART_MAIN);
    lv_obj_align(status_lbl, LV_ALIGN_TOP_LEFT, 8, large ? 34 : 26);

    int w = lv_obj_get_width(overlay), h = lv_obj_get_height(overlay);
    list = lv_list_create(overlay);
    lv_obj_set_size(list, w - 16, h - (large ? 130 : 108));
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, large ? 56 : 42);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x181825), LV_PART_MAIN);
    lv_obj_set_style_border_width(list, 0, LV_PART_MAIN);

    int bh = large ? 44 : 34, bw = (w - 16 - 12) / 3;
    lv_obj_t* r = button(overlay, "Rescan", [](lv_event_t*) { startScan(); }, 0x89b4fa, bw, bh);
    lv_obj_align(r, LV_ALIGN_BOTTOM_LEFT, 8, -6);
    lv_obj_t* z = button(overlay, "Zip code", [](lv_event_t*) { pageZip(); }, 0xf9e2af, bw, bh);
    lv_obj_align(z, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_t* c = button(overlay, "Close", [](lv_event_t*) { closeAll(); }, 0x6c7086, bw, bh);
    lv_obj_align(c, LV_ALIGN_BOTTOM_RIGHT, -8, -6);
    startScan();
}

// ---- page 2: the password on the touch keyboard

static void connect_now() {
    const char* pw = ta ? lv_textarea_get_text(ta) : "";
    settings.setWifiSSID(chosen);
    settings.setWifiPassword(String(pw));
    clearPage();
    lv_obj_t* l = lv_label_create(overlay);
    lv_label_set_text(l, "Saved. Restarting to connect...");
    lv_obj_set_style_text_font(l, font(), LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(0xa6e3a1), LV_PART_MAIN);
    lv_obj_center(l);
    lv_refr_now(NULL);
    delay(700);
    ESP.restart();
}

static void kb_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) connect_now();
    else if (code == LV_EVENT_CANCEL) pageList();
}

static void pagePassword(const String& ssid) {
    clearPage();
    char buf[80];
    snprintf(buf, sizeof(buf), "Password for %s", ssid.c_str());
    lv_obj_t* t = title(buf);
    lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);
    lv_obj_set_width(t, lv_obj_get_width(overlay) - 16);

    int w = lv_obj_get_width(overlay);
    ta = lv_textarea_create(overlay);
    lv_obj_set_size(ta, w - 16 - (large ? 100 : 74), large ? 40 : 32);
    lv_obj_align(ta, LV_ALIGN_TOP_LEFT, 8, large ? 34 : 26);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_password_mode(ta, true);
    lv_textarea_set_max_length(ta, 63);
    lv_textarea_set_placeholder_text(ta, "Wi-Fi password");
    lv_obj_set_style_text_font(ta, font(), LV_PART_MAIN);

    lv_obj_t* show = button(overlay, "Show", [](lv_event_t* e) {
        lv_obj_t* l = lv_obj_get_child(lv_event_get_target(e), 0);
        bool hide = !lv_textarea_get_password_mode(ta);
        lv_textarea_set_password_mode(ta, hide);
        lv_label_set_text(l, hide ? "Show" : "Hide");
    }, 0x94e2d5, large ? 90 : 66, large ? 40 : 32);
    lv_obj_align(show, LV_ALIGN_TOP_RIGHT, -8, large ? 34 : 26);

    lv_obj_t* kb = lv_keyboard_create(overlay);
    lv_obj_set_size(kb, w, lv_obj_get_height(overlay) - (large ? 80 : 62));
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_add_event_cb(kb, kb_cb, LV_EVENT_ALL, nullptr);   // the tick connects, the cross goes back
}

// ---- page 3: the zip code for the weather

static void zip_kb_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) {
        String zip = ta ? String(lv_textarea_get_text(ta)) : String("");
        if (zip.length() >= 3) { settings.setZipCode(zip); settings.setCityCode(""); }
        pageList();
    } else if (code == LV_EVENT_CANCEL) pageList();
}

static void pageZip() {
    clearPage();
    title("Zip code for the weather");
    int w = lv_obj_get_width(overlay);
    ta = lv_textarea_create(overlay);
    lv_obj_set_size(ta, w - 16, large ? 40 : 32);
    lv_obj_align(ta, LV_ALIGN_TOP_LEFT, 8, large ? 34 : 26);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_max_length(ta, 10);
    lv_textarea_set_text(ta, settings.getZipCode().c_str());
    lv_obj_set_style_text_font(ta, font(), LV_PART_MAIN);
    lv_obj_t* kb = lv_keyboard_create(overlay);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
    lv_obj_set_size(kb, w, lv_obj_get_height(overlay) - (large ? 80 : 62));
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_add_event_cb(kb, zip_kb_cb, LV_EVENT_ALL, nullptr);
}

// ---- entry points

void showWifiSetup() {
    if (overlay) return;
    large = (lv_disp_get_hor_res(NULL) >= 480 || lv_disp_get_ver_res(NULL) >= 480);
    overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(overlay, lv_color_hex(0x1e1e2e), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(overlay, 0, LV_PART_MAIN);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_update_layout(overlay);
    pageList();
}

bool isWifiSetupShown() { return overlay != nullptr; }

void refreshWifiSettingsButton() {
    if (!settings_btn_lbl) return;
    char buf[80];
    snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI "  %s", settings.getWifiSSID().length() ? settings.getWifiSSID().c_str() : "Set up Wi-Fi");
    lv_label_set_text(settings_btn_lbl, buf);
}

lv_obj_t* createWifiSettingsButton(lv_obj_t* parent, bool isLargeScreen) {
    large = isLargeScreen;
    lv_obj_t* b = lv_btn_create(parent);
    lv_obj_set_size(b, lv_pct(100), isLargeScreen ? 44 : 34);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x89b4fa), LV_PART_MAIN);
    lv_obj_set_style_radius(b, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
    settings_btn_lbl = lv_label_create(b);
    lv_obj_set_style_text_font(settings_btn_lbl, font(), LV_PART_MAIN);
    lv_obj_set_style_text_color(settings_btn_lbl, lv_color_hex(0x11111b), LV_PART_MAIN);
    lv_label_set_long_mode(settings_btn_lbl, LV_LABEL_LONG_DOT);
    lv_obj_set_width(settings_btn_lbl, lv_pct(90));
    lv_obj_center(settings_btn_lbl);
    lv_obj_add_event_cb(b, [](lv_event_t*) { showWifiSetup(); }, LV_EVENT_CLICKED, nullptr);
    refreshWifiSettingsButton();
    return b;
}
