#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <vector>

// ================= CONFIG =================
#ifndef CURRENT_FIRMWARE_VERSION
#define CURRENT_FIRMWARE_VERSION "2026.05.31.211129"
#endif

const String CURRENT_VERSION = CURRENT_FIRMWARE_VERSION;

// ================= WIFI ===================
const char* WIFI_SSID = "Dev";
const char* WIFI_PASS = "12345679";

// ================= OTA ====================
const char* VERSION_URL = "https://xdazk.github.io/ESP32OTA/latest.json";
const unsigned long OTA_CHECK_INTERVAL = 120000; // Kiểm tra OTA mỗi 2 phút

// ================= OLED ===================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_SDA 5
#define OLED_SCL 4
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool oledAvailable = false;

// ================= POPUP GLOBAL API CONFIG =================
const char* POPUP_HOST          = "api-b.popupglobal.ai";
const char* POPUP_APP_ID        = "10000001";
const char* POPUP_DEVICE_ID     = "087A136A-5BE1-42E4-A785-0BE3B4F831FB";
const char* POPUP_FIXED_TOKEN   = "31rSScyqXLVcT/x6hpFNNu9+egxyAoIJaYyU0oK3RFMnbrYiwxhS8+YsUS4Dm4+WSLI95wDD5Wg18YBEGHCQ22WaWj13kG7bGiJbY1QoT/mkmPbKsaSbgUTNmpsV0YYnJAZ+fIJHgK0=";

// API Signatures
const char* SIGN_HOT_LIST       = "A747978795FD1A4C595A9C93AFB03CE684975C99";
const char* SIGN_RECOMMEND_LIST = "3B0133ED571B4899E2E2ED948F123ABBC88C1CB8";
const char* SIGN_JOIN           = "EACF2BD38FF71C959444155A23724536FF7A8D2F";
const char* SIGN_APPLY_PK       = "6BF40A58F9ECAC81DC6F4473864868B9DB06D1FB";
const char* SIGN_EXIT           = "90000B6D88742B8FD868EC99A3BDE3BF5879E867";

// Cấu hình vòng lặp
const unsigned long LOOP_INTERVAL_MS = 30000; // Nghỉ 30s giữa các vòng quét
const unsigned long ROOM_DELAY_MS    = 300;   // Delay 300ms giữa mỗi thao tác phòng

struct RoomInfo {
    String roomId;
    String topic;
    String classifyCode;
};

// ================= OLED HELPERS =================
void showOLED(String line1, String line2 = "", String line3 = "", String line4 = "")
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

    display.setCursor(0, 48);
    display.println(line4);

    display.display();
}

void updateOLEDStatus(String statusStr, int currentRoom = 0, int totalRooms = 0) 
{
    if (!oledAvailable) return;

    display.fillRect(0, 48, 128, 16, SSD1306_BLACK);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 48);
    if (totalRooms > 0) {
        display.printf("[%d/%d] %s", currentRoom, totalRooms, statusStr.c_str());
    } else {
        display.println(statusStr);
    }
    display.display();
}

// ================= WIFI & OTA =================
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
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\n[WiFi] Connected successfully!");
        Serial.println("[WiFi] IP Address: " + WiFi.localIP().toString());
        showOLED("WiFi Connected!", "IP: " + WiFi.localIP().toString(), "Popup Bot Ready");
        delay(1000);
        return true;
    }
    else
    {
        Serial.println("\n[WiFi] Connection Failed!");
        showOLED("WiFi Failed!", "Will retry...");
        delay(1000);
        return false;
    }
}

void checkOTA()
{
    if (WiFi.status() != WL_CONNECTED) return;

    Serial.println("\n[OTA] Checking for new firmware...");
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(10000);

    if (!http.begin(client, VERSION_URL)) return;

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK)
    {
        String payload = http.getString();
        JsonDocument doc;
        if (!deserializeJson(doc, payload))
        {
            String serverVersion = doc["version"].as<String>();
            String firmwareUrl = doc["firmware"].as<String>();

            if (serverVersion.length() > 0 && serverVersion != CURRENT_VERSION)
            {
                Serial.println("[OTA] Found new version: " + serverVersion);
                showOLED("OTA Updating...", "Ver: " + serverVersion);
                t_httpUpdate_return ret = httpUpdate.update(client, firmwareUrl);
            }
        }
    }
    http.end();
}

// ================= POPUP GLOBAL API IMPLEMENTATION =================

