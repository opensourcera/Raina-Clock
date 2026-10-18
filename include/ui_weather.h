#ifndef UI_WEATHER_H
#define UI_WEATHER_H

#include <lvgl.h>
#include "weather_client.h"

// The Weather page (the next hours and seven days, each with an icon, temperature and chance of rain) and the small
// current-weather panel beside the clock. Both are filled from the same WeatherData.
void createWeatherPage(lv_obj_t* tab, bool isLargeScreen);
void createClockWeatherPanel(lv_obj_t* tab, int x, int w, bool isLargeScreen);
void weatherPanelsUpdate(const WeatherData& data);

const char* getIconGlyph(int code);      // ui.cpp: weather icon font glyph for a WMO code
uint32_t getIconColor(int code);

#endif // UI_WEATHER_H
