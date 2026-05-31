#include <Arduino.h>

void setup()
{
    Serial.begin(115200);
    Serial.println("ESP32 OTA Demo");
}

void loop()
{
    Serial.println("Running...");
    delay(1000);
}