// 1. API Login -> Lấy dynamic token
String popupLogin(WiFiClientSecure& client)
{
    Serial.println("\n[1] Gọi API Login...");
    showOLED("Popup Global Bot", "Step 1: Login...", "Getting Token...");

    HTTPClient http;
    http.begin(client, "https://api-b.popupglobal.ai/login/info");
    http.addHeader("app-id", POPUP_APP_ID);
    http.addHeader("device-id", POPUP_DEVICE_ID);
    http.addHeader("x-auth-token", POPUP_FIXED_TOKEN);
    http.setTimeout(10000);

    int httpCode = http.POST("");
    String token = "";

    if (httpCode == HTTP_CODE_OK)
    {
        String payload = http.getString();
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload);
        if (!err) {
            token = doc["data"]["token"].as<String>();
            Serial.printf("[+] Login OK! Token: %s...\n", token.substring(0, 30).c_str());
        } else {
            Serial.printf("[-] JSON parse error: %s\n", err.c_str());
        }
    }
    else
    {
        Serial.printf("[-] Login Failed! HTTP code: %d\n", httpCode);
    }

    http.end();
    return token;
}

// 2. API Quét danh sách phòng HOT
std::vector<RoomInfo> popupGetHotRooms(WiFiClientSecure& client, const String& token)
{
    std::vector<RoomInfo> rooms;
    Serial.println("[2] Quét danh sách phòng HOT...");
    showOLED("Popup Global Bot", "Step 2: Fetching", "Hot Rooms List...");

    HTTPClient http;
    http.begin(client, "https://api-b.popupglobal.ai/chat/room/hot/list");
    http.addHeader("app-id", POPUP_APP_ID);
    http.addHeader("device-id", POPUP_DEVICE_ID);
    http.addHeader("api-sign", SIGN_HOT_LIST);
    http.addHeader("x-auth-token", token);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.setTimeout(10000);

    int httpCode = http.POST("classifyCodeId=0");
    if (httpCode == HTTP_CODE_OK)
    {
        String payload = http.getString();
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload);
        if (!err) {
            JsonArray list = doc["data"]["list"].as<JsonArray>();
            for (JsonObject r : list) {
                RoomInfo info;
                info.roomId = r["roomId"].as<String>();
                info.topic = r["topic"].as<String>();
                info.classifyCode = r["classifyDTO"]["classifyCode"].as<String>();
                if (info.roomId.length() > 0) {
                    rooms.push_back(info);
                }
            }
            Serial.printf("[+] Tải được %d phòng HOT!\n", (int)rooms.size());
        }
    }
    else
    {
        Serial.printf("[-] Lỗi lấy Hot Rooms! HTTP code: %d\n", httpCode);
    }

    http.end();
    return rooms;
}

// 3. API Tham gia phòng (Join)
bool popupJoinRoom(WiFiClientSecure& client, const String& token, const String& roomId)
{
    HTTPClient http;
    http.begin(client, "https://api-b.popupglobal.ai/chat/room/join");
    http.addHeader("app-id", POPUP_APP_ID);
    http.addHeader("device-id", POPUP_DEVICE_ID);
    http.addHeader("api-sign", SIGN_JOIN);
    http.addHeader("x-auth-token", token);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.setTimeout(8000);

    String body = "roomId=" + roomId + "&source=14";
    int httpCode = http.POST(body);
    bool success = false;

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        JsonDocument doc;
        if (!deserializeJson(doc, payload)) {
            success = (doc["code"].as<int>() == 10001) || doc["success"].as<bool>();
        }
    }

    http.end();
    return success;
}

// 4. API Mở PK (playType=1)
bool popupApplyPK(WiFiClientSecure& client, const String& token, const String& roomId)
{
    HTTPClient http;
    http.begin(client, "https://api-b.popupglobal.ai/chat/room/apply/party/model");
    http.addHeader("app-id", POPUP_APP_ID);
    http.addHeader("device-id", POPUP_DEVICE_ID);
    http.addHeader("api-sign", SIGN_APPLY_PK);
    http.addHeader("x-auth-token", token);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.setTimeout(8000);

    String body = "playType=1&roomId=" + roomId;
    int httpCode = http.POST(body);
    bool success = false;

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        JsonDocument doc;
        if (!deserializeJson(doc, payload)) {
            success = (doc["code"].as<int>() == 10001) || doc["success"].as<bool>();
        }
    }

    http.end();
    return success;
}

// 5. API Thoát phòng (Exit)
bool popupExitRoom(WiFiClientSecure& client, const String& token, const String& roomId)
{
    HTTPClient http;
    http.begin(client, "https://api-b.popupglobal.ai/chat/room/exit");
    http.addHeader("app-id", POPUP_APP_ID);
    http.addHeader("device-id", POPUP_DEVICE_ID);
    http.addHeader("api-sign", SIGN_EXIT);
    http.addHeader("x-auth-token", token);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.setTimeout(8000);

    String body = "roomId=" + roomId;
    int httpCode = http.POST(body);
    bool success = false;

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        JsonDocument doc;
        if (!deserializeJson(doc, payload)) {
            success = (doc["code"].as<int>() == 10001) || doc["success"].as<bool>();
        }
    }

    http.end();
    return success;
}

