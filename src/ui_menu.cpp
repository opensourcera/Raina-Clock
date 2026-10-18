#include "ui_menu.h"

// The tab bar is gone. A tap on any empty part of the screen slides this sheet of big square buttons up from the
// bottom; choosing one shows that page and slides the sheet away again. A tap on the dimmed area above closes it.

LV_FONT_DECLARE(weather_icons_24);

static lv_obj_t* menu_tv = nullptr;
static lv_obj_t* backdrop = nullptr;
static lv_obj_t* sheet = nullptr;
static lv_obj_t* items[4] = {nullptr};
static bool menu_open = false;
static int sheet_h = 0, scr_h = 0;

static const int ITEMS = 4;
static const char* names[ITEMS] = {"Clock", "Weather", "To-do", "Setup"};
static const int pages[ITEMS] = {4, 1, 3, 5};      // tab index of each (the Weather page lives in the old Days tab)
static const char* icons[ITEMS] = {LV_SYMBOL_BELL, "\xEF\x80\x8D" /* sun, weather icons */, LV_SYMBOL_OK, LV_SYMBOL_SETTINGS};

static void anim_y(void* o, int32_t v) { lv_obj_set_y((lv_obj_t*)o, v); }
static void anim_opa(void* o, int32_t v) { lv_obj_set_style_bg_opa((lv_obj_t*)o, (lv_opa_t)v, LV_PART_MAIN); }
static void hidden_cb(lv_anim_t* a) {
    (void)a;
    if (menu_open) return;                       // opened again while closing
    lv_obj_add_flag(sheet, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(backdrop, LV_OBJ_FLAG_HIDDEN);
}

static void slide(bool open) {
    int from = lv_obj_get_y(sheet), to = open ? scr_h - sheet_h : scr_h;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, sheet);
    lv_anim_set_exec_cb(&a, anim_y);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_time(&a, open ? 260 : 200);
    lv_anim_set_path_cb(&a, open ? lv_anim_path_overshoot : lv_anim_path_ease_in);
    if (!open) lv_anim_set_ready_cb(&a, hidden_cb);
    lv_anim_start(&a);

    lv_anim_t b;
    lv_anim_init(&b);
    lv_anim_set_var(&b, backdrop);
    lv_anim_set_exec_cb(&b, anim_opa);
    lv_anim_set_values(&b, open ? 0 : 140, open ? 140 : 0);
    lv_anim_set_time(&b, open ? 260 : 200);
    lv_anim_start(&b);
}

