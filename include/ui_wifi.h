#ifndef UI_WIFI_H
#define UI_WIFI_H

#include <lvgl.h>

// On-device Wi-Fi setup: scan, pick a network, type the password on the touch keyboard, save, restart.
// Also a zip code box for the weather. Nothing needs a PC, a phone or a file.
void showWifiSetup();                                    // full-screen overlay
void showZipSetup();                                     // the zip code page only
lv_obj_t* createZipSettingsButton(lv_obj_t* parent, bool isLargeScreen);
bool isWifiSetupShown();
void showWifiPasswordPreview();                          // the password page only, no scan (layout check over the API)
void closeWifiSetup();
lv_obj_t* createWifiSettingsButton(lv_obj_t* parent, bool isLargeScreen);   // a row for the Settings tab
void refreshWifiSettingsButton();

#endif // UI_WIFI_H
