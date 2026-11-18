#include "env.h"
#include "StorageManager.h"
#include <memory>

StorageManager::StorageManager(const char* apiUrl, int sdCsPin)
  : apiUrl(apiUrl), sdCsPin(sdCsPin), storageMode(StorageMode::NONE), lastCloudSync(0) {}

bool StorageManager::begin() {
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
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("[Wi-Fi] Connected; starting cloud sync.");
      syncWithCloud();
    } else {
      Serial.println("[Wi-Fi] Offline; using cached photos.");
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
  // client->setInsecure();
  client->setCACert(SUPABASE_ROOT_CA);

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

  // Commands remain pending on the server until a successful inventory report.
  std::vector<String> completedDeletions;
  JsonArray deletions = doc["deletions"].as<JsonArray>();
  for (JsonObject command : deletions) {
    String commandId = command["id"].as<String>();
    String id = command["photo_id"].as<String>();
    bool valid = !id.isEmpty() && !commandId.isEmpty();
    for (size_t i = 0; i < id.length(); ++i) {
      char c = id[i];
      if (c == '/' || c == '\\' || (unsigned char)c < 32 || (unsigned char)c == 127) valid = false;
    }
    if (!valid) continue;
    String path = "/" + id + ".bmp";
    String upperPath = "/" + id + ".BMP";
    bool removed = (!SD.exists(path.c_str()) || SD.remove(path.c_str())) &&
                   (!SD.exists(upperPath.c_str()) || SD.remove(upperPath.c_str()));
    if (!removed) {
      Serial.printf("[SD] Could not delete %s\n", id.c_str());
      continue;
    }
    for (auto it = localPhotos.begin(); it != localPhotos.end();) {
      if (*it == id) it = localPhotos.erase(it); else ++it;
    }
    completedDeletions.push_back(commandId);
  }

  // find and download one new image
  bool downloadOk = true;
  for (JsonObject image : images) {
    String id = image["id"].as<String>();
    String url = image["url"].as<String>();

    Serial.printf("[Cloud] Image ID: %s\n", id.c_str());

    if (id.isEmpty() || url.isEmpty()) {
        Serial.println("[Cloud] Missing ID or URL");
        continue;
    }

    bool pendingDeletion = false;
    for (JsonObject command : deletions) {
      if (command["photo_id"].as<String>() == id) { pendingDeletion = true; break; }
    }
    if (pendingDeletion) continue;

    if (isCached(id)) {
        Serial.printf("[Cache] Already cached: %s\n", id.c_str());
        continue;
    }

    Serial.printf("[Cache] NEW IMAGE: %s\n", id.c_str());

    downloadOk = downloadCloudImage(id, url);
    break;
  }

  JsonDocument report;
  JsonArray photoIds = report["photo_ids"].to<JsonArray>();
  for (const auto& id : localPhotos) photoIds.add(id);
  JsonArray completed = report["completed_deletions"].to<JsonArray>();
  for (const auto& id : completedDeletions) completed.add(id);
  String body;
  serializeJson(report, body);
  if (!http->begin(*client, apiUrl)) return false;
  http->addHeader("Authorization", String("Bearer ") + DEVICE_SECRET);
  http->addHeader("Content-Type", "application/json");
  http->setTimeout(10000);
  int reportCode = http->POST(body);
  http->end();
  if (reportCode != HTTP_CODE_OK) {
    Serial.printf("[Cloud] Inventory report failed: %d\n", reportCode);
    return false;
  }
  return downloadOk;
}

bool StorageManager::downloadCloudImage(const String& id, const String& signedUrl) {
  auto client = std::make_unique<WiFiClientSecure>();
  auto http = std::make_unique<HTTPClient>();

  // temp
  // client->setInsecure();
  client->setCACert(SUPABASE_ROOT_CA);

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

  String tempPath = "/" + id + ".tmp";
  String finalPath = "/" + id + ".bmp";
  File file = SD.open(
      tempPath.c_str(),
      FILE_WRITE
  );

  if (!file) {
    Serial.printf(
        "[SD] Failed to create %s\n",
        tempPath.c_str()
    );

    http->end();
    return false;
  }

  WiFiClient* stream = http->getStreamPtr();

  uint8_t buffer[1024];
  size_t totalWritten = 0;

  int timeoutCount = 0;

  while (totalWritten < contentLength) {

    size_t remaining = contentLength - totalWritten;
    size_t bytesToRead = min(remaining, sizeof(buffer));

    int bytesRead = stream->readBytes(buffer, bytesToRead);

    if (bytesRead == 0) {
      timeoutCount++;
      Serial.printf(
        "[Cloud] Read timeout #%d at %u / %u bytes\n",
        timeoutCount,
        (unsigned)totalWritten,
        (unsigned)contentLength
      );

      if (timeoutCount >= 10) {
        Serial.println("[Cloud] Too many read timeouts.");
        file.close();
        http->end();
        SD.remove(tempPath.c_str());
        return false;
      }

      continue;
    }

    timeoutCount = 0;

    size_t written = file.write(buffer, bytesRead);
    if (written != bytesRead) {
      Serial.println("[SD] Failed to write entire buffer.");
      file.close();
      http->end();
      SD.remove(tempPath.c_str());
      return false;
    }

    totalWritten += written;
  }

  file.close();

  if (!SD.rename(tempPath.c_str(), finalPath.c_str())) {
    Serial.println("[SD] Failed to rename temp file.");
    SD.remove(tempPath.c_str());
    return false;
  }

  http->end();

  Serial.printf(
      "[Cloud] Downloaded %u / %u bytes\n",
      (unsigned)totalWritten,
      (unsigned)contentLength
  );

  if (totalWritten != contentLength) {
      Serial.println("[Cloud] ERROR: File size mismatch.");
      SD.remove(tempPath.c_str());
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

// Get rid of any .tmp files (happens when image disconnects from WiFi while downloading)
bool StorageManager::cleanCache() {
  File root = SD.open("/");

  if (!root || !root.isDirectory()) {
    Serial.println("[SD] Failed to open root directory.");
    return false;
  }

  File file = root.openNextFile();

  while (file) {
    String path = String("/") + file.name();
    bool isFile = !file.isDirectory();

    file.close();

    if (isFile && path.endsWith(".tmp")) {
      Serial.printf("[SD] Deleting %s\n", path.c_str());
      SD.remove(path.c_str());
    }

    // if (isFile && path.endsWith(".bmp")) {
    //   Serial.printf("[SD] Deleting %s\n", path.c_str());
    //   SD.remove(path.c_str());
    // }

    file = root.openNextFile();
  }

  root.close();

  // localPhotos.clear();

  Serial.println("[SD] Cache cleared.");
  return true;
}
