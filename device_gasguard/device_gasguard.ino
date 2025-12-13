/*
  GasGuard firmware
  Device: ESP32
  Description: Reads gas sensor data (MQ-7), processes values, 
               and sends them to the server or cloud service.
  Author: Andriy Tymchuk
  Repository: https://github.com/trinity-corp/gasguard/

  Note: Sensor readings (CO ppm) are approximate and depend on MQ-7 limitations.
*/

#include <Wire.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include "EcoMonitor.h"

void setup() {
  Serial.begin(115200);
  prefs.begin("config", false);

  // Enable OLED screen using defined OLED_SDA and OLED_SCL adressess
  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("SSD1306 allocation failed")); 
    for(;;); // If screen does not starts device will not work
  }

  // Get the device_id
  device_id = prefs.getString("device_id");
  if (device_id.length() == 0) {
    device_id = generateDeviceID(); // If device_id empty generate a new one 
    prefs.putString("device_id", device_id); // Write new device_id to NVS
  }

  // Get the API URL
  api_base_url = prefs.getString("api_base_url", api_base_url);

  // Define reading time
  reading_time = prefs.getString("reading_time", "15");
  int reading_minutes = reading_time.toInt(); // Convert string reading_time to integer
  if (reading_minutes <= 0 || reading_minutes > 1440) {
    reading_minutes = 15;
    reading_time = "15";
    prefs.putString("reading_time", reading_time);
    Serial.println("Invalid reading time, set to default: 15 minutes");
  }
  readingInterval = (unsigned long)reading_minutes * 60UL * 1000UL;

  Serial.println("Reading interval: " + String(reading_minutes) + " minutes = " + String(readingInterval) + " ms");

  displayMessage("GasGuard", "Device ID: " + device_id, "Initializing...");
  delay(1000);

  // Checking configuration
  checkConfiguration();
  if (isConfigured) {
    displayMessage("Connecting to", "saved WiFi...", sta_ssid);
    connectToWiFi();
  } else {
    displayMessage("Configuration", "Mode", "Connect to GasGuard AP");
    startAPMode();
    setupWebServer();
  }

  // Connection status is unknown before readings sent
  connectionStatus = "Unknown";

  Serial.println("GasGuard Started!");
  Serial.println("Device ID: " + device_id);
  Serial.println("Reading interval: " + String(readingInterval/1000) + " seconds");

  // Uncomment for verbose mode
  // Serial.println("=== Verbose data ===");
  // Serial.println("device_id: " + prefs.getString("device_id"));
  // Serial.println("sta_ssid: " + prefs.getString("sta_ssid"));
  // Serial.println("sta_password: " +prefs.getString("sta_password"));
  // Serial.println("isConfigured: " + prefs.getBool("isConfigured"));
  // Serial.println("Reading interval: " + prefs.getString("reading_time"));
  // Serial.println("API URL: " + prefs.getString("api_base_url"));

}

void loop() {
  server.handleClient();
  isConfigured = prefs.getBool("isConfigured");
  
  // If device not configured (Was not setted up or factory reset)
  if (!isConfigured) {
    static unsigned long lastDisplayUpdate = 0;
    if (millis() - lastDisplayUpdate > 2000) {
      lastDisplayUpdate = millis();
      displayMessage("GasGuard AP Mode", "SSID: " + String(ap_ssid), "Password: " + String(ap_password), "URL: " + WiFi.softAPIP().toString());
    }
    return;
  }

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= displayReadingInterval) {
    previousMillis = currentMillis;

    float final_value = readSensor();
    int rawValue = analogRead(MQ7_PIN);        // Only for GasGuard
    float voltage = rawValue * (3.3 / 4095.0); // Only for GasGuard

    // Only for GasGuard
    Serial.print("Raw: "); Serial.print(rawValue);
    Serial.print(" | Voltage: "); Serial.print(voltage,2); 
    Serial.print("V | CO: "); Serial.print(final_value,1);
    Serial.println(" ppm"); //

    displayData(final_value, rawValue, voltage, connectionStatus);
  }
    
  if (lastReading == 0 || millis() - lastReading >= readingInterval) {
    lastReading = millis();

    float final_value = readSensor();
    int rawValue = analogRead(MQ7_PIN);        // Only for GasGuard
    float voltage = rawValue * (3.3 / 4095.0); // Only for GasGuard
    sendDataToAPI(final_value, rawValue, voltage);
    
    Serial.println("=== Sensor readings sent ===");
  }
}