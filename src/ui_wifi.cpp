#include "ui_wifi.h"
#include "settings_manager.h"
#include "config/config.h"
#include "wifi_manager.h"
#include <WiFi.h>
#include <vector>

extern SettingsManager settings;

static lv_obj_t* overlay = nullptr;      // everything lives in here, rebuilt per page
static lv_timer_t* scan_timer = nullptr;
static lv_obj_t* list = nullptr;
static lv_obj_t* status_lbl = nullptr;
static lv_obj_t* ta = nullptr;
static lv_obj_t* kb = nullptr;
static lv_obj_t* settings_btn_lbl = nullptr;
static std::vector<String> found;        // SSIDs of the last scan, same order as the list buttons
static std::vector<bool> found_open;
static String chosen;
static bool large = false;
static int kb_page = 0;                  // 0 lower, 1 upper, 2 numbers, 3 symbols

static const lv_font_t* font() { return large ? &lv_font_montserrat_20 : &lv_font_montserrat_14; }

static void pageList();
static void pagePassword(const String& ssid);
static void pageZip();

static void closeAll() {
    wifi_scan_hold = false;
    if (scan_timer) { lv_timer_del(scan_timer); scan_timer = nullptr; }
    if (overlay) { lv_obj_del(overlay); overlay = nullptr; }
    list = status_lbl = ta = kb = nullptr;
}

static void clearPage() {
    if (scan_timer) { lv_timer_del(scan_timer); scan_timer = nullptr; }
    lv_obj_clean(overlay);
    list = status_lbl = ta = kb = nullptr;
}

static lv_obj_t* button(lv_obj_t* parent, const char* text, lv_event_cb_t cb, uint32_t color, int w, int h) {
    lv_obj_t* b = lv_btn_create(parent);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_radius(b, 8, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(b, 0, LV_PART_MAIN);
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

static void saved_and_restart(const char* msg) {
    clearPage();
    lv_obj_t* l = lv_label_create(overlay);
    lv_label_set_text(l, msg);
    lv_obj_set_style_text_font(l, font(), LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(0xa6e3a1), LV_PART_MAIN);
    lv_obj_center(l);
    lv_refr_now(NULL);
    delay(700);
    ESP.restart();
}

static void network_cb(lv_event_t* e) {
    size_t idx = (size_t)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
    if (idx >= found.size()) return;
    chosen = found[idx];
    if (found_open[idx]) {
        settings.setWifiSSID(chosen);
        settings.setWifiPassword("");
        saved_and_restart("Saved. Restarting...");
    } else {
        pagePassword(chosen);
    }
}

static void scan_tick(lv_timer_t* t) {
    (void)t;
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) return;
    Serial.printf("[WiFiSetup] scan finished: %d\n", n);
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
        lv_obj_t* b = lv_list_add_btn(list, LV_SYMBOL_WIFI, buf);
        lv_obj_set_style_bg_color(b, lv_color_hex(0x313244), LV_PART_MAIN);
        lv_obj_set_style_text_color(b, lv_color_hex(0xcdd6f4), LV_PART_MAIN);
        lv_obj_set_style_text_font(b, font(), LV_PART_MAIN);
        lv_obj_set_style_pad_ver(b, large ? 12 : 10, LV_PART_MAIN);   // tall rows: easy to hit
        lv_obj_set_user_data(b, (void*)(intptr_t)(found.size() - 1));
        lv_obj_add_event_cb(b, network_cb, LV_EVENT_CLICKED, nullptr);
    }
    snprintf(buf, sizeof(buf), "%d networks. Tap yours.", (int)found.size());
    lv_label_set_text(status_lbl, buf);
    WiFi.scanDelete();
}

