#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <SD.h>
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <driver/i2c.h>

class CustomLGFX : public lgfx::LGFX_Device {
public:
    lgfx::Bus_RGB busInstance;
    lgfx::Panel_RGB panelInstance;
    lgfx::Touch_GT911 touchInstance;
    CustomLGFX();
};

enum class WifiChoice {
    SavedNetwork,
    NewNetwork,
    Offline
};

/*
    Controls the screen
*/
class DisplayManager {
private:
    CustomLGFX lcd;
    uint8_t backlightPin;

public:
    DisplayManager(uint8_t backlightPin);

    void begin(uint8_t rotation = 1);
    void setBacklight(bool enable);
    void clear(uint32_t color = TFT_BLACK);
    void showMessage(const char* text, uint16_t color = TFT_WHITE);
    void showWifiSetup(const char* network, const char* password, const char* address);
    
    bool renderCenteredBMP(fs::FS &fs, const char *filename);
    
    bool drawBmpStream(Stream* stream) {
        return lcd.drawBmp(stream, 0, 0);
    }
    
    int32_t getWidth() const { return lcd.width(); }
    int32_t getHeight() const { return lcd.height(); }

    WifiChoice showWifiMenu(bool hasSavedCredentials);
};

#endif
