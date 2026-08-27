#include "env.h"
#include "StorageManager.h"
#include <memory>

StorageManager::StorageManager(const char* apiUrl, int sdCsPin)
  : apiUrl(apiUrl), sdCsPin(sdCsPin), storageMode(StorageMode::NONE), lastCloudSync(0) {}

bool StorageManager::begin(const char* ssid, const char* password) {
    Serial.println("\n--- [StorageManager Initializing] ---");

    // 1. Initialize SD card
    Serial.println("[SD] Initializing SD card...");

    SPI.begin(
        12,  // SCK
        13,  // MISO
        11,  // MOSI
        sdCsPin
    );

    if (!SD.begin(sdCsPin)) {
        Serial.println("[SD] Failed to mount SD Card.");

        storageMode = StorageMode::NONE;
        return false;
    }

    Serial.println("[SD] SD Card mounted successfully.");

    storageMode = StorageMode::SD_CARD;

    if (scanSdCard()) {
        Serial.printf(
            "[SD] Found %d cached photo(s).\n",
            (int)localPhotos.size()
        );
    } else {
        Serial.println("[SD] No cached photos found.");
    }
    
    // 2. Try connecting to WiFi
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(100);

    Serial.printf(
        "[Wi-Fi] Connecting to SSID: %s\n",
        ssid
    );

    WiFi.begin(ssid, password);

    unsigned long startAttempt = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 12000) {
        delay(500);
        Serial.print(".");
    }

    Serial.println();


    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf(
            "[Wi-Fi] Connected! IP: %s (Signal: %d dBm)\n",
            WiFi.localIP().toString().c_str(),
            WiFi.RSSI()
        );
        
        Serial.print("MAC Address: ");
        Serial.println(WiFi.macAddress());
        Serial.printf("[WiFi] Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
        Serial.printf("[WiFi] DNS: %s\n", WiFi.dnsIP().toString().c_str());

        IPAddress ntpIP;
        if (WiFi.hostByName("pool.ntp.org", ntpIP)) {
            Serial.printf("[DNS] pool.ntp.org -> %s\n", ntpIP.toString().c_str());
        } else {
            Serial.println("[DNS] FAILED");
        }

        // immediately try to get a new photo
        syncWithCloud();

    } else {
        Serial.printf(
            "[Wi-Fi] Failed to connect. Status Code: %d\n",
            WiFi.status()
        );

        Serial.println(
            "[Wi-Fi] Continuing in offline mode."
        );
    }

    return storageMode == StorageMode::SD_CARD;
}

bool StorageManager::scanSdCard() {
    localPhotos.clear();

    File root = SD.open("/");
    if (!root || !root.isDirectory()) {
        Serial.println("[SD] Failed to open SD root directory!");
        return false;
    }

    File file = root.openNextFile();
    while (file) {
      if (!file.isDirectory()) {
          String fileName = String(file.name());

          if (fileName.endsWith(".bmp") || fileName.endsWith(".BMP")) {
              // remove leading slash
              if (fileName.startsWith("/")) {
                  fileName.remove(0, 1);
              }

              // remove .bmp
              int extensionIndex = fileName.lastIndexOf('.');

              if (extensionIndex > 0) {
                  String id = fileName.substring(0, extensionIndex);
                  localPhotos.push_back(id);
              }
          }
      }

        file = root.openNextFile();
    }

    root.close();

    Serial.printf("[SD] Found %d cached BMP files.\n", (int)localPhotos.size());

    return !localPhotos.empty();
}

// potentially rewrite this function later to use a hashmap since we are working with up to 60 gb of ~2mb photos
bool StorageManager::isCached(const String& id) const {
  for (const auto& localPhoto : localPhotos) {
    if (localPhoto == id) return true;
  }

  return false;
}