static void startScan() {
    if (status_lbl) lv_label_set_text(status_lbl, "Scanning...");
    if (list) lv_obj_clean(list);
    lv_refr_now(NULL);                       // show "Scanning..." before the radio blocks us for a few seconds
    wifi_scan_hold = true;                   // no connection attempts while we scan
    if (WiFi.getMode() == WIFI_OFF) WiFi.mode(WIFI_STA);
    int n = 0;
    for (int attempt = 1; attempt <= 3; attempt++) {
        WiFi.scanDelete();
        n = WiFi.scanNetworks(false, true, false, 400);   // blocking, hidden networks included, 400 ms per channel
        Serial.printf("[WiFiSetup] scan attempt %d: %d networks (mode %d)\n", attempt, n, (int)WiFi.getMode());
        if (n > 0) break;
        delay(400);
    }
    if (!scan_timer) scan_timer = lv_timer_create(scan_tick, 50, nullptr);   // scan_tick reads the result below
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
    int bh = large ? 48 : 40;
    list = lv_list_create(overlay);
    lv_obj_set_size(list, w - 16, h - bh - (large ? 66 : 54));
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, large ? 56 : 42);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x181825), LV_PART_MAIN);
    lv_obj_set_style_border_width(list, 0, LV_PART_MAIN);

    int bw = (w - 16 - 12) / 3;
    lv_obj_t* r = button(overlay, "Rescan", [](lv_event_t*) { startScan(); }, 0x89b4fa, bw, bh);
    lv_obj_align(r, LV_ALIGN_BOTTOM_LEFT, 8, -4);
    lv_obj_t* z = button(overlay, "Zip code", [](lv_event_t*) { pageZip(); }, 0xf9e2af, bw, bh);
    lv_obj_align(z, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_t* c = button(overlay, "Close", [](lv_event_t*) { closeAll(); }, 0x6c7086, bw, bh);
    lv_obj_align(c, LV_ALIGN_BOTTOM_RIGHT, -8, -4);
    startScan();
}

// ---- the big keyboard: keys only, every control on its own row underneath

static const char* map_lower[] = {"q","w","e","r","t","y","u","i","o","p","\n","a","s","d","f","g","h","j","k","l","\n","z","x","c","v","b","n","m",""};
static const char* map_upper[] = {"Q","W","E","R","T","Y","U","I","O","P","\n","A","S","D","F","G","H","J","K","L","\n","Z","X","C","V","B","N","M",""};
static const char* map_num[]   = {"1","2","3","4","5","6","7","8","9","0","\n","-","/",":",";","(",")","$","&","@","\n",".",",","?","!","'","_","+",""};
static const char* map_sym[]   = {"~","`","|","^","*","\\","{","}","[","]","\n","<",">","=","%","#","\"","@","&","$","\n",".",",","?","!","'","_","+",""};
static const char* map_digits[] = {"1","2","3","4","5","\n","6","7","8","9","0",""};
static lv_btnmatrix_ctrl_t ctrl_keys[40];

static void kb_show_page(int page) {
    kb_page = page;
    lv_keyboard_set_mode(kb, (lv_keyboard_mode_t)(LV_KEYBOARD_MODE_USER_1 + page));
}

static void style_keys(lv_obj_t* k) {
    lv_obj_set_style_bg_color(k, lv_color_hex(0x11111b), LV_PART_MAIN);
    lv_obj_set_style_pad_all(k, 3, LV_PART_MAIN);
    lv_obj_set_style_pad_gap(k, 4, LV_PART_MAIN);
    lv_obj_set_style_border_width(k, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(k, lv_color_hex(0x45475a), LV_PART_ITEMS);
    lv_obj_set_style_text_color(k, lv_color_hex(0xcdd6f4), LV_PART_ITEMS);
    lv_obj_set_style_text_font(k, large ? &lv_font_montserrat_28 : &lv_font_montserrat_20, LV_PART_ITEMS);
    lv_obj_set_style_radius(k, 8, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(k, lv_color_hex(0xfab387), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(k, lv_color_hex(0x11111b), LV_PART_ITEMS | LV_STATE_PRESSED);
}

static void make_keyboard(int x, int y, int w, int h, bool digitsOnly) {
    for (int i = 0; i < 40; i++) ctrl_keys[i] = (lv_btnmatrix_ctrl_t)1;   // every key one unit wide
    kb = lv_keyboard_create(overlay);
    lv_obj_set_size(kb, w, h);
    lv_obj_align(kb, LV_ALIGN_TOP_LEFT, x, y);   // lv_keyboard_create aligns it to the bottom; set_pos would then offset from there
    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_USER_1, digitsOnly ? map_digits : map_lower, ctrl_keys);
    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_USER_2, map_upper, ctrl_keys);
    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_USER_3, map_num, ctrl_keys);
    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_USER_4, map_sym, ctrl_keys);
    style_keys(kb);
    lv_keyboard_set_textarea(kb, ta);
    kb_show_page(0);
}

// ---- page 2: the password

static void connect_now() {
    settings.setWifiSSID(chosen);
    settings.setWifiPassword(String(ta ? lv_textarea_get_text(ta) : ""));
    saved_and_restart("Saved. Restarting to connect...");
}

