#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>
#include <Preferences.h> // Thêm thư viện lưu trữ cấu hình

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ================= CONFIG =================
Preferences preferences;
String CURRENT_VERSION = "0"; // Sẽ được cập nhật từ bộ nhớ khi boot

// ================= WIFI =================
const char* WIFI_SSID = "Dev";
const char* WIFI_PASS = "12345679";

// ================= OTA ==================
const char* VERSION_URL = "https://xdazk.github.io/ESP32OTA/latest.json";

// ================= OLED =================
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

void connectWifi()
{
    showOLED("Connecting WiFi");

    WiFi.begin(WIFI_SSID, WIFI_PASS);

    int retry = 0;

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
        showOLED("Connecting WiFi", "Retry: " + String(retry++));
    }

    Serial.println();
    Serial.println("WiFi Connected");

    showOLED("WiFi Connected", WiFi.localIP().toString());
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
        showOLED("OTA Error", "HTTP: " + String(code));
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

    showOLED("Current: " + CURRENT_VERSION, "Server : " + serverVersion);
    delay(2000);

    if (serverVersion == CURRENT_VERSION)
    {
        Serial.println("Already latest");
        showOLED("No Update", "Version: " + serverVersion);
        return;
    }

    // TEST thử URL của firmware xem có tồn tại không
    HTTPClient test;
    test.begin(firmwareUrl);
    int firmwareCode = test.GET();

    Serial.printf("Firmware HTTP Code: %d\n", firmwareCode);
    Serial.printf("Firmware Size: %d bytes\n", test.getSize());
    test.end();

    if (firmwareCode != 200)
    {
        showOLED("Firmware Error", "HTTP: " + String(firmwareCode));
        return;
    }

    showOLED("New Version", serverVersion, "Updating...");
    delay(1000);

    // --- LƯU PHIÊN BẢN MỚI VÀO BỘ NHỚ TRƯỚC KHI KHỞI ĐỘNG CẬP NHẬT ---
    preferences.begin("ota_storage", false);
    preferences.putString("version", serverVersion);
    preferences.end();

    WiFiClientSecure client;
    client.setInsecure();

    httpUpdate.onStart([]()
    {
        Serial.println("OTA Start");
        showOLED("OTA Started");
    });

    httpUpdate.onProgress([](int current, int total)
    {
        int percent = (current * 100) / total;

        Serial.printf("OTA %d%%\n", percent);

        display.clearDisplay();
        display.setTextSize(2);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 10);
        display.print("OTA UPDATE");

        display.setCursor(0, 40);
        display.print(percent);
        display.print("%");
        display.display();
    });

    httpUpdate.onEnd([]()
    {
        Serial.println("OTA Done");
        showOLED("OTA Success", "Rebooting...");
    });

    t_httpUpdate_return ret = httpUpdate.update(client, firmwareUrl);

    switch (ret)
    {
        case HTTP_UPDATE_FAILED:
            Serial.printf("OTA FAIL (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
            showOLED("OTA Failed", String(httpUpdate.getLastErrorString()));
            
            // Nếu update thất bại, nạp lại version cũ vào bộ nhớ để không bị lệch thông tin
            preferences.begin("ota_storage", false);
            preferences.putString("version", CURRENT_VERSION);
            preferences.end();
            break;

        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("No Update");
            showOLED("No Update");
            break;

        case HTTP_UPDATE_OK:
            Serial.println("OTA Success"); // Thường ESP32 sẽ tự động reset tại đây
            break;
    }
}

void setup()
{
    Serial.begin(115200);

    // Khởi tạo và đọc Version đã lưu trong Flash
    preferences.begin("ota_storage", false);
    CURRENT_VERSION = preferences.getString("version", "0"); // Mặc định là "0" nếu chạy lần đầu
    preferences.end();

    Wire.begin(OLED_SDA, OLED_SCL);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        Serial.println("OLED fail");
        while (true) delay(1000);
    }

    showOLED("ESP32 OTA System", "Current Ver: " + CURRENT_VERSION, "Booting...");
    delay(1500);

    connectWifi();
    checkOTA();
}

void loop()
{
    static unsigned long lastCheck = 0;
    // Thay đổi từ 10 giây thành 15 phút (900,000 ms) để tối ưu hệ thống
    unsigned long checkInterval = 900000; 

    if (millis() - lastCheck > checkInterval)
    {
        lastCheck = millis();
        if (WiFi.status() == WL_CONNECTED) {
            checkOTA();
        } else {
            connectWifi();
        }
    }

    delay(1000);
}