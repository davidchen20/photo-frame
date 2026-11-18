#include "PhotoFrame.h"
#include "WifiSetup.h"

PhotoFrame::PhotoFrame(DisplayManager &disp, StorageManager &stor, unsigned long interval, bool isShuffle)
  : display(disp), storage(stor), intervalMs(interval), shuffle(isShuffle), lastChangeTime(0), currentIndex(0), emptyMessageShown(false) {}

bool PhotoFrame::begin() {
    display.begin(1);

    WifiSetup wifiSetup(display);
    bool connected = wifiSetup.run();

    Serial.println(
        connected ? "Starting online." : "Starting offline."
    );

    display.showMessage("Initializing Storage...");

    if (!storage.begin()) {
        display.showMessage("SD card initialization failed", TFT_RED);
        return false;
    }

    storage.cleanCache();

    display.showMessage("SD card ready", TFT_GREEN);
    delay(1000);

    nextImage();
    return true;
}

void PhotoFrame::update() {
  if (storage.getPhotoCount() == 0) {
    if (!emptyMessageShown) {
      display.showMessage("No photos on frame");
      emptyMessageShown = true;
    }
    return;
  }
  emptyMessageShown = false;

  if (millis() - lastChangeTime >= intervalMs) {
    nextImage();
  }
}

void PhotoFrame::nextImage() {
  size_t count = storage.getPhotoCount();
  if (count == 0) return;

  if (shuffle) {
    currentIndex = random(0, count);
  } else {
    currentIndex = (currentIndex + 1) % count;
  }

  // Stream raw bytes directly to LovyanGFX
  storage.streamPhoto(currentIndex, [this](Stream* stream) {
    display.drawBmpStream(stream);
  });

  lastChangeTime = millis();
}
