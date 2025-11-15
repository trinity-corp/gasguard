/*
  GasGuard
  created at 16 september 2025
  by Andriy Tymchuk
  https://github.com/trinity-corp/gasguard/
*/

#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
#define OLED_SDA 21
#define OLED_SCL 22

#define MQ7_PIN 34
#define RL 10
#define RO_CLEAN_AIR 9.8

#define EEPROM_SIZE 512
#define SSID_ADDR 0
#define PASS_ADDR 32
#define CONFIGURED_FLAG_ADDR 64
#define API_URL_ADDR 160
#define DEVICE_ID_ADDR 128
#define API_TOKEN_ADDR 224
#define READING_TIME 256

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
WebServer server(80);
HTTPClient http;

const char* ap_ssid = "GasGuard"; 
const char* ap_password = "12345678";

String sta_ssid = "";
String sta_password = "";
String api_base_url = "https://ecomonitor-znv9.onrender.com/api";
String device_id = "";
String connectionStatus = "";
String reading_time = "15";

bool screenEnabled = true;
bool isConfigured = false;
unsigned long lastReading = 0;
unsigned long lastApiCall = 0;
unsigned long lastCommandCheck = 0;
const unsigned long apiInterval = 30000;
const unsigned long commandCheckInterval = 10000;
unsigned long readingInterval;
unsigned long lastCheck = 0;

void checkConfiguration();
void startAPMode();
void setupWebServer();
void connectToWiFi();
void checkWiFiConnection();
void clearConfiguration();
void writeStringToEEPROM(int addr, String str);
String readStringFromEEPROM(int addr);
void saveConfiguration();
float readMQ7();
void displayData(float ppm, int rawValue, float voltage);
void displayMessage(String line1, String line2 = "", String line3 = "");
void sendDataToAPI(float ppm, int rawValue, float voltage);
void handleApiCommand(String command, String payload);
String generateDeviceID();

void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);

  device_id = readStringFromEEPROM(DEVICE_ID_ADDR);
  if (device_id.length() == 0) {
    device_id = generateDeviceID();
    writeStringToEEPROM(DEVICE_ID_ADDR, device_id);
  }

  reading_time = readStringFromEEPROM(READING_TIME);
  if (reading_time.length() == 0) {
      reading_time = "15";
      writeStringToEEPROM(READING_TIME, reading_time);
  }

  int reading_minutes = reading_time.toInt();
  if (reading_minutes <= 0 || reading_minutes > 1440) {
    reading_minutes = 15;
    reading_time = "15";
    writeStringToEEPROM(READING_TIME, reading_time);
    Serial.println("Invalid reading time, set to default: 1 minute");
  }

  readingInterval = (unsigned long)reading_minutes * 60UL * 1000UL;

  Serial.println("Reading interval: " + String(reading_minutes) + " minutes = " + String(readingInterval) + " ms");

  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  displayMessage("GasGuard", "Device ID: " + device_id, "Initializing...");
  delay(1000);

  String saved_api_url = readStringFromEEPROM(API_URL_ADDR);
  if (saved_api_url.length() > 0) {
    api_base_url = saved_api_url;
  }

  checkConfiguration();
  Serial.print("isConfigured = ");
  Serial.println(isConfigured);

  if (isConfigured) {
    displayMessage("Connecting to", "saved WiFi...", sta_ssid);
    connectToWiFi();
  } else {
    displayMessage("Configuration", "Mode", "Connect to GasGuard AP");
    startAPMode();
    setupWebServer();
  }

  connectionStatus = "Unknown";

  Serial.println("GasGuard Started!");
  Serial.println("Device ID: " + device_id);
  Serial.println("Reading interval: " + String(readingInterval/1000) + " seconds");
  Serial.println("----------------------------");
}

