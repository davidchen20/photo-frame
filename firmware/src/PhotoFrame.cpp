#include "PhotoFrame.h"
#include "WifiSetup.h"
#include <Preferences.h>
#include <limits.h>

PhotoFrame::PhotoFrame(DisplayManager &disp, StorageManager &stor, unsigned long interval, bool isShuffle)
  : display(disp), storage(stor), intervalMs(interval), shuffle(isShuffle), lastChangeTime(0), currentIndex(0), emptyMessageShown(false) {}

bool PhotoFrame::begin() {
    display.begin(1);

    Preferences preferences;
    if (preferences.begin("slideshow", true)) {
        unsigned long saved = preferences.getULong("interval", intervalMs);
        if (saved >= 5000UL && saved <= 3600000UL) intervalMs = saved;
        preferences.end();
    }

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
  handleTouch();
  if (intervalMenuOpen) return;
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

void PhotoFrame::handleTouch() {
  uint16_t x, y;
  bool touching = display.getTouch(x, y);
  bool pressed = touching && !touchWasDown;
  touchWasDown = touching;
  if (!pressed) return;

  if (!intervalMenuOpen) {
    intervalMenuOpen = true;
    display.showIntervalMenu(intervalMs);
    return;
  }

  unsigned long choice = display.intervalMenuChoice(x, y);
  if (choice == 0) return;
  if (choice != ULONG_MAX && choice != intervalMs) {
    Preferences preferences;
    bool saved = false;
    if (preferences.begin("slideshow", false)) {
      saved = preferences.putULong("interval", choice) == sizeof(uint32_t);
      preferences.end();
    }
    if (saved) {
      intervalMs = choice;
    } else {
      Serial.println("[Settings] Could not save interval.");
      display.showMessage("Could not save interval", TFT_RED);
      delay(1500);
    }
  }

  intervalMenuOpen = false;
  restoreDisplay();
}

void PhotoFrame::restoreDisplay() {
  size_t count = storage.getPhotoCount();
  if (count == 0) {
    display.showMessage("No photos on frame");
    emptyMessageShown = true;
  } else {
    if (currentIndex >= count) currentIndex = 0;
    display.clear();
    storage.streamPhoto(currentIndex, [this](Stream* stream) {
      display.drawBmpStream(stream);
    });
    emptyMessageShown = false;
  }
  lastChangeTime = millis();
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
