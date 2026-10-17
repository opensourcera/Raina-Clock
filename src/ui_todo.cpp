#include "ui_todo.h"
#include "todo_manager.h"
#include "settings_manager.h"
#include "catppuccin.h"
#include "config/config.h"

extern SettingsManager settings;
int getCurrentThemeFlavor();   // catppuccin.cpp

static lv_obj_t* todo_tab = nullptr;
static bool todo_large = false;
static const int TODO_SHOW_CHARS = 38;          // one line on the 2.8" screen at 14 px

static void todo_item_cb(lv_event_t* e) {
    size_t index = (size_t)(intptr_t)lv_event_get_user_data(e);
    if (todo.toggle(index)) {
        todo.save();
        refreshTodoTab();
    }
}

void createTodoTab(lv_obj_t* tab, bool isLandscape, bool isLargeScreen) {
    (void)isLandscape;
    todo_tab = tab;
    todo_large = isLargeScreen;
    lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tab, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(tab, isLargeScreen ? 8 : 4, LV_PART_MAIN);
    refreshTodoTab();
}

void refreshTodoTab() {
    if (todo_tab == nullptr) return;
    lv_obj_clean(todo_tab);
    const auto& flavor = getCatppuccinFlavor(getCurrentThemeFlavor());
    const lv_font_t* font = todo_large ? &lv_font_montserrat_20 : &lv_font_montserrat_14;
    const lv_font_t* small = todo_large ? &lv_font_montserrat_14 : &lv_font_montserrat_10;
    const auto& items = todo.items();

    // Header line: how much is left, and where to edit
    lv_obj_t* head = lv_label_create(todo_tab);
    char buf[96];
    if (items.empty()) {
        lv_snprintf(buf, sizeof(buf), "Nothing to do");
    } else {
        lv_snprintf(buf, sizeof(buf), "%d of %d open", (int)todo.openCount(), (int)items.size());
    }
    lv_label_set_text(head, buf);
    lv_obj_set_style_text_color(head, TO_LV_COLOR(flavor.peach), LV_PART_MAIN);
    lv_obj_set_style_text_font(head, font, LV_PART_MAIN);

    for (size_t i = 0; i < items.size(); i++) {
        lv_obj_t* cb = lv_checkbox_create(todo_tab);
        String text = items[i].text;
        if (text.length() > (unsigned)TODO_SHOW_CHARS) text = text.substring(0, TODO_SHOW_CHARS - 1) + "...";
        lv_checkbox_set_text(cb, text.c_str());
        lv_obj_set_style_text_font(cb, font, LV_PART_MAIN);
        lv_obj_set_style_text_color(cb, TO_LV_COLOR(items[i].done ? flavor.overlay0 : flavor.text), LV_PART_MAIN);
        lv_obj_set_style_pad_top(cb, 4, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(cb, 4, LV_PART_MAIN);
        lv_obj_set_style_pad_column(cb, 10, LV_PART_MAIN);
        // The box: outlined in the text colour, filled peach when ticked
        lv_obj_set_style_border_color(cb, TO_LV_COLOR(flavor.overlay0), LV_PART_INDICATOR);
        lv_obj_set_style_border_width(cb, 2, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(cb, TO_LV_COLOR(flavor.base), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(cb, TO_LV_COLOR(flavor.peach), LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_border_color(cb, TO_LV_COLOR(flavor.peach), LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_radius(cb, 4, LV_PART_INDICATOR);
        lv_obj_set_ext_click_area(cb, 6);
        if (items[i].done) lv_obj_add_state(cb, LV_STATE_CHECKED);
        lv_obj_add_event_cb(cb, todo_item_cb, LV_EVENT_VALUE_CHANGED, (void*)(intptr_t)i);
    }

    // Footer hint
    lv_obj_t* hint = lv_label_create(todo_tab);
    lv_snprintf(buf, sizeof(buf), "Edit at http://%s.local/todo", settings.getHostname().c_str());
    lv_label_set_text(hint, buf);
    lv_obj_set_style_text_color(hint, TO_LV_COLOR(flavor.overlay0), LV_PART_MAIN);
    lv_obj_set_style_text_font(hint, small, LV_PART_MAIN);
    lv_obj_set_style_pad_top(hint, 6, LV_PART_MAIN);
}