float readMQ7() {
  int sensorValue = analogRead(MQ7_PIN);
  float voltage = sensorValue * (3.3 / 4095.0);
  float RS = ((5.0 - voltage) / voltage) * RL;
  float ratio = RS / RO_CLEAN_AIR;
  float ppm = 100 * pow(ratio, -2.95);
  return ppm;
}

void displayData(float ppm, int rawValue, float voltage) {
  if (!screenEnabled) return;
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  
  // Header
  if (WiFi.getMode() == WIFI_MODE_AP) {
    display.println("GasGuard - AP Mode");
  } else {
    display.print(device_id);
    display.print(" - ");
    display.println(connectionStatus);
  }
  display.drawLine(0, 12, 128, 12, WHITE);

  display.setCursor(0,20);
  display.setTextSize(2);
  if (ppm < 0.01) {
    display.print("<0.01");
  } else if (ppm < 1.0) {
    display.print(ppm, 3);
  } else if (ppm < 10.0) {
    display.print(ppm, 2);
  } else {
    display.print(ppm, 1);
  }
  display.setTextSize(1);
  display.println(" ppm");
  
  display.setCursor(0,40);
  display.print("Raw: ");
  display.print(rawValue);
  
  display.setCursor(80,40);
  display.print(voltage, 3);
  display.println("V");
  
  display.display();
}

void displayMessage(String line1, String line2, String line3) {
  if (!screenEnabled) return;
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.println(line1);
  display.setCursor(0,20);
  display.println(line2);
  display.setCursor(0,40);
  display.println(line3);
  display.display();
}

void loop() {
  server.handleClient();

  if (!isConfigured) {
    static unsigned long lastDisplayUpdate = 0;
    if (millis() - lastDisplayUpdate > 2000) {
      lastDisplayUpdate = millis();
      displayMessage("GasGuard AP Mode", "SSID: " + String(ap_ssid), "IP: " + WiFi.softAPIP().toString());
    }
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    connectionStatus = "Reconnecting";
    checkWiFiConnection();
    return;
  }

  connectionStatus = "Online";
  
  if (lastReading == 0 || millis() - lastReading >= readingInterval) {
    lastReading = millis();
    
    Serial.println("=== Reading sensor ===");
    
    // Read sensor values
    float ppm = readMQ7();
    int rawValue = analogRead(MQ7_PIN);
    float voltage = rawValue * (3.3 / 4095.0);

    // Display data on screen
    displayData(ppm, rawValue, voltage);
    
    // Send to API
    sendDataToAPI(ppm, rawValue, voltage);
    
    Serial.println("=== Sensor reading complete ===");
  }
}

