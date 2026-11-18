#ifndef WIFI_SETUP_H
#define WIFI_SETUP_H
#include <Arduino.h>
#include "DisplayManager.h"

class WifiSetup {
    public:
        explicit WifiSetup(DisplayManager& display);

        // will return true if the frame has wifi
        bool run();
    
    private:
        DisplayManager& display;

        bool connectToNetwork(const String& ssid, const String& password);
        bool startSetupPage();
};

#endif