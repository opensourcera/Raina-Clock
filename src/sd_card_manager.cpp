#include "sd_card_manager.h"
#include <SPI.h>
#include <SD.h>
#include <FS.h>
#include "config/config.h"

static SemaphoreHandle_t sdMutex = nullptr;
static portMUX_TYPE sdMutexInit = portMUX_INITIALIZER_UNLOCKED;
SdLock::SdLock() {
    if (!sdMutex) {
        SemaphoreHandle_t m = xSemaphoreCreateRecursiveMutex();
        portENTER_CRITICAL(&sdMutexInit);
        if (!sdMutex) { sdMutex = m; m = nullptr; }
        portEXIT_CRITICAL(&sdMutexInit);
        if (m) vSemaphoreDelete(m);
    }
    xSemaphoreTakeRecursive(sdMutex, portMAX_DELAY);
}
SdLock::~SdLock() { xSemaphoreGiveRecursive(sdMutex); }

static SPIClass sdSPI(VSPI);
static bool sdMounted = false;
static bool sdCardDetected = false;

bool SdCardManager::begin() {
    SdLock lock;
    if (sdMounted) {
        return true;
    }
    
    Serial.println("Initializing SD card...");
    
    sdSPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    sdMounted = SD.begin(SD_CS_PIN, sdSPI, 4000000, "/sd", 5, true);

    if (sdMounted) {
        Serial.println("SD card initialized successfully.");
        sdCardDetected = true;
    } else {
        Serial.println("Failed to initialize SD card.");
        sdCardDetected = false;
    }
    return sdMounted;
}

bool SdCardManager::isMounted() {
    return sdMounted;
}

void SdCardManager::end() {
    // The card stays mounted once it is: mounting again later needs a block of memory the screen has long since
    // broken up (esp_vfs_fat_register then fails with "no memory"). It is mounted at boot, before the screen is built.
    return;
    SdLock lock;
    if (sdMounted) {
        SD.end();
        sdMounted = false;
        Serial.println("SD card unmounted.");
    }
    sdSPI.end();
}

bool SdCardManager::isCardPresent() {
    return sdCardDetected;
}
