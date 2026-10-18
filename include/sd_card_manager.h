#ifndef SD_CARD_MANAGER_H
#define SD_CARD_MANAGER_H

#include <Arduino.h>

// One lock for the SD card. The MP3 player reads it from its own task while the main loop may save the weather cache
// or a screenshot; two tasks in the SD library at once can corrupt a transfer or hang the bus. Hold an SdLock for the
// duration of any SD access outside the audio player.
class SdLock {
public:
    SdLock();
    ~SdLock();
};

class SdCardManager {
public:
    static bool begin();
    static bool isMounted();
    static void end();
    static bool isCardPresent();
};

#endif // SD_CARD_MANAGER_H
