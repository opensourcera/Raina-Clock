#include "event_log.h"
#include <SPIFFS.h>
#include <WiFi.h>
#include <esp_system.h>
#include <stdarg.h>
#include <time.h>

static const char* LOG_FILE = "/log.txt";
static const char* LOG_OLD = "/log.old";
static const size_t LOG_LIMIT = 24 * 1024;

static bool fs_ok = false;
static SemaphoreHandle_t fs_mx = nullptr;

static const char* resetName(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_POWERON:   return "power on";
        case ESP_RST_EXT:       return "reset pin";
        case ESP_RST_SW:        return "software restart";
        case ESP_RST_PANIC:     return "CRASH (panic/exception)";
        case ESP_RST_INT_WDT:   return "HANG (interrupt watchdog)";
        case ESP_RST_TASK_WDT:  return "HANG (task watchdog)";
        case ESP_RST_WDT:       return "HANG (other watchdog)";
        case ESP_RST_DEEPSLEEP: return "deep sleep wake";
        case ESP_RST_BROWNOUT:  return "BROWNOUT (power dipped)";
        case ESP_RST_SDIO:      return "sdio";
        default:                return "unknown";
    }
}

void eventLogBegin() {
    if (!fs_mx) fs_mx = xSemaphoreCreateRecursiveMutex();
    fs_ok = SPIFFS.begin(true);   // formats the partition the first time
    Serial.printf("[log] flash file system %s\n", fs_ok ? "ready" : "NOT available");
    esp_reset_reason_t r = esp_reset_reason();
    eventLog("BOOT reason: %s, free heap %u, largest block %u", resetName(r), (unsigned)ESP.getFreeHeap(), (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

void eventLog(const char* fmt, ...) {
    char msg[200];
    va_list ap; va_start(ap, fmt); vsnprintf(msg, sizeof(msg), fmt, ap); va_end(ap);
    char line[260];
    time_t now = time(nullptr);
    if (now > 1700000000) {
        struct tm t; localtime_r(&now, &t);
        snprintf(line, sizeof(line), "%04d-%02d-%02d %02d:%02d:%02d  %s\n", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, msg);
    } else {
        snprintf(line, sizeof(line), "up %lus  %s\n", (unsigned long)(millis() / 1000), msg);   // clock not set yet
    }
    Serial.print("[log] "); Serial.print(line);
    if (!fs_ok || !fs_mx) return;
    xSemaphoreTakeRecursive(fs_mx, portMAX_DELAY);
    File f = SPIFFS.open(LOG_FILE, FILE_APPEND);
    if (f) {
        f.print(line);
        size_t sz = f.size();
        f.close();
        if (sz > LOG_LIMIT) { SPIFFS.remove(LOG_OLD); SPIFFS.rename(LOG_FILE, LOG_OLD); }
    }
    xSemaphoreGiveRecursive(fs_mx);
}

String eventLogRead() {
    String out;
    if (!fs_ok || !fs_mx) return out;
    xSemaphoreTakeRecursive(fs_mx, portMAX_DELAY);
    const char* names[2] = {LOG_OLD, LOG_FILE};
    for (int i = 0; i < 2; i++) {
        File f = SPIFFS.open(names[i], FILE_READ);
        if (f) { out += f.readString(); f.close(); }
    }
    xSemaphoreGiveRecursive(fs_mx);
    return out;
}

void eventLogSend(void (*sink)(const char* data, size_t len, void* ctx), void* ctx) {
    if (!fs_ok || !fs_mx) return;
    xSemaphoreTakeRecursive(fs_mx, portMAX_DELAY);
    const char* names[2] = {LOG_OLD, LOG_FILE};
    char buf[512];
    for (int i = 0; i < 2; i++) {
        File f = SPIFFS.open(names[i], FILE_READ);
        if (!f) continue;
        for (int n; (n = f.read((uint8_t*)buf, sizeof(buf))) > 0; ) sink(buf, (size_t)n, ctx);
        f.close();
    }
    xSemaphoreGiveRecursive(fs_mx);
}

void eventLogClear() {
    if (!fs_ok || !fs_mx) return;
    xSemaphoreTakeRecursive(fs_mx, portMAX_DELAY);
    SPIFFS.remove(LOG_FILE); SPIFFS.remove(LOG_OLD);
    xSemaphoreGiveRecursive(fs_mx);
}

void eventLogHeartbeat() {
    static uint32_t last = 0;
    if (last != 0 && millis() - last < 600000UL) return;
    last = millis();
    if (last == 0) last = 1;
    eventLog("alive: up %lu min, heap %u (lowest %u), wifi %s", (unsigned long)(millis() / 60000UL), (unsigned)ESP.getFreeHeap(),
             (unsigned)ESP.getMinFreeHeap(), WiFi.status() == WL_CONNECTED ? "connected" : "not connected");
}

bool fsWriteFile(const char* path, const String& text) {
    if (!fs_ok || !fs_mx) return false;
    xSemaphoreTakeRecursive(fs_mx, portMAX_DELAY);
    String tmp = String(path) + ".tmp";
    bool ok = false;
    File f = SPIFFS.open(tmp, FILE_WRITE);
    if (f) {
        ok = f.print(text) == text.length();
        f.close();
        if (ok) { SPIFFS.remove(path); ok = SPIFFS.rename(tmp, path); }   // never leave a half-written file behind
    }
    xSemaphoreGiveRecursive(fs_mx);
    return ok;
}

String fsReadFile(const char* path) {
    String out;
    if (!fs_ok || !fs_mx) return out;
    xSemaphoreTakeRecursive(fs_mx, portMAX_DELAY);
    File f = SPIFFS.open(path, FILE_READ);
    if (f) { out = f.readString(); f.close(); }
    xSemaphoreGiveRecursive(fs_mx);
    return out;
}
