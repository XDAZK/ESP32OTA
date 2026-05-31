#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>
#include <Preferences.h> 

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ================= CONFIG =================
Preferences preferences;
String CURRENT_VERSION = "0"; 

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

// Biến toàn cục để lưu trạng thái hiển thị hiện tại trên OLED nhằm tránh bị đè chữ khi đang log giây
String globalLine1 = "";
String globalLine2 = "";

void showOLED(String line1, String line2 = "", String line3 = "")
{
    globalLine1 = line1;
    globalLine2 = line2;

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

// Hàm cập nhật riêng dòng đồng hồ ở dưới cùng OLED mà không xóa các dòng trên
void updateOLEDClock(String clockStr) 
{
    // Xóa riêng khu vực dòng số 3 (Y từ 48 đến 64)
    display.fillRect(0, 48, 128, 16, SSD1306_BLACK);
    
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 48);
    display.println(clockStr);
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
            
            preferences.begin("ota_storage", false);
            preferences.putString("version", CURRENT_VERSION);
            preferences.end();
            break;

        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("No Update");
            showOLED("No Update");
            break;

        case HTTP_UPDATE_OK:
            Serial.println("OTA Success");
            break;
    }
}

void setup()
{
    Serial.begin(115200);

    preferences.begin("ota_storage", false);
    CURRENT_VERSION = preferences.getString("version", "0"); 
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
    static unsigned long lastClockLog = 0;
    
    unsigned long checkInterval = 10000; // Giữ nguyên 10 giây check OTA 1 lần theo logic cũ của bạn

    // --- CHỨC NĂNG LOG ĐỒNG HỒ MỖI GIÂY (1000 ms) ---
    if (millis() - lastClockLog >= 1000)
    {
        lastClockLog = millis();
        
        // Tính toán Giờ : Phút : Giây từ uptime hệ thống
        unsigned long totalSeconds = millis() / 1000;
        unsigned long seconds = totalSeconds % 60;
        unsigned long minutes = (totalSeconds / 60) % 60;
        unsigned long hours = (totalSeconds / 3600);

        // Định dạng chuỗi thời gian hiển thị (Ví dụ: "Uptime: 01:23:45")
        char clockBuffer[20];
        sprintf(clockBuffer, "Uptime: %02lu:%02lu:%02lu", hours, minutes, seconds);

        // 1. Log ra màn hình Serial máy tính
        Serial.println(clockBuffer);

        // 2. Log ra dòng cuối cùng trên màn hình OLED
        updateOLEDClock(String(clockBuffer));
    }

    // --- KHỐI LOGIC CHECK OTA GIỮ NGUYÊN ---
    if (millis() - lastCheck > checkInterval)
    {
        lastCheck = millis();
        if (WiFi.status() == WL_CONNECTED) {
            checkOTA();
        } else {
            connectWifi();
        }
    }

    // Đã loại bỏ delay(1000) cũ ở đây để đảm bảo hàm loop quét mượt mà, đồng hồ chạy chính xác từng mi-li-giây.
}