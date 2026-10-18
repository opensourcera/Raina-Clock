#ifndef EVENT_LOG_H
#define EVENT_LOG_H

// A small persistent log and a few files on the internal flash (SPIFFS), so that a hang, a reboot or a lost setting
// can be explained afterwards. The log keeps the last ~48 KB (two files), is also printed to the serial port, and is
// served by the web interface at /log.

#include <Arduino.h>

void eventLogBegin();                          // mount the flash file system, log why the chip (re)started
void eventLog(const char* fmt, ...) __attribute__((format(printf, 1, 2)));   // thread-safe, printf style
void eventLogSend(void (*sink)(const char* data, size_t len, void* ctx), void* ctx);   // the log in 512-byte pieces, oldest first
String eventLogRead();                         // the whole log, oldest first
void eventLogClear();
void eventLogHeartbeat();                      // call often; writes a status line about every 10 minutes

bool fsWriteFile(const char* path, const String& text);   // false when the flash file system is not available
String fsReadFile(const char* path);                      // "" when missing

#endif // EVENT_LOG_H
