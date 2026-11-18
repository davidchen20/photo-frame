#include "DisplayManager.h"
#include <limits.h>

namespace {
struct IntervalOption {
  const char* label;
  unsigned long milliseconds;
};

constexpr IntervalOption intervalOptions[] = {
  {"5 seconds", 5000UL}, {"15 seconds", 15000UL},
  {"30 seconds", 30000UL}, {"1 minute", 60000UL},
  {"5 minutes", 300000UL}, {"15 minutes", 900000UL},
  {"1 hour", 3600000UL}
};
constexpr int intervalOptionCount = sizeof(intervalOptions) / sizeof(intervalOptions[0]);
constexpr int menuMargin = 20;
constexpr int menuGap = 10;
constexpr int menuTop = 80;
constexpr int buttonHeight = 60;
constexpr int rowSpacing = 70;
}

CustomLGFX::CustomLGFX() {
    {
      auto cfg = busInstance.config();
      cfg.panel = &panelInstance;
      
      cfg.pin_d0  = GPIO_NUM_15; // B0
      cfg.pin_d1  = GPIO_NUM_7;  // B1
      cfg.pin_d2  = GPIO_NUM_6;  // B2
      cfg.pin_d3  = GPIO_NUM_5;  // B3
      cfg.pin_d4  = GPIO_NUM_4;  // B4
      
      cfg.pin_d5  = GPIO_NUM_9;  // G0
      cfg.pin_d6  = GPIO_NUM_46; // G1
      cfg.pin_d7  = GPIO_NUM_3;  // G2
      cfg.pin_d8  = GPIO_NUM_8;  // G3
      cfg.pin_d9  = GPIO_NUM_16; // G4
      cfg.pin_d10 = GPIO_NUM_1;  // G5
      
      cfg.pin_d11 = GPIO_NUM_14; // R0
      cfg.pin_d12 = GPIO_NUM_21; // R1
      cfg.pin_d13 = GPIO_NUM_47; // R2
      cfg.pin_d14 = GPIO_NUM_48; // R3
      cfg.pin_d15 = GPIO_NUM_45; // R4

      cfg.pin_henable = GPIO_NUM_41;
      cfg.pin_vsync   = GPIO_NUM_40;
      cfg.pin_hsync   = GPIO_NUM_39;
      cfg.pin_pclk    = GPIO_NUM_0;
      cfg.freq_write  = 15000000;

      cfg.hsync_polarity    = 0;
      cfg.hsync_front_porch = 40;
      cfg.hsync_pulse_width = 48;
      cfg.hsync_back_porch  = 40;
      
      cfg.vsync_polarity    = 0;
      cfg.vsync_front_porch = 1;
      cfg.vsync_pulse_width = 31;
      cfg.vsync_back_porch  = 13;

      cfg.pclk_active_neg   = 1;
      cfg.de_idle_high      = 0;
      cfg.pclk_idle_high    = 0;

      busInstance.config(cfg);
    }
    {
      auto cfg = panelInstance.config();
      cfg.memory_width  = 800;
      cfg.memory_height = 480;
      cfg.panel_width   = 800;
      cfg.panel_height  = 480;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      panelInstance.config(cfg);
    }
    panelInstance.setBus(&busInstance);

    {
      auto cfg = touchInstance.config();

      cfg.x_min = 0;
      cfg.x_max = 799;
      cfg.y_min = 0;
      cfg.y_max = 479;

      cfg.pin_int = -1;
      cfg.pin_rst = -1;
      cfg.bus_shared = false;
      cfg.offset_rotation = 0;

      cfg.i2c_port = I2C_NUM_1;
      cfg.pin_sda = GPIO_NUM_19;
      cfg.pin_scl = GPIO_NUM_20;
      cfg.freq = 400000;
      cfg.i2c_addr = 0x14;

      touchInstance.config(cfg);
      panelInstance.setTouch(&touchInstance);
      
    }
    setPanel(&panelInstance);
}

DisplayManager::DisplayManager(uint8_t backlightPin) : backlightPin(backlightPin) {}

void DisplayManager::begin(uint8_t rotation) {
    pinMode(backlightPin, OUTPUT);
    setBacklight(true);
    lcd.begin();
    lcd.setRotation(rotation);
    clear();
    lcd.setTextSize(2);
}

void DisplayManager::setBacklight(bool enable) {
  digitalWrite(backlightPin, enable ? HIGH : LOW);
}

void DisplayManager::clear(uint32_t color) {
  lcd.fillScreen(color);
}

void DisplayManager::showMessage(const char* text, uint16_t color) {
  lcd.fillScreen(TFT_BLACK);
  lcd.setTextColor(color, TFT_BLACK);
  lcd.drawString(text, 20, 20);
}

void DisplayManager::showWifiSetup(const char* network, const char* password, const char* address) {
  lcd.fillScreen(TFT_BLACK);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  lcd.setTextSize(2);
  lcd.drawString("On your phone, connect to:", 20, 20);
  lcd.drawString(network, 20, 55);
  lcd.drawString("Password:", 20, 100);
  lcd.drawString(password, 20, 135);
  lcd.drawString("Then open in a browser:", 20, 180);
  lcd.drawString(address, 20, 215);
}

