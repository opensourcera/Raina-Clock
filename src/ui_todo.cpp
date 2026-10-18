#include "ui_todo.h"
#include "todo_manager.h"
#include "settings_manager.h"
#include "catppuccin.h"
#include "config/config.h"
#include "time_utils.h"
#include <WiFi.h>
#include <vector>
#include <algorithm>
#include "ui_qr.h"

extern SettingsManager settings;
int getCurrentThemeFlavor();   // catppuccin.cpp

// The To-do page shows today like a bullet journal: urgent tasks first (red "!"), then the rest, then the ones done
// today with a tick (unless "hide done" is on). Open tasks from earlier days come along until they are done. A tap
// ticks a task. The QR code in the corner opens the phone page /todo, where the list is written and planned.

static lv_obj_t* todo_tab = nullptr;
static lv_obj_t* todo_list = nullptr;
static lv_obj_t* qr_obj = nullptr;
static bool todo_large = false;
static const int TODO_SHOW_CHARS = 30;

// ---- the QR code: it follows the clock's address, which may arrive after the page is built

static String qr_url;
static void qrMake() {
    String url = WiFi.status() == WL_CONNECTED ? "http://" + WiFi.localIP().toString() + "/todo" : String("");
    if (url == qr_url || qr_obj == nullptr) return;
    qr_url = url;
    qrSetText(qr_obj, url.c_str());
    if (url.length()) lv_obj_clear_flag(qr_obj, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(qr_obj, LV_OBJ_FLAG_HIDDEN);
}
static void qr_timer_cb(lv_timer_t* t) { (void)t; qrMake(); }

// ---- the list

static void todo_item_cb(lv_event_t* e) {
    size_t index = (size_t)(intptr_t)lv_event_get_user_data(e);
    if (todo.toggle(index)) {
        todo.save();
        refreshTodoTab();
    }
}

static String todayString() {
    struct tm t;
    if (!getLocalTimeWrapper(&t, 0)) return "";
    char b[12];
    snprintf(b, sizeof(b), "%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    return String(b);
}

void createTodoTab(lv_obj_t* tab, bool isLandscape, bool isLargeScreen) {
    (void)isLandscape;
    todo_tab = tab;
    todo_large = isLargeScreen;
    lv_obj_set_layout(tab, 0);
    lv_obj_set_style_pad_all(tab, 0, LV_PART_MAIN);

    int W = lv_disp_get_hor_res(nullptr);
    int qrW = isLargeScreen ? 120 : 84;
    qr_obj = qrCreate(tab, qrW);
    lv_obj_align(qr_obj, LV_ALIGN_TOP_RIGHT, -6, 6);
    lv_obj_t* cap = lv_label_create(tab);
    lv_label_set_text(cap, "Plan on\nyour phone");
    lv_obj_set_style_text_align(cap, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(cap, isLargeScreen ? &lv_font_montserrat_14 : &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_set_style_text_color(cap, lv_color_hex(0x9399b2), LV_PART_MAIN);
    lv_obj_align_to(cap, qr_obj, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);

    todo_list = lv_obj_create(tab);
    lv_obj_set_size(todo_list, W - qrW - 18, LV_PCT(100));
    lv_obj_set_pos(todo_list, 0, 0);
    lv_obj_set_flex_flow(todo_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(todo_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_bg_opa(todo_list, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(todo_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(todo_list, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_row(todo_list, isLargeScreen ? 6 : 2, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(todo_list, LV_SCROLLBAR_MODE_AUTO);

    qr_url = "-";                                                  // force the first computation
    qrMake();
    lv_timer_create(qr_timer_cb, 5000, nullptr);
    refreshTodoTab();
}

void refreshTodoTab() {
    if (todo_list == nullptr) return;
    lv_obj_clean(todo_list);
    const auto& flavor = getCatppuccinFlavor(getCurrentThemeFlavor());
    const lv_font_t* font = todo_large ? &lv_font_montserrat_20 : &lv_font_montserrat_14;
    const auto& items = todo.items();
    String today = todayString();

    // Today's page: tasks of today, tasks without a day, and open tasks carried over from earlier days
    std::vector<size_t> shown;
    for (size_t i = 0; i < items.size(); i++) {
        const TodoItem& it = items[i];
        bool mine = today.length() == 0 || it.day.length() == 0 || it.day == today || (!it.done && it.day < today);
        if (!mine || (it.done && todo.hideDone())) continue;
        shown.push_back(i);
    }
    auto rank = [&](size_t i) { const TodoItem& it = items[i]; return it.done ? 2 : (it.urgent ? 0 : 1); };
    std::stable_sort(shown.begin(), shown.end(), [&](size_t a, size_t b) { return rank(a) < rank(b); });

    int open = 0;
    for (size_t i : shown) if (!items[i].done) open++;
    lv_obj_t* head = lv_label_create(todo_list);
    char buf[64];
    if (shown.empty()) lv_snprintf(buf, sizeof(buf), "Today: nothing to do");
    else lv_snprintf(buf, sizeof(buf), "Today: %d open", open);
    lv_label_set_text(head, buf);
    lv_obj_set_style_text_color(head, TO_LV_COLOR(flavor.peach), LV_PART_MAIN);
    lv_obj_set_style_text_font(head, font, LV_PART_MAIN);

    for (size_t i : shown) {
        const TodoItem& it = items[i];
        lv_obj_t* cb = lv_checkbox_create(todo_list);
        String text = it.text;
        if (text.length() > (unsigned)TODO_SHOW_CHARS) text = text.substring(0, TODO_SHOW_CHARS - 1) + "...";
        if (it.urgent && !it.done) text = "! " + text;
        lv_checkbox_set_text(cb, text.c_str());
        lv_obj_set_width(cb, LV_PCT(100));
        lv_obj_set_style_text_font(cb, font, LV_PART_MAIN);
        uint32_t col = it.done ? 0x6c7086 : (it.urgent ? 0xf38ba8 : 0xcdd6f4);
        lv_obj_set_style_text_color(cb, lv_color_hex(col), LV_PART_MAIN);
        lv_obj_set_style_pad_top(cb, 4, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(cb, 4, LV_PART_MAIN);
        lv_obj_set_style_pad_column(cb, 10, LV_PART_MAIN);
        // A round bullet; ticked green when done
        lv_obj_set_style_radius(cb, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
        lv_obj_set_style_border_color(cb, lv_color_hex(it.urgent ? 0xf38ba8 : 0x7f849c), LV_PART_INDICATOR);
        lv_obj_set_style_border_width(cb, 2, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(cb, TO_LV_COLOR(flavor.base), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(cb, lv_color_hex(0xa6e3a1), LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_border_color(cb, lv_color_hex(0xa6e3a1), LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_ext_click_area(cb, 6);
        if (it.done) lv_obj_add_state(cb, LV_STATE_CHECKED);
        lv_obj_add_event_cb(cb, todo_item_cb, LV_EVENT_VALUE_CHANGED, (void*)(intptr_t)i);
    }
}
