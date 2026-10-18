#ifndef CONFIG_BACKUP_H
#define CONFIG_BACKUP_H

#include <Arduino.h>

// A copy of the settings that matter (Wi-Fi, zip code, time zone, alarm) in /config.json on the flash file system,
// apart from the preferences store. If that store is ever erased or corrupted, the copy brings everything back at
// the next start.

void configBackupSave();       // write the file; called whenever one of these settings changes
String configBackupStatus();   // JSON: size, Wi-Fi name, password length (not the password), zip, time zone
bool configBackupRestore();    // after settings.begin(): when the Wi-Fi name is missing, load the copy. true if restored

#endif // CONFIG_BACKUP_H