// 6. Thực hiện chuỗi Join -> Mở PK -> Thoát phòng
void processAllRoomsPipeline(WiFiClientSecure& client, const String& token, const std::vector<RoomInfo>& rooms)
{
    Serial.printf("\n===================================================\n");
    Serial.printf("  🚀 BẮT ĐẦU CHUỖI [JOIN ➔ PK ➔ THOÁT] (%d PHÒNG)\n", (int)rooms.size());
    Serial.printf("===================================================\n");

    int successCount = 0;

    for (size_t i = 0; i < rooms.size(); i++)
    {
        const RoomInfo& r = rooms[i];

        // Cập nhật OLED
        showOLED("Popup Auto PK Bot", 
                 "Room: " + r.roomId, 
                 "Topic: " + r.topic.substring(0, 16),
                 "[" + String(i + 1) + "/" + String(rooms.size()) + "] Processing...");

        // Bước 1: Join
        bool joinOk = popupJoinRoom(client, token, r.roomId);

        // Bước 2: Apply PK
        bool pkOk = popupApplyPK(client, token, r.roomId);

        // Bước 3: Exit
        bool exitOk = popupExitRoom(client, token, r.roomId);

        if (joinOk && pkOk) successCount++;

        Serial.printf("[%d/%d] Join: %s | PK: %s | Exit: %s -> %s (%s)\n",
                      (int)(i + 1), (int)rooms.size(),
                      joinOk ? "✅" : "❌",
                      pkOk ? "⚔️ OK" : "⚠️ Fail",
                      exitOk ? "🚪 OK" : "❌ Fail",
                      r.roomId.c_str(),
                      r.topic.c_str());

        delay(ROOM_DELAY_MS);
    }

    Serial.printf("\n[✔] Hoàn tất vòng: %d/%d phòng thành công!\n", successCount, (int)rooms.size());
    showOLED("Vong Lap Hoan Tat!", 
             "Thanh cong: " + String(successCount) + "/" + String(rooms.size()),
             "Nghi " + String(LOOP_INTERVAL_MS / 1000) + "s...");
}

// ================= ARDUINO SETUP & LOOP =================
void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n========================================");
    Serial.println("       ESP32 POPUP GLOBAL AUTO BOT      ");
    Serial.println("========================================");
    Serial.printf("Firmware Version: %s\n", CURRENT_VERSION.c_str());
    Serial.println("========================================");

    // Khởi tạo OLED
    Wire.begin(OLED_SDA, OLED_SCL);
    Wire.setTimeOut(1000);

    if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C, false, false))
    {
        oledAvailable = true;
        showOLED("ESP32 Popup Bot", "Ver: " + CURRENT_VERSION, "Booting...");
    }
    else
    {
        oledAvailable = false;
        Serial.println("[OLED] Warning: Display not detected. Continuing via Serial.");
    }
    delay(1000);

    // Kết nối WiFi
    connectWifi();
}

void loop()
{
    static unsigned long lastOTACheck = 0;
    static int roundCount = 1;

    // 1. Kiểm tra kết nối WiFi
    if (WiFi.status() != WL_CONNECTED)
    {
        connectWifi();
        delay(2000);
        return;
    }

    // 2. Kiểm tra OTA định kỳ
    if (millis() - lastOTACheck >= OTA_CHECK_INTERVAL)
    {
        lastOTACheck = millis();
        checkOTA();
    }

    // 3. Thực thi vòng lặp Cron vô tận của Popup Global Bot
    Serial.printf("\n>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
    Serial.printf("       [⏱️] BẮT ĐẦU VÒNG LẶP CRON #%d\n", roundCount);
    Serial.printf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");

    WiFiClientSecure client;
    client.setInsecure(); // Bỏ qua SSL cert để tăng tốc kết nối

    // Bước 1: Login lấy token mới
    String token = popupLogin(client);

    if (token.length() > 0)
    {
        // Bước 2: Quét danh sách phòng HOT
        std::vector<RoomInfo> rooms = popupGetHotRooms(client, token);

        // Bước 3: Tự động Join -> Mở PK -> Thoát phòng
        if (!rooms.empty())
        {
            processAllRoomsPipeline(client, token, rooms);
        }
        else
        {
            Serial.println("[-] Không tìm thấy phòng nào trong vòng này.");
            showOLED("Khong co phong", "Doi vong sau...");
        }
    }
    else
    {
        Serial.println("[-] Đăng nhập thất bại. Sẽ thử lại vòng sau.");
        showOLED("Login Failed", "Retrying next loop...");
    }

    Serial.printf("\n[☕] Hoàn tất Vòng #%d. Nghỉ %lu giây...\n", roundCount, LOOP_INTERVAL_MS / 1000);
    roundCount++;

    // Thời gian nghỉ giữa các vòng
    delay(LOOP_INTERVAL_MS);
}
