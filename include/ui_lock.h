#ifndef UI_LOCK_H
#define UI_LOCK_H

// One recursive mutex for everything that touches the screen and the clock's settings. The main loop holds it while it
// works; the web server task takes it only while a page handler runs, so a slow or silent browser connection can stall
// the web task but never the screen.

#include <Arduino.h>

inline SemaphoreHandle_t uiMutex() {
    static SemaphoreHandle_t m = xSemaphoreCreateRecursiveMutex();
    return m;
}

struct UiLock {
    UiLock()  { xSemaphoreTakeRecursive(uiMutex(), portMAX_DELAY); }
    ~UiLock() { xSemaphoreGiveRecursive(uiMutex()); }
};

#endif // UI_LOCK_H
