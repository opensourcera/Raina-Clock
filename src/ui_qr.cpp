#include "ui_qr.h"
#include <WiFi.h>
#include "src/extra/libs/qrcode/qrcodegen.h"

static const int QR_MAX_VERSION = 6;                 // up to about 130 characters, more than any address here
static const size_t QR_BUF = qrcodegen_BUFFER_LEN_FOR_VERSION(QR_MAX_VERSION);

// The encoded code lives in the widget's user data: QR_BUF bytes, the first byte 0 when there is nothing to show
static void qr_draw_cb(lv_event_t* e) {
    lv_obj_t* obj = lv_event_get_target(e);
    uint8_t* code = (uint8_t*)lv_obj_get_user_data(obj);
    if (code == nullptr || code[0] == 0) return;
    lv_draw_ctx_t* ctx = lv_event_get_draw_ctx(e);
    lv_area_t a; lv_obj_get_coords(obj, &a);
    int n = qrcodegen_getSize(code);
    int px = lv_area_get_width(&a) / (n + 4);       // a two-module quiet zone on each side
    if (px < 1) px = 1;
    int x0 = a.x1 + (lv_area_get_width(&a) - px * n) / 2, y0 = a.y1 + (lv_area_get_height(&a) - px * n) / 2;
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
    d.bg_color = lv_color_black(); d.bg_opa = LV_OPA_COVER;
    for (int y = 0; y < n; y++) {
        for (int x = 0; x < n; x++) {
            if (!qrcodegen_getModule(code, x, y)) continue;
            int run = 1;                             // dark modules side by side: one rectangle
            while (x + run < n && qrcodegen_getModule(code, x + run, y)) run++;
            lv_area_t r = {(lv_coord_t)(x0 + x * px), (lv_coord_t)(y0 + y * px), (lv_coord_t)(x0 + (x + run) * px - 1), (lv_coord_t)(y0 + (y + 1) * px - 1)};
            lv_draw_rect(ctx, &d, &r);
            x += run - 1;
        }
    }
}

static void qr_delete_cb(lv_event_t* e) {
    void* code = lv_obj_get_user_data(lv_event_get_target(e));
    if (code) lv_mem_free(code);
}

lv_obj_t* qrCreate(lv_obj_t* parent, int size) {
    lv_obj_t* q = lv_obj_create(parent);
    lv_obj_set_size(q, size, size);
    lv_obj_set_style_bg_color(q, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(q, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(q, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(q, 6, LV_PART_MAIN);
    lv_obj_clear_flag(q, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    uint8_t* code = (uint8_t*)lv_mem_alloc(QR_BUF);
    if (code) code[0] = 0;
    lv_obj_set_user_data(q, code);
    lv_obj_add_event_cb(q, qr_draw_cb, LV_EVENT_DRAW_MAIN_END, nullptr);
    lv_obj_add_event_cb(q, qr_delete_cb, LV_EVENT_DELETE, nullptr);
    return q;
}

void qrSetText(lv_obj_t* qr, const char* text) {
    uint8_t* code = (uint8_t*)lv_obj_get_user_data(qr);
    if (code == nullptr) return;
    code[0] = 0;
    if (text && text[0]) {
        uint8_t tmp[QR_BUF];
        if (!qrcodegen_encodeText(text, tmp, code, qrcodegen_Ecc_LOW, 1, QR_MAX_VERSION, qrcodegen_Mask_AUTO, true)) code[0] = 0;
    }
    lv_obj_invalidate(qr);
}

// ---- the popup

static lv_obj_t* popup = nullptr;
static void popup_close_cb(lv_event_t* e) { (void)e; if (popup) { lv_obj_del_async(popup); popup = nullptr; } }   // async: it may be its own button asking

void showQrPopup(const char* title, const char* path) {
    if (popup) { lv_obj_del(popup); popup = nullptr; }
    int W = lv_disp_get_hor_res(nullptr), H = lv_disp_get_ver_res(nullptr);
    bool big = W >= 480 || H >= 480;
    bool online = WiFi.status() == WL_CONNECTED;
    String url = online ? "http://" + WiFi.localIP().toString() + path : String("");

    popup = lv_obj_create(lv_layer_top());
    lv_obj_set_size(popup, W, H);
    lv_obj_set_pos(popup, 0, 0);
    lv_obj_set_style_bg_color(popup, lv_color_hex(0x11111b), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(popup, 245, LV_PART_MAIN);
    lv_obj_set_style_border_width(popup, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(popup, 0, LV_PART_MAIN);
    lv_obj_clear_flag(popup, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(popup, popup_close_cb, LV_EVENT_CLICKED, nullptr);   // a tap anywhere closes it

    int qs = H - 40 < W * 42 / 100 ? H - 40 : W * 42 / 100;
    lv_obj_t* q = qrCreate(popup, qs);
    lv_obj_align(q, LV_ALIGN_LEFT_MID, 14, 0);
    qrSetText(q, url.c_str());

    int tx = 14 + qs + 14, tw = W - tx - 10;
    lv_obj_t* t = lv_label_create(popup);
    lv_label_set_text(t, title);
    lv_label_set_long_mode(t, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(t, tw);
    lv_obj_set_style_text_font(t, big ? &lv_font_montserrat_28 : &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(t, -2, LV_PART_MAIN);
    lv_obj_set_style_text_color(t, lv_color_hex(0xfab387), LV_PART_MAIN);
    lv_obj_set_pos(t, tx, 10);

    lv_obj_t* hint = lv_label_create(popup);
    lv_label_set_text(hint, online ? "Scan with the phone camera (same Wi-Fi)." : "Connect the clock to Wi-Fi first.");
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(hint, tw);
    lv_obj_set_style_text_font(hint, big ? &lv_font_montserrat_20 : &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(hint, lv_color_hex(0xa6adc8), LV_PART_MAIN);
    lv_obj_align_to(hint, t, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 6);

    lv_obj_t* u = lv_label_create(popup);
    lv_label_set_text(u, url.c_str());
    lv_label_set_long_mode(u, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(u, tw);
    lv_obj_set_style_text_font(u, big ? &lv_font_montserrat_14 : &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(u, lv_color_hex(0xcba6f7), LV_PART_MAIN);
    lv_obj_align_to(u, hint, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);

    lv_obj_t* b = lv_btn_create(popup);
    lv_obj_set_size(b, tw, big ? 48 : 36);
    lv_obj_align(b, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x45475a), LV_PART_MAIN);
    lv_obj_set_style_radius(b, 10, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
    lv_obj_t* bl = lv_label_create(b);
    lv_label_set_text(bl, "Close");
    lv_obj_set_style_text_font(bl, big ? &lv_font_montserrat_20 : &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(bl, lv_color_hex(0xcdd6f4), LV_PART_MAIN);
    lv_obj_center(bl);
    lv_obj_add_event_cb(b, popup_close_cb, LV_EVENT_CLICKED, nullptr);
}

void closeQrPopup() { if (popup) { lv_obj_del(popup); popup = nullptr; } }