void sendDataToAPI(float ppm, int rawValue, float voltage) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Cannot send data - WiFi not connected");
    connectionStatus = "Offline";
    return;
  }

  // FIXED: Use the correct endpoint that matches your Django URLs
  String url = api_base_url + "/sensor-readings/";
  Serial.println("Sending to: " + url);

  DynamicJsonDocument doc(256); // Increased size for safety
  doc["device_id"] = device_id;
  doc["final_value"] = ppm;
  doc["raw_value"] = rawValue;
  doc["voltage"] = voltage;

  String payload;
  serializeJson(doc, payload);
  Serial.println("Payload: " + payload);

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(10000); // Increased timeout to 10 seconds

  Serial.println("Sending POST request...");
  int httpCode = http.POST(payload);
  
  if (httpCode > 0) {
    String response = http.getString();
    Serial.println("HTTP Code: " + String(httpCode) + ", Response: " + response);
    connectionStatus = "Online";
    
    // Check if response contains commands
    if (response.length() > 2) { // Not empty response
      DynamicJsonDocument resDoc(256);
      DeserializationError error = deserializeJson(resDoc, response);
      
      if (!error && resDoc.containsKey("command")) {
        String command = resDoc["command"];
        String payload = resDoc["payload"] | "";
        Serial.println("Found command in response: " + command);
        handleApiCommand(command, payload);
      }
    }
  } else {
    Serial.println("HTTP POST failed: " + http.errorToString(httpCode));
    connectionStatus = "Offline - " + http.errorToString(httpCode);
  }

  http.end();
}
void handleApiCommand(String command, String payload) {
  Serial.println("Executing command: " + command + " | Payload: " + payload);
  
  if (command == "clear_eeprom") {
    clearConfiguration();
    displayMessage("EEPROM Cleared", "Restarting...", "");
    delay(2000);
    ESP.restart();
  } 
  else if (command == "restart" || command == "reboot") {
    displayMessage("Restarting...", "", "");
    delay(2000);
    ESP.restart();
  }
  else if (command == "enable_screen") {
    screenEnabled = true;
    display.ssd1306_command(SSD1306_DISPLAYON);
    Serial.println("Screen enabled by command");
    delay(2000);
  }
  else if (command == "disable_screen") {
    screenEnabled = false;
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    Serial.println("Screen disabled by command");
  }
  else if (command == "update_api_url") {
    api_base_url = payload;
    writeStringToEEPROM(API_URL_ADDR, api_base_url);
    displayMessage("API URL Updated", api_base_url, "");
    delay(2000);
  }
  else if (command == "change_reading_time") {
    reading_time = payload;
    writeStringToEEPROM(READING_TIME, reading_time);
    displayMessage("Reading time", "changed to " + payload + "m", "Restarting...");
    delay(2000);
    ESP.restart();
  }
  else if (command == "display_message") {
    displayMessage("Server Message", payload, "");
    delay(5000);
  }
  else if (command == "factory_reset") {
    clearConfiguration();
    displayMessage("Factory Reset", "Restarting...", "");
    delay(2000);
    ESP.restart();
  }
  else {
    Serial.println("Unknown command: " + command);
  }
}


String generateDeviceID() {
  String id = "GG-";
  id += String((uint32_t)ESP.getEfuseMac(), HEX);
  id.toUpperCase();
  return id;
}

void checkConfiguration() {
  byte configuredFlag = EEPROM.read(CONFIGURED_FLAG_ADDR);
  if (configuredFlag == 0xFF) {
    sta_ssid = readStringFromEEPROM(SSID_ADDR);
    sta_password = readStringFromEEPROM(PASS_ADDR);
    reading_time = readStringFromEEPROM(READING_TIME);
    
    if (sta_ssid.length() > 0) {
      isConfigured = true;
      Serial.println("Device is configured");
    } else {
      isConfigured = false;
    }
  } else {
    isConfigured = false;
  }
}

