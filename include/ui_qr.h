#ifndef UI_QR_H
#define UI_QR_H

#include <lvgl.h>

// A QR code widget that draws itself module by module (no image buffer, so it costs a few hundred bytes), and a
// popup that shows the clock's address for one of its web pages, to be scanned with a phone camera.
lv_obj_t* qrCreate(lv_obj_t* parent, int size);
void qrSetText(lv_obj_t* qr, const char* text);     // empty text: the widget shows nothing
void showQrPopup(const char* title, const char* path);   // path on this clock, e.g. "/" or "/settings"
void closeQrPopup();

#endif // UI_QR_H
