#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ================= WIFI =================
const char* WIFI_SSID = "Dev";
const char* WIFI_PASS = "12345679";

// ================= OTA ==================
const char* VERSION_URL =
    "https://xdazk.github.io/ESP32OTA/metadata/latest.json";

String CURRENT_VERSION = "0";

// ================= OLED =================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_SDA 5
#define OLED_SCL 4
#define OLED_RESET -1

Adafruit_SSD1306 display(
    SCREEN_WIDTH,
    SCREEN_HEIGHT,
    &Wire,
    OLED_RESET
);

void showOLED(
    String line1,
    String line2 = "",
    String line3 = "")
{
    display.clearDisplay();

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.println(line1);

    display.setCursor(0, 16);
    display.println(line2);

    display.setCursor(0, 32);
    display.println(line3);

    display.display();
}

void connectWifi()
{
    showOLED("Connecting WiFi");

    WiFi.begin(WIFI_SSID, WIFI_PASS);

    int retry = 0;

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);

        Serial.print(".");

        showOLED(
            "Connecting WiFi",
            "Retry: " + String(retry++)
        );
    }

    Serial.println();
    Serial.println("WiFi Connected");

    showOLED(
        "WiFi Connected",
        WiFi.localIP().toString()
    );

    delay(1500);
}

void checkOTA()
{
    HTTPClient http;

    showOLED("Checking OTA");

    http.begin(VERSION_URL);

    int code = http.GET();

    if (code != HTTP_CODE_OK)
    {
        Serial.printf("latest.json HTTP Error: %d\n", code);

        showOLED(
            "OTA Error",
            "HTTP: " + String(code)
        );

        http.end();
        return;
    }

    String payload = http.getString();

    http.end();

    JsonDocument doc;

    if (deserializeJson(doc, payload))
    {
        showOLED("JSON Error");
        return;
    }

    String serverVersion = doc["version"].as<String>();
    String firmwareUrl = doc["firmware"].as<String>();

    Serial.println("========================");
    Serial.println("Current : " + CURRENT_VERSION);
    Serial.println("Server  : " + serverVersion);
    Serial.println("Firmware: " + firmwareUrl);
    Serial.println("========================");

    showOLED(
        "Current",
        CURRENT_VERSION,
        serverVersion
    );

    delay(2000);

    if (serverVersion == CURRENT_VERSION)
    {
        Serial.println("Already latest");

        showOLED(
            "No Update",
            serverVersion
        );

        return;
    }

    // TEST firmware URL
    HTTPClient test;

    test.begin(firmwareUrl);

    int firmwareCode = test.GET();

    Serial.printf(
        "Firmware HTTP Code: %d\n",
        firmwareCode
    );

    Serial.printf(
        "Firmware Size: %d bytes\n",
        test.getSize()
    );

    test.end();

    if (firmwareCode != 200)
    {
        showOLED(
            "Firmware Error",
            String(firmwareCode)
        );

        return;
    }

    showOLED(
        "New Version",
        serverVersion
    );

    delay(1000);

    WiFiClientSecure client;
    client.setInsecure();

    httpUpdate.onStart([]()
    {
        Serial.println("OTA Start");

        showOLED(
            "OTA Started"
        );
    });

    httpUpdate.onProgress(
        [](int current, int total)
        {
            int percent =
                (current * 100) / total;

            Serial.printf(
                "OTA %d%%\n",
                percent
            );

            display.clearDisplay();

            display.setTextSize(2);
            display.setCursor(0, 10);
            display.print("OTA");

            display.setCursor(0, 35);
            display.print(percent);
            display.print("%");

            display.display();
        });

    httpUpdate.onEnd([]()
    {
        Serial.println("OTA Done");

        showOLED(
            "OTA Success",
            "Rebooting..."
        );
    });

    t_httpUpdate_return ret =
        httpUpdate.update(
            client,
            firmwareUrl
        );

    switch (ret)
    {
        case HTTP_UPDATE_FAILED:

            Serial.printf(
                "OTA FAIL (%d): %s\n",
                httpUpdate.getLastError(),
                httpUpdate.getLastErrorString().c_str()
            );

            showOLED(
                "OTA Failed",
                String(httpUpdate.getLastError())
            );

            break;

        case HTTP_UPDATE_NO_UPDATES:

            Serial.println("No Update");

            showOLED(
                "No Update"
            );

            break;

        case HTTP_UPDATE_OK:

            Serial.println("OTA Success");

            break;
    }
}
void setup()
{
    Serial.begin(115200);

    Wire.begin(
        OLED_SDA,
        OLED_SCL
    );

    if (!display.begin(
            SSD1306_SWITCHCAPVCC,
            0x3C))
    {
        Serial.println("OLED fail");

        while (true)
            delay(1000);
    }

    showOLED(
        "ESP32 OTA",
        "Booting..."
    );

    connectWifi();

    checkOTA();
}

void loop()
{
    static unsigned long lastCheck = 0;

    if (millis() - lastCheck > 10000)
    {
        lastCheck = millis();

        checkOTA();
    }

    delay(1000);
}