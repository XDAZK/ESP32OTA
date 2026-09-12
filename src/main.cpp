#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ================= CONFIG =================
// Version được định nghĩa từ build flag hoặc giá trị mặc định
#ifndef CURRENT_FIRMWARE_VERSION
#define CURRENT_FIRMWARE_VERSION "2026.05.31.211129"
#endif

const String CURRENT_VERSION = CURRENT_FIRMWARE_VERSION;

// ================= WIFI ===================
const char* WIFI_SSID = "Dev";
const char* WIFI_PASS = "12345679";

// ================= OTA ====================
const char* VERSION_URL = "https://xdazk.github.io/ESP32OTA/latest.json";
const unsigned long OTA_CHECK_INTERVAL = 60000; // Kiểm tra OTA mỗi 60 giây

// ================= OLED ===================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Chân I2C (Mặc định bạn dùng 5 và 4, nếu chuyển sang 21 và 22 thì đổi tại đây)
#define OLED_SDA 5
#define OLED_SCL 4
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool oledAvailable = false;

void showOLED(String line1, String line2 = "", String line3 = "")
{
    if (!oledAvailable) return;

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

void updateOLEDClock(String clockStr) 
{
    if (!oledAvailable) return;

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

    Serial.printf("\n[WiFi] Connecting to %s", WIFI_SSID);
    showOLED("Connecting WiFi...", String(WIFI_SSID));
    
    WiFi.disconnect(true);
    delay(100);
    WiFi.mode(WIFI_STA);
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
        Serial.println("\n[WiFi] Connected successfully!");
        Serial.println("[WiFi] IP Address: " + WiFi.localIP().toString());
        showOLED("WiFi Connected!", "IP: " + WiFi.localIP().toString());
        delay(1000);
        return true;
    }
    else
    {
        Serial.println("\n[WiFi] Connection Failed (Timeout)!");
        showOLED("WiFi Failed!", "Will retry later...");
        delay(1000);
        return false;
    }
}

void checkOTA()
{
    if (WiFi.status() != WL_CONNECTED) return;

    Serial.println("\n=================================");
    Serial.println("[OTA] Checking for new firmware...");
    Serial.println("=================================");
    showOLED("Checking OTA...");

    WiFiClientSecure client;
    client.setInsecure(); // Bỏ qua xác thực SSL certificate của GitHub Pages

    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(10000); // 10s timeout

    if (!http.begin(client, VERSION_URL))
    {
        Serial.println("[OTA] Cannot connect to VERSION_URL");
        showOLED("OTA Error", "Connect Failed");
        return;
    }

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK)
    {
        Serial.printf("[OTA] HTTP Error fetching JSON: %d\n", httpCode);
        showOLED("OTA Error", "HTTP: " + String(httpCode));
        http.end();
        return;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
        Serial.printf("[OTA] JSON parse error: %s\n", error.c_str());
        showOLED("JSON Error", error.c_str());
        return;
    }

    String serverVersion = doc["version"].as<String>();
    String firmwareUrl = doc["firmware"].as<String>();

    Serial.println("[OTA] Current Version : " + CURRENT_VERSION);
    Serial.println("[OTA] Server Version  : " + serverVersion);
    Serial.println("[OTA] Firmware URL    : " + firmwareUrl);

    if (serverVersion.length() == 0 || serverVersion == CURRENT_VERSION)
    {
        Serial.println("[OTA] Already running the latest version.");
        showOLED("Firmware Up-to-date", "Ver: " + CURRENT_VERSION);
        return;
    }

    // Phát hiện firmware mới
    Serial.println("[OTA] Found new version! Starting OTA update process...");
    showOLED("New Version Found!", "Ver: " + serverVersion, "Downloading...");
    delay(1500);

    httpUpdate.onStart([]() {
        Serial.println("[OTA] Update process started...");
        showOLED("OTA Starting...");
    });

    httpUpdate.onProgress([](int current, int total) {
        int percent = (total > 0) ? ((current * 100) / total) : 0;
        Serial.printf("[OTA] Progress: %d%%\n", percent);

        if (oledAvailable)
        {
            display.clearDisplay();
            display.setTextSize(2);
            display.setTextColor(SSD1306_WHITE);
            display.setCursor(0, 10);
            display.print("OTA UPDATE");

            display.setCursor(0, 38);
            display.printf("%d %%", percent);
            display.display();
        }
    });

    httpUpdate.onEnd([]() {
        Serial.println("[OTA] Download & Flash Complete! Rebooting now...");
        showOLED("OTA Success!", "Rebooting...");
    });

    httpUpdate.onError([](int err) {
        Serial.printf("[OTA] Error Code: %d\n", err);
    });

    // Thực hiện nạp OTA (tự động reboot khi hoàn tất)
    t_httpUpdate_return ret = httpUpdate.update(client, firmwareUrl);

    switch (ret)
    {
        case HTTP_UPDATE_FAILED:
            Serial.printf("[OTA] UPDATE FAILED (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
            showOLED("OTA Failed!", httpUpdate.getLastErrorString());
            break;

        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("[OTA] HTTP_UPDATE_NO_UPDATES");
            showOLED("No Updates");
            break;

        case HTTP_UPDATE_OK:
            Serial.println("[OTA] Update Success!");
            break;
    }
}

void setup()
{
    // 1. Khởi động Serial ngay đầu tiên và chờ ổn định
    Serial.begin(115200);
    delay(1000); 

    Serial.println("\n\n========================================");
    Serial.println("       ESP32 OTA SYSTEM STARTING        ");
    Serial.println("========================================");
    Serial.printf("Firmware Version: %s\n", CURRENT_VERSION.c_str());
    Serial.printf("Built At        : %s %s\n", __DATE__, __TIME__);
    Serial.println("========================================");

    // 2. Khởi tạo I2C và OLED an toàn (tránh treo chip nếu OLED không có)
    Wire.begin(OLED_SDA, OLED_SCL);
    Wire.setTimeOut(1000); // Timeout 1s tránh lock bus

    // periphBegin = false để không tự ý reset chân I2C
    if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C, false, false))
    {
        oledAvailable = true;
        Serial.println("[OLED] SSD1306 Display initialized successfully.");
        showOLED("ESP32 OTA System", "Ver: " + CURRENT_VERSION, "Booting...");
    }
    else
    {
        oledAvailable = false;
        Serial.println("[OLED] WARNING: SSD1306 Display not detected (Check wiring SDA=5, SCL=4).");
        Serial.println("[OLED] System will continue running without OLED display.");
    }
    delay(1000);

    // 3. Kết nối WiFi và kiểm tra OTA ngay khi khởi động
    if (connectWifi())
    {
        checkOTA();
    }
}

void loop()
{
    static unsigned long lastCheck = 0;
    static unsigned long lastClockLog = 0;

    // 1. Cập nhật Uptime mỗi giây
    if (millis() - lastClockLog >= 1000)
    {
        lastClockLog = millis();

        unsigned long totalSeconds = millis() / 1000;
        unsigned long seconds = totalSeconds % 60;
        unsigned long minutes = (totalSeconds / 60) % 60;
        unsigned long hours = (totalSeconds / 3600);

        char clockBuffer[30];
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