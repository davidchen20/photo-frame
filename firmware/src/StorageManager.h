#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <SPI.h>
#include <vector>
#include <memory>

enum class StorageMode {
    SD_CARD,
    NONE
};

/*
  Manages the local photo cache and synchronization with the cloud

  Supabase is the source of truth and the SD card is used for playback

  SD card stores at most 60 GB of photos and uses FIFO eviction
*/
class StorageManager {
private:
  const char* apiUrl;
  int sdCsPin;
  StorageMode storageMode;
  unsigned long lastCloudSync;

  // contains the ids of all the photos in cache
  std::vector<String> localPhotos;

  bool scanSdCard();
  bool isCached(const String& id) const;

public:
  StorageManager(const char* apiUrl, int sdCsPin = 10);

  StorageMode getMode() const { return storageMode; }

  bool begin();
  void update();

  // Cloud stuff
  bool syncWithCloud(); // what to download
  bool downloadCloudImage(const String& id, const String& signedUrl); // how to download

  // Local stuff
  size_t getPhotoCount() const;
  bool streamPhoto(size_t index, std::function<void(Stream*)> onDataReady);
  bool cleanCache();
};

#endif