#ifndef UI_TODO_H
#define UI_TODO_H

#include <lvgl.h>

// The To-do tab: one checkbox per item, tap to tick. Items come from TodoManager; edits on the web page /todo
// arrive through refreshTodoTab() from the main loop.
void createTodoTab(lv_obj_t* tab, bool isLandscape, bool isLargeScreen);
void refreshTodoTab();

#endif // UI_TODO_H
