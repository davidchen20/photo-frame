#include "WifiSetup.h"

#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_system.h>

namespace {
    const char* AP_NAME = "PhotoFrame-Setup";
    struct WifiCredentials {
        char ssid[33];
        char password[64];
    };

    // A basic local page. The phone opens it through the frame's
    // access point, not through the existing photo-management website.
    const char SETUP_PAGE[] PROGMEM = R"html(
<!doctype html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Photo Frame Wi-Fi</title>
    <style>
        body {
            font-family: sans-serif;
            max-width: 30rem;
            margin: 3rem auto;
            padding: 0 1rem;
        }
        label, input, button {
            display: block;
            width: 100%;
            box-sizing: border-box;
        }
        input, button {
            margin: 0.5rem 0 1rem;
            padding: 0.75rem;
            font-size: 1rem;
        }
    </style>
</head>
<body>
    <h1>Connect your photo frame</h1>

    <form method="post" action="/connect">
        <label for="ssid">Wi-Fi name</label>
        <input id="ssid" name="ssid" maxlength="32" required>

        <label for="password">Wi-Fi password</label>
        <input id="password" name="password"
               type="password" maxlength="63">

        <button type="submit">Connect</button>
    </form>

    <p>Leave the password blank for an open network.</p>
</body>
</html>
)html";
}

WifiSetup::WifiSetup(DisplayManager& display) : display(display) {}

bool WifiSetup::connectToNetwork(const String& ssid, const String& password) {
    display.showMessage("Connecting to Wifi...");

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    const unsigned long started = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - started < 15000) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[Wifi] Connected");
        return true;
    }

    Serial.println("[Wifi] Connection failed.");
    WiFi.disconnect();
    return false;
}

bool WifiSetup::run() {
    // prevent wifi begin from auto writing credentials in since we will just save them after a successful connection
    WiFi.persistent(false);

    Preferences preferences;
    String savedSsid;
    String savedPassword;

    if (preferences.begin("wifi", true)) {
        WifiCredentials credentials = {};
        if (preferences.getBytesLength("credentials") == sizeof(credentials) &&
            preferences.getBytes("credentials", &credentials, sizeof(credentials)) == sizeof(credentials)) {
            credentials.ssid[sizeof(credentials.ssid) - 1] = '\0';
            credentials.password[sizeof(credentials.password) - 1] = '\0';
            savedSsid = credentials.ssid;
            savedPassword = credentials.password;
        } else {
            // Read credentials saved by the earlier version of this code.
            savedSsid = preferences.getString("ssid", "");
            savedPassword = preferences.getString("password", "");
        }
        preferences.end();
    }

    WifiChoice choice = display.showWifiMenu(savedSsid.length() > 0);

    switch (choice) {
        case WifiChoice::SavedNetwork:
            if (connectToNetwork(savedSsid, savedPassword)) {
                return true;
            }

            display.showMessage("Wifi failed. Going offline");
            delay(2000);
            WiFi.mode(WIFI_OFF);
            return false;
        
        case WifiChoice::NewNetwork:
            return startSetupPage();
        
        case WifiChoice::Offline:
            WiFi.mode(WIFI_OFF);
            return false;
    }

    return false;
}

bool WifiSetup::startSetupPage() {
    WiFi.mode(WIFI_AP_STA);

    // Generate a temporary eight-digit password for this setup session.
    // This is different from the password for the home Wi-Fi network.
    char setupPassword[9];
    uint32_t number = 10000000 + (esp_random() % 90000000);
    snprintf(setupPassword, sizeof(setupPassword), "%lu",
             static_cast<unsigned long>(number));

    if (!WiFi.softAP(AP_NAME, setupPassword)) {
        display.showMessage("Could not start setup Wi-Fi.");
        delay(2000);
        WiFi.mode(WIFI_OFF);
        return false;
    }

    display.showWifiSetup(AP_NAME, setupPassword, "http://192.168.4.1");

    Serial.printf("[Setup] Network: %s\n", AP_NAME);
    Serial.println("[Setup] Open http://192.168.4.1 on your phone.");

    WebServer server(80);

    String submittedSsid;
    String submittedPassword;
    bool submitted = false;
    bool connected = false;

    server.on("/", HTTP_GET, [&server]() {
        server.send_P(200, "text/html", SETUP_PAGE);
    });

    server.on("/connect", HTTP_POST, [&]() {
        if (submitted) {
            server.send(
                409,
                "text/plain",
                "A connection attempt is already in progress."
            );
            return;
        }

        String ssid = server.arg("ssid");
        String password = server.arg("password");

        // Wi-Fi limits are measured in bytes.
        if (ssid.length() == 0 || ssid.length() > 32 ||
            password.length() > 63 ||
            (password.length() > 0 && password.length() < 8)) {
            server.send(
                400,
                "text/plain",
                "Check the network name and password, then go back."
            );
            return;
        }

        submittedSsid = ssid;
        submittedPassword = password;
        submitted = true;

        server.send(
            200,
            "text/html",
            "<p>Trying to connect. Check the frame for the result.</p>"
            "<p>If it fails, reconnect to PhotoFrame-Setup and try again.</p>"
        );
    });

    server.begin();

    const unsigned long setupStarted = millis();
    const unsigned long setupTimeout = 5UL * 60UL * 1000UL;

    while (millis() - setupStarted < setupTimeout) {
        server.handleClient();

        if (submitted) {
            display.showMessage("Trying new Wi-Fi...");

            // Keep the setup AP available while trying the new network.
            WiFi.begin(submittedSsid.c_str(), submittedPassword.c_str());

            const unsigned long attemptStarted = millis();

            while (WiFi.status() != WL_CONNECTED &&
                   millis() - attemptStarted < 15000) {
                server.handleClient();
                vTaskDelay(pdMS_TO_TICKS(100));
            }

            if (WiFi.status() == WL_CONNECTED) {
                Preferences preferences;

                bool saved = false;
                if (preferences.begin("wifi", false)) {
                    WifiCredentials credentials = {};
                    submittedSsid.toCharArray(credentials.ssid, sizeof(credentials.ssid));
                    submittedPassword.toCharArray(credentials.password, sizeof(credentials.password));
                    saved = preferences.putBytes("credentials", &credentials, sizeof(credentials)) == sizeof(credentials);
                    preferences.end();
                }
                if (saved) {
                    display.showMessage("Connected! Wi-Fi saved.");
                } else {
                    // The current connection works, but won't be saved.
                    display.showMessage("Connected; save failed.");
                }

                delay(2000);
                connected = true;
                break;
            }

            WiFi.disconnect();
            submitted = false;
            submittedPassword = "";

            display.showMessage("Connection failed. Try again.");
            delay(2000);

            // Show the setup password again for another attempt.
            display.showWifiSetup(AP_NAME, setupPassword, "http://192.168.4.1");
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }

    server.stop();
    WiFi.softAPdisconnect(true);

    if (connected) {
        WiFi.mode(WIFI_STA);
        return true;
    }

    WiFi.mode(WIFI_OFF);
    return false;
}