bool StorageManager::syncWithCloud() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Cloud] Skipped: WiFi not connected.");
    return false;
  }

  Serial.println("[Cloud] Checking for new photos...");

  auto client = std::make_unique<WiFiClientSecure>();
  auto http = std::make_unique<HTTPClient>();

  Serial.printf("[Memory] Free heap: %u\n", ESP.getFreeHeap());
  Serial.printf("[Memory] Largest block: %u\n", ESP.getMaxAllocHeap());

  // change later
  client->setInsecure();

  if (!http->begin(*client, apiUrl)) {
    Serial.println("[Cloud] Failed to initialize HTTP connection.");
    return false;
  }

  http->addHeader(
    "Authorization",
    String("Bearer ") + DEVICE_SECRET
  );

  http->setTimeout(10000);

  int httpCode = http->GET();
  Serial.printf("[Cloud] HTTP Code: %d\n", httpCode);

  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("[Cloud] GET failed: %s\n", http->errorToString(httpCode).c_str());

    http->end();
    return false;
  }

  String payload = http->getString();
  http->end();

  Serial.printf("[Memory] Free heap after HTTP: %u\n", ESP.getFreeHeap());

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);

  if (err) {
    Serial.printf("[Cloud] JSON error: %s\n", err.c_str());
    return false;
  }

  JsonArray images = doc["images"].as<JsonArray>();

  if (images.isNull()) {
    Serial.println("[Cloud] No images array returned.");
    return false;
  }

  // find and download one new image
  for (JsonObject image : images) {
    String id = image["id"].as<String>();
    String url = image["url"].as<String>();

    Serial.printf("[Cloud] Image ID: %s\n", id.c_str());

    if (id.isEmpty() || url.isEmpty()) {
        Serial.println("[Cloud] Missing ID or URL");
        continue;
    }

    if (isCached(id)) {
        Serial.printf("[Cache] Already cached: %s\n", id.c_str());
        continue;
    }

    Serial.printf("[Cache] NEW IMAGE: %s\n", id.c_str());

    return downloadCloudImage(id, url);
  }

  return true;
}

bool StorageManager::downloadCloudImage(const String& id, const String& signedUrl) {
  auto client = std::make_unique<WiFiClientSecure>();
  auto http = std::make_unique<HTTPClient>();

  // temp
  client->setInsecure();

  if (!http->begin(*client, signedUrl)) {
    Serial.println("[Cloud] Failed to open signed URL.");
    return false;
  }

  http->setTimeout(15000);

  int httpCode = http->GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("[Cloud] Image download failed: %d\n", httpCode);

    http->end();
    return false;
  }

  int contentLength = http->getSize();

  Serial.printf("[Cloud] Image size: %d bytes\n", contentLength);

  String path = "/" + id + ".bmp";
  File file = SD.open(
      path.c_str(),
      FILE_WRITE
  );

  if (!file) {
    Serial.printf(
        "[SD] Failed to create %s\n",
        path.c_str()
    );

    http->end();
    return false;
  }

  WiFiClient* stream = http->getStreamPtr();

  uint8_t buffer[4096];

  size_t totalWritten = 0;

  while (http->connected() && (contentLength > 0 || contentLength == -1)) {
    size_t available = stream->available();

    if (available) {
      size_t bytesToRead =
          min(available, sizeof(buffer));

      int bytesRead = stream->readBytes(buffer, bytesToRead);

      if (bytesRead > 0) {
        file.write(buffer, bytesRead);

        totalWritten += bytesRead;

        if (contentLength > 0) {
            contentLength -= bytesRead;
        }
      }
    }

    delay(1);
  }

  file.close();
  http->end();

  Serial.printf("[Cloud] Downloaded %u bytes\n", (unsigned)totalWritten);

  // TODO: validate download before adding to cache.

  if (totalWritten == 0) {
    Serial.println("[Cloud] Download produced no data.");

    SD.remove(path.c_str());

    return false;
  }

  localPhotos.push_back(id);

  Serial.printf("[Cloud] Cached %s\n", id.c_str());

  return true;
}

size_t StorageManager::getPhotoCount() const {
  return localPhotos.size();
}

bool StorageManager::streamPhoto(size_t index, std::function<void(Stream*)> onDataReady) {
  if (index >= localPhotos.size() ||!onDataReady) return false;

  String id = localPhotos[index];

  String path = "/" + id + ".bmp";

  File bmpFile = SD.open(path.c_str(), FILE_READ);

  if (!bmpFile) {
    Serial.printf("[SD] Failed to open %s\n", path.c_str());

    return false;
  }

  onDataReady(&bmpFile);

  bmpFile.close();

  return true;
}

void StorageManager::update() {
  if (WiFi.status() == WL_CONNECTED) {
    if (millis() - lastCloudSync >= 10000) {

      syncWithCloud();

      lastCloudSync = millis();
    }
  }
}