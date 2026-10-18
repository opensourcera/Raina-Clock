#ifndef WEATHER_ALERT_H
#define WEATHER_ALERT_H

#include <lvgl.h>
#include "weather_client.h"

// A one-line message bubble on the Clock page, like a chat message: what the next hours bring (storm, snow, rain soon,
// a freezing night, a fast drop or rise of the temperature, heat, fog) or a calm "nice weather" line. The most urgent
// one wins; urgent ones blink their icon. Picked from the hourly forecast, again every 15 seconds.
void createWeatherAlert(lv_obj_t* tab, bool isLargeScreen);
void weatherAlertUpdate(const WeatherData& data);
void weatherAlertDemo(int kind);         // layout checks: show message kind 0..11, -1 back to the forecast

#endif // WEATHER_ALERT_H
