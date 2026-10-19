#ifndef WEATHER_CLIENT_H
#define WEATHER_CLIENT_H

#include <Arduino.h>

struct ForecastDay {
    float tempMax = 0;
    float tempMin = 0;
    int weatherCode = -1;       // -1: no data for this day
    int rainProb = 0;           // highest chance of rain that day, percent
    String status;
    String dayName;
};

struct HourlyForecast {
    float temperature = 0;
    int precipitationProbability = 0;
    int weatherCode = -1;
    int hour = -1;              // local hour of day, 0-23; -1 unknown
    bool isDay = true;          // between sunrise and sunset
};

static const int FORECAST_DAYS = 7;

struct WeatherData {
    float temperature;
    int humidity;
    String status;
    bool valid;
    int weatherCode;
    float windSpeed;
    int windDirection;
    String cityName;
    ForecastDay forecast[7];
    HourlyForecast hourly[24];
    bool isDay;                 // the current weather is between sunrise and sunset (no default: keeps the brace initialisers)
};

class WeatherClient {
public:
    WeatherClient();
    WeatherData fetchWeather();
    const String& getCityName() const { return _cityName; }
    static String getWeatherDesc(int code);
    static void reserveMemory();   // at boot, before the screen is built: the parser's 12 KB in one piece
    static bool parseWeatherJson(const char* json, WeatherData& data, time_t ref_time = 0);
    bool parseOwmJson(const char* json, WeatherData& data, time_t ref_time = 0);
    static String serializeWeatherData(const WeatherData& data);
    static bool deserializeWeatherData(const String& json, WeatherData& data);
    bool isLocationEmpty();
    bool fetchIpLocation(String& latStr, String& lonStr, String& city);
    static bool parseIpLocationJson(const char* json, float& lat, float& lon, String& city);

private:
    bool geocodeZip();
    bool reverseGeocode();
    static int owmToWmoCode(int owmCode);

    String _latitude;
    String _longitude;
    String _zipCode;
    bool _useZip;
    bool _zipResolved;
    String _resolvedLat;
    String _resolvedLng;
    String _cityName;
};

#endif // WEATHER_CLIENT_H