void showSlideMenu() {
    if (!sheet || menu_open) return;
    menu_open = true;
    int cur = (int)lv_tabview_get_tab_act(menu_tv);
    for (int i = 0; i < ITEMS; i++) {
        bool on = pages[i] == cur;
        lv_obj_set_style_bg_color(items[i], lv_color_hex(on ? 0xcba6f7 : 0x313244), LV_PART_MAIN);
        lv_obj_set_style_text_color(items[i], lv_color_hex(on ? 0x11111b : 0xcdd6f4), LV_PART_MAIN);
    }
    lv_obj_clear_flag(backdrop, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sheet, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(backdrop);
    lv_obj_move_foreground(sheet);
    slide(true);
}

void hideSlideMenu() {
    if (!sheet || !menu_open) return;
    menu_open = false;
    slide(false);
}

static void item_cb(lv_event_t* e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    lv_tabview_set_act(menu_tv, pages[i], LV_ANIM_OFF);
    hideSlideMenu();
}
static void backdrop_cb(lv_event_t* e) { (void)e; hideSlideMenu(); }
static void blank_tap_cb(lv_event_t* e) { (void)e; showSlideMenu(); }

// Every plain container on the pages that has no job of its own opens the menu when tapped. Labels and pictures are
// not clickable, so a tap on them reaches their container. Pages rebuild parts of themselves, so this runs again now
// and then; containers that already have a handler (ours or their own) are left alone.
static void hookBlank(lv_obj_t* o) {
    if (lv_obj_check_type(o, &lv_obj_class) && lv_obj_has_flag(o, LV_OBJ_FLAG_CLICKABLE)) {
        bool busy = o->spec_attr && o->spec_attr->event_dsc_cnt > 0;
        if (!busy) lv_obj_add_event_cb(o, blank_tap_cb, LV_EVENT_SHORT_CLICKED, nullptr);
    }
    uint32_t n = lv_obj_get_child_cnt(o);
    for (uint32_t i = 0; i < n; i++) hookBlank(lv_obj_get_child(o, i));
}
static void rehook_cb(lv_timer_t* t) {
    (void)t;
    lv_obj_t* cont = lv_tabview_get_content(menu_tv);
    uint32_t n = lv_obj_get_child_cnt(cont);
    for (uint32_t i = 0; i < n; i++) hookBlank(lv_obj_get_child(cont, i));
}

void createSlideMenu(lv_obj_t* tabview, bool isLargeScreen) {
    menu_tv = tabview;
    int W = lv_disp_get_hor_res(nullptr);
    scr_h = lv_disp_get_ver_res(nullptr);
    bool tall = scr_h > W;
    int cols = tall ? 2 : 4, rows = tall ? 2 : 1;
    int gap = isLargeScreen ? 12 : 8;
    int bw = (W - (cols + 1) * gap) / cols;
    int bh = isLargeScreen ? 110 : (tall ? 90 : 78);
    int top = isLargeScreen ? 22 : 18;                 // room for the grab handle
    sheet_h = top + rows * bh + rows * gap;

    backdrop = lv_obj_create(lv_layer_top());
    lv_obj_set_size(backdrop, W, scr_h);
    lv_obj_set_pos(backdrop, 0, 0);
    lv_obj_set_style_bg_color(backdrop, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(backdrop, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(backdrop, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(backdrop, 0, LV_PART_MAIN);
    lv_obj_clear_flag(backdrop, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(backdrop, backdrop_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(backdrop, LV_OBJ_FLAG_HIDDEN);

    sheet = lv_obj_create(lv_layer_top());
    lv_obj_set_size(sheet, W, sheet_h + 20);           // 20 px more below, for the overshoot of the slide
    lv_obj_set_pos(sheet, 0, scr_h);
    lv_obj_set_style_bg_color(sheet, lv_color_hex(0x181825), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sheet, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(sheet, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(sheet, 18, LV_PART_MAIN);
    lv_obj_set_style_pad_all(sheet, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(sheet, 24, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(sheet, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(sheet, 160, LV_PART_MAIN);
    lv_obj_clear_flag(sheet, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(sheet, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* grab = lv_obj_create(sheet);
    lv_obj_set_size(grab, 44, 5);
    lv_obj_set_style_bg_color(grab, lv_color_hex(0x6c7086), LV_PART_MAIN);
    lv_obj_set_style_border_width(grab, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(grab, 3, LV_PART_MAIN);
    lv_obj_clear_flag(grab, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(grab, LV_ALIGN_TOP_MID, 0, (top - 5) / 2);

    const lv_font_t* fname = isLargeScreen ? &lv_font_montserrat_20 : &lv_font_montserrat_14;
    for (int i = 0; i < ITEMS; i++) {
        lv_obj_t* b = lv_btn_create(sheet);
        items[i] = b;
        lv_obj_set_size(b, bw, bh);
        lv_obj_set_pos(b, gap + (i % cols) * (bw + gap), top + (i / cols) * (bh + gap));
        lv_obj_set_style_radius(b, 14, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(b, 0, LV_PART_MAIN);
        lv_obj_set_style_transform_width(b, -4, LV_STATE_PRESSED);   // a little press-in
        lv_obj_set_style_transform_height(b, -4, LV_STATE_PRESSED);
        lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_row(b, isLargeScreen ? 8 : 4, LV_PART_MAIN);
        lv_obj_t* ic = lv_label_create(b);
        lv_label_set_text(ic, icons[i]);
        lv_obj_set_style_text_font(ic, i == 1 ? &weather_icons_24 : &lv_font_montserrat_28, LV_PART_MAIN);
        lv_obj_t* nm = lv_label_create(b);
        lv_label_set_text(nm, names[i]);
        lv_obj_set_style_text_font(nm, fname, LV_PART_MAIN);
        lv_obj_add_event_cb(b, item_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }

    lv_tabview_set_act(tabview, 4, LV_ANIM_OFF);      // the clock is the home page
    rehook_cb(nullptr);
    lv_timer_create(rehook_cb, 2000, nullptr);
}