static void pagePassword(const String& ssid) {
    clearPage();
    int w = lv_obj_get_width(overlay), h = lv_obj_get_height(overlay);
    int th = large ? 48 : 36, ch = large ? 52 : 42, gap = 4;
    ta = lv_textarea_create(overlay);
    lv_obj_set_size(ta, w - 8, th);
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 3);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_password_mode(ta, true);
    lv_textarea_set_max_length(ta, 63);
    char ph[80]; snprintf(ph, sizeof(ph), "Password for %s", ssid.c_str());
    lv_textarea_set_placeholder_text(ta, ph);
    lv_obj_set_style_text_font(ta, font(), LV_PART_MAIN);

    int ky = th + 3 + gap, kh = h - ky - ch - 2 * gap;
    make_keyboard(0, ky, w, kh, false);

    // The control row: Show, Aa, 123, Space (double), Del, Cancel, OK
    int cy = h - ch - gap, bw = (w - 8 - 6 * 3) / 7;
    int x = 4;
    auto add = [&](const char* txt, lv_event_cb_t cb, uint32_t col, int units) {
        int bwidth = bw * units + 3 * (units - 1);
        lv_obj_t* b = button(overlay, txt, cb, col, bwidth, ch);
        lv_obj_set_pos(b, x, cy);
        x += bwidth + 3;
        return b;
    };
    add(LV_SYMBOL_EYE_OPEN, [](lv_event_t* e) {
        bool hide = !lv_textarea_get_password_mode(ta);
        lv_textarea_set_password_mode(ta, hide);
        lv_label_set_text(lv_obj_get_child(lv_event_get_target(e), 0), hide ? LV_SYMBOL_EYE_OPEN : LV_SYMBOL_EYE_CLOSE);
    }, 0x94e2d5, 1);
    add("Aa", [](lv_event_t*) { kb_show_page(kb_page == 0 ? 1 : 0); }, 0x89b4fa, 1);
    add("123", [](lv_event_t* e) {
        int next = kb_page == 2 ? 3 : (kb_page == 3 ? 0 : 2);
        kb_show_page(next);
        lv_label_set_text(lv_obj_get_child(lv_event_get_target(e), 0), next == 0 ? "123" : (next == 2 ? "#+=" : "abc"));
    }, 0xf9e2af, 1);
    add("Space", [](lv_event_t*) { lv_textarea_add_char(ta, ' '); }, 0xcdd6f4, 2);
    add(LV_SYMBOL_BACKSPACE, [](lv_event_t*) { lv_textarea_del_char(ta); }, 0xfab387, 1);
    int rest = w - 4 - x;
    lv_obj_t* cancel = button(overlay, LV_SYMBOL_CLOSE, [](lv_event_t*) { pageList(); }, 0xf38ba8, rest / 2 - 2, ch);
    lv_obj_set_pos(cancel, x, cy);
    lv_obj_t* ok = button(overlay, LV_SYMBOL_OK, [](lv_event_t*) { connect_now(); }, 0xa6e3a1, rest - rest / 2 - 2, ch);
    lv_obj_set_pos(ok, x + rest / 2, cy);
}

// ---- page 3: the zip code for the weather

static void pageZip() {
    clearPage();
    int w = lv_obj_get_width(overlay), h = lv_obj_get_height(overlay);
    int th = large ? 48 : 36, ch = large ? 52 : 42, gap = 4;
    ta = lv_textarea_create(overlay);
    lv_obj_set_size(ta, w - 8, th);
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 3);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_max_length(ta, 10);
    lv_textarea_set_placeholder_text(ta, "Zip code for the weather");
    lv_textarea_set_text(ta, settings.getZipCode().c_str());
    lv_obj_set_style_text_font(ta, font(), LV_PART_MAIN);

    int ky = th + 3 + gap, kh = h - ky - ch - 2 * gap;
    make_keyboard(0, ky, w, kh, true);

    int cy = h - ch - gap, bw = (w - 8 - 2 * 3) / 3;
    lv_obj_t* del = button(overlay, LV_SYMBOL_BACKSPACE, [](lv_event_t*) { lv_textarea_del_char(ta); }, 0xfab387, bw, ch);
    lv_obj_set_pos(del, 4, cy);
    lv_obj_t* cancel = button(overlay, LV_SYMBOL_CLOSE, [](lv_event_t*) { pageList(); }, 0xf38ba8, bw, ch);
    lv_obj_set_pos(cancel, 4 + bw + 3, cy);
    lv_obj_t* ok = button(overlay, LV_SYMBOL_OK, [](lv_event_t*) {
        String zip = String(lv_textarea_get_text(ta));
        if (zip.length() >= 3) { settings.setZipCode(zip); settings.setCityCode(""); }
        pageList();
    }, 0xa6e3a1, bw, ch);
    lv_obj_set_pos(ok, 4 + 2 * (bw + 3), cy);
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
