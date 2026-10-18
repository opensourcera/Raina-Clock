#include "config_backup.h"
#include "event_log.h"
#include "settings_manager.h"
#include "alarm_manager.h"
#include <ArduinoJson.h>
#include <string>
#include <SPIFFS.h>
#include <lvgl.h>

extern SettingsManager settings;

static const char* CONFIG_FILE = "/config.json";
static bool restoring = false;   // the setters called during a restore must not write the file again

void configBackupSave() {
    if (restoring) return;
    DynamicJsonDocument doc(1024);
    String ssid = settings.getWifiSSID(), pass = settings.getWifiPassword();
    if (ssid.length() == 0 || ssid == "YOUR_WIFI_SSID") {   // never replace a good copy with an empty one
        DynamicJsonDocument old(1024);
        if (!deserializeJson(old, fsReadFile(CONFIG_FILE).c_str()) && String(old["wifi_ssid"] | "").length() > 0) {
            ssid = String(old["wifi_ssid"] | ""); pass = String(old["wifi_password"] | "");
        }
    }
    doc["wifi_ssid"] = ssid;
    doc["wifi_password"] = pass;
    doc["zip"] = settings.getZipCode();
    doc["timezone"] = settings.getTimezone();
    const AlarmConfig& a = alarmClock.cfg();
    JsonObject al = doc.createNestedObject("alarm");
    al["enabled"] = a.enabled; al["hour"] = a.hour; al["minute"] = a.minute; al["repeat"] = a.repeat;
    al["snooze_minutes"] = a.snoozeMinutes; al["volume"] = a.volume;
    std::string out;
    serializeJson(doc, out);
    if (!fsWriteFile(CONFIG_FILE, String(out.c_str()))) eventLog("config backup: could not write %s", CONFIG_FILE);
}

String configBackupStatus() {   // for the web page: what the copy holds, without the password itself
    DynamicJsonDocument doc(1024), out(512);
    String text = fsReadFile(CONFIG_FILE);
    out["file_bytes"] = text.length();
    { lv_mem_monitor_t m; lv_mem_monitor(&m); out["lvgl_used_pct"] = m.used_pct; out["lvgl_free"] = m.free_size; out["lvgl_frag_pct"] = m.frag_pct; }
    out["heap"] = ESP.getFreeHeap(); out["heap_largest"] = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    out["fs_total"] = SPIFFS.totalBytes(); out["fs_used"] = SPIFFS.usedBytes();
    { File f = SPIFFS.open("/log.txt", "r"); out["log_bytes"] = f ? (int)f.size() : -1; if (f) f.close(); }
    { File f = SPIFFS.open("/log.old", "r"); out["log_old_bytes"] = f ? (int)f.size() : -1; if (f) f.close(); }
    if (text.length() && !deserializeJson(doc, text.c_str())) {
        out["wifi_ssid"] = doc["wifi_ssid"].as<String>();
        out["wifi_password_length"] = String(doc["wifi_password"] | "").length();
        out["zip"] = doc["zip"].as<String>();
        out["timezone"] = doc["timezone"].as<String>();
    }
    std::string o; serializeJson(out, o);
    return String(o.c_str());
}

bool configBackupRestore() {
    const String& have = settings.getWifiSSID();
    if (have.length() > 0 && have != "YOUR_WIFI_SSID") return false;   // the preferences store is fine
    String text = fsReadFile(CONFIG_FILE);
    if (text.length() == 0) return false;
    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, text.c_str())) { eventLog("config backup: %s is damaged, ignored", CONFIG_FILE); return false; }
    String ssid = doc["wifi_ssid"] | "";
    if (ssid.length() == 0) return false;
    restoring = true;
    settings.setWifiSSID(ssid);
    settings.setWifiPassword(String(doc["wifi_password"] | ""));
    String zip = doc["zip"] | "";
    if (zip.length() >= 3) settings.setZipCode(zip);
    String tz = doc["timezone"] | "";
    if (tz.length() > 0) settings.setTimezone(tz);
    if (doc.containsKey("alarm")) {
        std::string aj; serializeJson(doc["alarm"], aj);
        alarmClock.applyJson(String(aj.c_str()), nullptr);
        alarmClock.save();
    }
    restoring = false;
    eventLog("config backup: preferences were empty, restored Wi-Fi '%s', zip %s from %s", ssid.c_str(), zip.c_str(), CONFIG_FILE);
    return true;
}
