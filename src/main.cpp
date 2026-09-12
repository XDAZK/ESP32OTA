#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ================= CONFIG =================
// Phiên bản hiện tại của firmware này. Khi xuất bản bản mới, chỉ cần tăng số này và build .bin
const String CURRENT_VERSION = "2026.05.31.204932"; 

// ================= WIFI ===================
const char* WIFI_SSID = "Dev";
const char* WIFI_PASS = "12345679";

// ================= OTA ====================
const char* VERSION_URL = "https://xdazk.github.io/ESP32OTA/latest.json";
const unsigned long OTA_CHECK_INTERVAL = 60000; // Kiểm tra OTA mỗi 60 giây

// ================= OLED ===================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_SDA 5
#define OLED_SCL 4
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void showOLED(String line1, String line2 = "", String line3 = "")
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

// Cập nhật riêng dòng đồng hồ Uptime ở dưới cùng màn hình (Y: 48)
void updateOLEDClock(String clockStr) 
{
    display.fillRect(0, 48, 128, 16, SSD1306_BLACK);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 48);
    display.println(clockStr);
    display.display();
}

bool connectWifi(int timeoutSeconds = 15)
{
    if (WiFi.status() == WL_CONNECTED) return true;

    showOLED("Connecting WiFi...", String(WIFI_SSID));
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < (timeoutSeconds * 2))
    {
        delay(500);
        Serial.print(".");
        retry++;
        showOLED("Connecting WiFi", "SSID: " + String(WIFI_SSID), "Retry: " + String(retry));
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\nWiFi Connected: " + WiFi.localIP().toString());
        showOLED("WiFi Connected!", "IP: " + WiFi.localIP().toString());
        delay(1000);
        return true;
    }
    else
    {
        Serial.println("\nWiFi Connection Failed (Timeout)!");
        showOLED("WiFi Failed!", "Will retry later...");
        delay(1000);
        return false;
    }
}

void checkOTA()
{
    if (WiFi.status() != WL_CONNECTED) return;

    Serial.println("\n--- Checking for OTA Update ---");
    showOLED("Checking OTA...");

    WiFiClientSecure client;
    client.setInsecure(); // Chấp nhận HTTPS GitHub Pages

    HTTPClient http;
    if (!http.begin(client, VERSION_URL))
    {
        Serial.println("Cannot connect to VERSION_URL");
        showOLED("OTA Error", "Connect Failed");
        return;
    }

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK)
    {
        Serial.printf("latest.json HTTP Error: %d\n", httpCode);
        showOLED("OTA Error", "HTTP Code: " + String(httpCode));
        http.end();
        return;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
        Serial.printf("JSON parse error: %s\n", error.c_str());
        showOLED("JSON Error", error.c_str());
        return;
    }

    String serverVersion = doc["version"].as<String>();
    String firmwareUrl = doc["firmware"].as<String>();

    Serial.println("========================");
    Serial.println("Current Ver : " + CURRENT_VERSION);
    Serial.println("Server Ver  : " + serverVersion);
    Serial.println("Firmware URL: " + firmwareUrl);
    Serial.println("========================");

    if (serverVersion == CURRENT_VERSION || serverVersion.length() == 0)
    {
        Serial.println("Device is running latest version.");
        showOLED("Firmware Latest", "Ver: " + CURRENT_VERSION);
        return;
    }

    // Phát hiện phiên bản mới
    showOLED("New Update Found!", "Ver: " + serverVersion, "Starting OTA...");
    delay(1500);

    httpUpdate.onStart([]() {
        Serial.println("OTA Update Started");
        showOLED("OTA Starting...");
    });

    httpUpdate.onProgress([](int current, int total) {
        int percent = (total > 0) ? ((current * 100) / total) : 0;
        Serial.printf("OTA Progress: %d%%\n", percent);

        display.clearDisplay();
        display.setTextSize(2);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 10);
        display.print("OTA UPDATE");

        display.setCursor(0, 38);
        display.printf("%d %%", percent);
        display.display();
    });

    httpUpdate.onEnd([]() {
        Serial.println("OTA Update Finished! Rebooting...");
        showOLED("OTA Success!", "Rebooting...");
    });

    httpUpdate.onError([](int err) {
        Serial.printf("OTA Callback Error: %d\n", err);
    });

    // Thực hiện OTA update
    t_httpUpdate_return ret = httpUpdate.update(client, firmwareUrl);

    switch (ret)
    {
        case HTTP_UPDATE_FAILED:
            Serial.printf("OTA FAIL (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
            showOLED("OTA Failed!", httpUpdate.getLastErrorString());
            break;

        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("HTTP_UPDATE_NO_UPDATES");
            showOLED("No Updates");
            break;

        case HTTP_UPDATE_OK:
            Serial.println("OTA Update OK");
            break;
    }
}

void setup()
{
    Serial.begin(115200);

    Wire.begin(OLED_SDA, OLED_SCL);
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        Serial.println("OLED allocation failed");
        while (true) delay(1000);
    }

    showOLED("ESP32 OTA System", "Ver: " + CURRENT_VERSION, "Booting...");
    delay(1500);

    if (connectWifi())
    {
        checkOTA();
    }
}

void loop()
{
    static unsigned long lastCheck = 0;
    static unsigned long lastClockLog = 0;

    // 1. Cập nhật Uptime mỗi 1 giây
    if (millis() - lastClockLog >= 1000)
    {
        lastClockLog = millis();

        unsigned long totalSeconds = millis() / 1000;
        unsigned long seconds = totalSeconds % 60;
        unsigned long minutes = (totalSeconds / 60) % 60;
        unsigned long hours = (totalSeconds / 3600);

        char clockBuffer[25];
        snprintf(clockBuffer, sizeof(clockBuffer), "Uptime: %02lu:%02lu:%02lu", hours, minutes, seconds);

        Serial.println(clockBuffer);
        updateOLEDClock(String(clockBuffer));
    }

    // 2. Kiểm tra OTA định kỳ
    if (millis() - lastCheck >= OTA_CHECK_INTERVAL)
    {
        lastCheck = millis();
        if (WiFi.status() == WL_CONNECTED)
        {
            checkOTA();
        }
        else
        {
            connectWifi();
        }
    }
}