bool DisplayManager::getTouch(uint16_t& x, uint16_t& y) {
  return lcd.getTouch(&x, &y);
}

void DisplayManager::showIntervalMenu(unsigned long intervalMs) {
  lcd.fillScreen(TFT_BLACK);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  lcd.setTextSize(2);
  lcd.drawString("Slideshow interval", menuMargin, 20);
  lcd.drawString("Choose a time:", menuMargin, 48);

  const int buttonWidth = (lcd.width() - 2 * menuMargin - menuGap) / 2;
  for (int i = 0; i < intervalOptionCount + 1; ++i) {
    const int x = menuMargin + (i % 2) * (buttonWidth + menuGap);
    const int y = menuTop + (i / 2) * rowSpacing;
    const bool selected = i < intervalOptionCount && intervalOptions[i].milliseconds == intervalMs;
    lcd.fillRoundRect(x, y, buttonWidth, buttonHeight, 8, selected ? TFT_DARKGREEN : TFT_DARKGREY);
    lcd.drawRoundRect(x, y, buttonWidth, buttonHeight, 8, TFT_WHITE);
    lcd.setTextColor(TFT_WHITE, selected ? TFT_DARKGREEN : TFT_DARKGREY);
    lcd.drawString(i < intervalOptionCount ? intervalOptions[i].label : "Cancel", x + 10, y + 20);
  }
}

unsigned long DisplayManager::intervalMenuChoice(uint16_t x, uint16_t y) const {
  const int buttonWidth = (lcd.width() - 2 * menuMargin - menuGap) / 2;
  for (int i = 0; i < intervalOptionCount + 1; ++i) {
    const int left = menuMargin + (i % 2) * (buttonWidth + menuGap);
    const int top = menuTop + (i / 2) * rowSpacing;
    if (x >= left && x < left + buttonWidth && y >= top && y < top + buttonHeight) {
      return i < intervalOptionCount ? intervalOptions[i].milliseconds : ULONG_MAX;
    }
  }
  return 0;
}

bool DisplayManager::renderCenteredBMP(fs::FS &fs, const char *filename) {
  if (!fs.exists(filename)) return false;

  File file = fs.open(filename, "r");
  if (!file) return false;

  file.seek(18); // Header offset for width
  uint32_t imgWidth  = file.read() | (file.read() << 8) | (file.read() << 16) | (file.read() << 24);
  uint32_t imgHeight = file.read() | (file.read() << 8) | (file.read() << 16) | (file.read() << 24);
  file.close();

  int x = (lcd.width() - (int)imgWidth) / 2;
  int y = (lcd.height() - (int)imgHeight) / 2;
  if (x < 0) x = 0;
  if (y < 0) y = 0;

  clear();
  return lcd.drawBmp(fs, filename, x, y);
}

WifiChoice DisplayManager::showWifiMenu(bool hasSavedCredentials) {
  lcd.fillScreen(TFT_BLACK);
  lcd.setTextSize(2);
  lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  lcd.drawString("Choose Wi-Fi mode", 20, 20);

  // Use the current screen width, since the display can be rotated.
  const int buttonX = 20;
  const int buttonWidth = lcd.width() - 40;
  const int buttonHeight = 60;

  const int savedY = 80;
  const int newY = 160;
  const int offlineY = 240;

  const uint16_t enabledColor = TFT_DARKGREY;
  const uint16_t disabledColor = TFT_BLACK;

  lcd.fillRoundRect(
      buttonX, savedY, buttonWidth, buttonHeight, 8,
      hasSavedCredentials ? enabledColor : disabledColor
  );
  lcd.drawRoundRect(buttonX, savedY, buttonWidth, buttonHeight, 8, TFT_WHITE);
  lcd.drawString(
      hasSavedCredentials ? "Use saved Wi-Fi" : "No saved Wi-Fi",
      buttonX + 12, savedY + 20
  );

  lcd.fillRoundRect(buttonX, newY, buttonWidth, buttonHeight, 8, enabledColor);
  lcd.drawString("Set up new Wi-Fi", buttonX + 12, newY + 20);

  lcd.fillRoundRect(buttonX, offlineY, buttonWidth, buttonHeight, 8, enabledColor);
  lcd.drawString("Stay offline", buttonX + 12, offlineY + 20);

  auto insideButton = [&](int x, int y, int top) {
      return x >= buttonX &&
              x < buttonX + buttonWidth &&
              y >= top &&
              y < top + buttonHeight;
  };

  while (true) {
      uint16_t x;
      uint16_t y;

      if (lcd.getTouch(&x, &y)) {
          // Useful while checking touch orientation.
          Serial.printf("Touch: x=%u y=%u\n", x, y);

          if (hasSavedCredentials && insideButton(x, y, savedY)) {
              return WifiChoice::SavedNetwork;
          }

          if (insideButton(x, y, newY)) {
              return WifiChoice::NewNetwork;
          }

          if (insideButton(x, y, offlineY)) {
              return WifiChoice::Offline;
          }
      }

      // Yield to other tasks instead of continuously polling.
      vTaskDelay(pdMS_TO_TICKS(20));
  }
}

