#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <String.h>  

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
#define OLED_SDA 21
#define OLED_SCL 22

#define MQ7_PIN 34
#define RL 10
#define RO_CLEAN_AIR 9.8

extern Preferences prefs;
extern Adafruit_SSD1306 display;
extern WebServer server;

extern String sta_ssid;
extern String sta_password;
extern String device_id;
extern String connectionStatus;
extern String reading_time;
extern String ap_ssid;
extern String ap_password;
extern String api_base_url;
extern bool screenEnabled;
extern bool isConfigured;
extern unsigned long lastReading;
extern unsigned long readingInterval;
extern unsigned long previousMillis;
extern unsigned long displayReadingInterval;

// Coniguration
void checkConfiguration();
void clearConfiguration();
void saveConfiguration();

// Display
void displayMessage(String line1 = "", String line2 = "", String line3 = "", String line4 = "");
void displayData(float final_value, int rawValue, float voltage, String connectionStatus);

// Sensor
float readSensor();

// WiFi and Webserver
void setupWebServer();
void startAPMode();
void connectToWiFi();
void handleApiCommand(String command, String payload);
void sendDataToAPI(float final_value, int rawValue, float voltage);

// Device ID
String generateDeviceID();