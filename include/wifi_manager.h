#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>

#ifndef NATIVE_TEST
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <FS.h>
#include <ImprovWiFiLibrary.h>
#include "ui_lock.h"

// A WebServer whose page handlers run under the UI lock (see ui_lock.h)
class LockedWebServer : public WebServer {
public:
    using WebServer::WebServer;
    void on(const Uri& u, THandlerFunction fn) { WebServer::on(u, wrap(fn)); }
    void on(const Uri& u, HTTPMethod m, THandlerFunction fn) { WebServer::on(u, m, wrap(fn)); }
    void on(const Uri& u, HTTPMethod m, THandlerFunction fn, THandlerFunction ufn) { WebServer::on(u, m, wrap(fn), wrap(ufn)); }
    void onNotFound(THandlerFunction fn) { WebServer::onNotFound(wrap(fn)); }
private:
    static THandlerFunction wrap(THandlerFunction fn) { return [fn]() { UiLock g; fn(); }; }
};
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
    LockedWebServer* _webServer = nullptr;
    volatile int _webWant = 0;   // what the web task should run: 0 nothing, 1 settings server (STA), 2 portal (AP)
    int _webHave = 0;
    TaskHandle_t _webTask = nullptr;
    void buildApServer();
    void teardownServers();
public:
    void webLoop();              // the web task body
private:
    ImprovWiFi* _improv = nullptr;
    void startWebServer();
    void stopWebServer();
    void handleScreenshot();
    void handleSettings();
    void handleSettingsSave();
    void handleTodo();
    void handleAlarm();
    void registerOTARoutes();
#endif
};

#endif // WIFI_MANAGER_H
