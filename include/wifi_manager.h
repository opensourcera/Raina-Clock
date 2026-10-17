#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>

#ifndef NATIVE_TEST
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <FS.h>
#include <ImprovWiFiLibrary.h>
#endif

enum WifiState {
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_AP_MODE
};

// Set by the on-device Wi-Fi picker (ui_wifi.cpp) while it scans: the manager must not start connection attempts,
// which cancel a scan in progress.
extern volatile bool wifi_scan_hold;

class WifiManager {
public:
    WifiManager(const char* ssid, const char* password);
    void begin();
    void update();
    WifiState getState() const;
    String getIPAddress() const;
    int8_t getRSSI() const;
    void setCredentials(const String& ssid, const String& password);
    String getAPSSID();
    void applyScreenshotServerSetting(bool enabled);

private:
    void startAPMode();
    void handleRoot();
    void handleLanding();
    void handleSave();
    void handleNotFound();

    String _ssid;
    String _password;
    WifiState _state;
    unsigned long _lastReconnectAttempt;
    unsigned long _connectionStartTime;
    const unsigned long _reconnectInterval = 10000; // 10 seconds
    const unsigned long _connectionTimeout = 30000; // 30 seconds
    String _cachedNetworksHTML;

#ifndef NATIVE_TEST
    DNSServer* _dnsServer = nullptr;
    WebServer* _webServer = nullptr;
    ImprovWiFi* _improv = nullptr;
    fs::File _uploadFile;   // fs:: because TFT_eSPI turns the FS globals off
    void startWebServer();
    void stopWebServer();
    void handleScreenshot();
    void handleSettings();
    void handleSettingsSave();
    void handleTodo();
    void handleAlarm();
    void handleMusicUpload();
    void registerOTARoutes();
#endif
};

#endif // WIFI_MANAGER_H