void startAPMode() {
  Serial.println("Starting AP mode for configuration...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);
  
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  Serial.print("AP SSID: ");
  Serial.println(ap_ssid);
}

void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    String html = R"=====(
    <!DOCTYPE html>
    <html>
    <head>
      <title>GasGuard WiFi Configuration</title>
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <style>
        body { font-family: Arial, sans-serif; margin: 40px; background-color: #f0f0f0; }
        .container { max-width: 500px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }
        h2 { color: #2c3e50; text-align: center; }
        input[type="text"], input[type="password"] { 
          width: 100%; padding: 12px; margin: 8px 0; 
          border: 1px solid #ccc; border-radius: 4px; 
          box-sizing: border-box;
        }
        input[type="submit"] { 
          width: 100%; background-color: #3498db; 
          color: white; padding: 14px; border: none; 
          border-radius: 4px; cursor: pointer; 
          font-size: 16px; margin-top: 10px;
        }
        input[type="submit"]:hover { background-color: #2980b9; }
        label { font-weight: bold; color: #34495e; }
        .device-info { background: #f8f9fa; padding: 15px; border-radius: 5px; margin-bottom: 15px; }
      </style>
    </head>
    <body>
      <div class="container">
        <h2>GasGuard WiFi Setup</h2>
        
        <div class="device-info">
          <strong>Device ID:</strong> )=====" + device_id + R"=====(<br>
        </div>
        
        <form action="/configure" method="post">
          <label for="ssid">WiFi Network Name:</label>
          <input type="text" id="ssid" name="ssid" required placeholder="Enter your WiFi name">
          
          <label for="password">WiFi Password:</label>
          <input type="password" id="password" name="password" placeholder="Enter your WiFi password">
          
          <input type="submit" value="Save & Connect">
        </form>
      </div>
    </body>
    </html>
    )=====";
    server.send(200, "text/html", html);
  });

  server.on("/configure", HTTP_POST, []() {
    if (server.hasArg("ssid")) {
      sta_ssid = server.arg("ssid");
      sta_password = server.arg("password");

      
      // Update API URL if provided
      if (server.hasArg("api_url") && server.arg("api_url").length() > 0) {
        api_base_url = server.arg("api_url");
        writeStringToEEPROM(API_URL_ADDR, api_base_url);
      }
      
      // Save to EEPROM
      saveConfiguration();
      
      // Send success response
      String html = R"=====(
      <!DOCTYPE html>
      <html>
      <head>
        <title>Configuration Saved</title>
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <style>
          body { font-family: Arial, sans-serif; margin: 40px; text-align: center; background-color: #f0f0f0; }
          .success { color: #27ae60; font-size: 24px; font-weight: bold; }
          .container { max-width: 400px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }
        </style>
      </head>
      <body>
        <div class="container">
          <div class="success">Configuration Saved!</div>
          <p>Device will restart and connect in 5 seconds...</p>
        </div>
        <script>
          setTimeout(function() {
            window.location.href = "/";
          }, 5000);
        </script>
      </body>
      </html>
      )=====";
      
      server.send(200, "text/html", html);
      delay(5000);
      ESP.restart();
    } else {
      server.send(400, "text/plain", "Error: Missing WiFi Name");
    }
  });

  server.begin();
  Serial.println("HTTP server started");
}

void saveConfiguration() {  
  writeStringToEEPROM(SSID_ADDR, sta_ssid);
  writeStringToEEPROM(PASS_ADDR, sta_password);
  EEPROM.write(CONFIGURED_FLAG_ADDR, 0xFF);
  EEPROM.commit();
  Serial.println("Configuration saved to EEPROM");
}

void connectToWiFi() {
  Serial.println("Connecting to saved WiFi...");
  Serial.println("SSID: " + sta_ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(sta_ssid.c_str(), sta_password.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    // IMPORTANT: Make sure we're marked as configured
    isConfigured = true;
    connectionStatus = "Online";
    
    displayMessage("WiFi Connected!", "IP: " + WiFi.localIP().toString(), "Reading sensor...");
    delay(2000);
    
    // Force an immediate sensor reading
    lastReading = 0; // This will trigger reading in next loop()
    
  } else {
    Serial.println("\nFailed to connect to WiFi. Starting AP mode...");
    displayMessage("WiFi Connection", "Failed!", "Starting AP mode...");
    delay(2000);
    clearConfiguration();
    startAPMode();
    setupWebServer();
    isConfigured = false;
  }
}
void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost. Reconnecting...");
    WiFi.reconnect();
    delay(2000);
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Reconnection failed.");
    }
  }
}

void clearConfiguration() {
  for (int i = 0; i < 96; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  Serial.println("Configuration cleared");
}

void writeStringToEEPROM(int addr, String str) {
  int len = str.length();
  EEPROM.write(addr, len);
  for (int i = 0; i < len; i++) {
    EEPROM.write(addr + 1 + i, str[i]);
  }
  EEPROM.commit();
}

String readStringFromEEPROM(int addr) {
  int len = EEPROM.read(addr);
  if (len <= 0 || len > 50) return "";
  
  char data[len + 1];
  for (int i = 0; i < len; i++) {
    data[i] = EEPROM.read(addr + 1 + i);
  }
  data[len] = '\0';
  return String(data);
}