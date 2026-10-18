#ifndef UI_MENU_H
#define UI_MENU_H

#include <lvgl.h>

// The page menu: big square buttons with icons that slide up from the bottom when an empty part of the screen is
// tapped, and slide away again once a page is chosen. It replaces the tab bar.
void createSlideMenu(lv_obj_t* tabview, bool isLargeScreen);
void showSlideMenu();
void hideSlideMenu();

#endif // UI_MENU_H
