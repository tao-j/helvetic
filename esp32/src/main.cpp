#include <Arduino.h>
#include <WiFi.h>
#include <esp_log.h>

#include "dns_server.h"
#include "web_server.h"
#include "scale_ble_service.h"
#include <LittleFS.h>
#include <M5Unified.h>

const char *ssid = "DefaultSSID";         // Your desired SSID
const char *password = "DefaultPassword"; // Your desired password (min 8 chars)
const char *deviceName = "DefaultDevice"; // Add this line to declare deviceName
const char *userName = "You";             // Default userName
uint8_t gender = 2;                       // Default gender (2=male, 0=female)
int age = 18;                             // Default age
int height = 1800;                        // Default height in mm

static const char *TAG = "MAIN"; // Tag for ESP logging

// Webserver and BLE services

CaptiveDNSServer dnsServer;
CaptiveWebServer webServer;
ScaleBLEService bleService;

void updateDisplay()
{
    static uint32_t lastUpdate = 0;
    if (millis() - lastUpdate < 1000)
        return;
    lastUpdate = millis();

    M5.Display.setCursor(0, 0);
    M5.Display.setTextColor(YELLOW, BLACK);
    M5.Display.setTextSize(2);
    M5.Display.println("HELVETIC");

    M5.Display.setTextSize(1);
    M5.Display.setTextColor(WHITE, BLACK);
    
    int batLevel = M5.Power.getBatteryLevel();
    M5.Display.printf("Battery: %d%%\n\n", batLevel);

    M5.Display.printf("SSID: %s\n", ssid);
    M5.Display.printf("IP: %s\n", WiFi.softAPIP().toString().c_str());
    M5.Display.printf("BLE: %d connected\n", bleService.getConnectedCount());
    M5.Display.printf("BLE Status: %s\n", bleService.getLastStatus());

    WeightHistoryRecord last = bleService.getLastMeasurement();
    M5.Display.setTextColor(ORANGE, BLACK);
    M5.Display.printf("Weight: %.2f kg\n", last.weight);
    M5.Display.setTextColor(WHITE, BLACK);

    auto dt = M5.Rtc.getDateTime();
    M5.Display.printf("\nTime: %04d-%02d-%02d\n      %02d:%02d:%02d\n", dt.date.year, dt.date.month, dt.date.date, dt.time.hours, dt.time.minutes, dt.time.seconds);
}

// WiFi event handler
void WiFiEventHandler(WiFiEvent_t event, WiFiEventInfo_t info)
{
    switch (event)
    {
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
        ESP_LOGI(TAG, "Client connected - MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                 info.wifi_ap_staconnected.mac[0], info.wifi_ap_staconnected.mac[1],
                 info.wifi_ap_staconnected.mac[2], info.wifi_ap_staconnected.mac[3],
                 info.wifi_ap_staconnected.mac[4], info.wifi_ap_staconnected.mac[5]);
        break;
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
        ESP_LOGI(TAG, "Client disconnected - MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                 info.wifi_ap_stadisconnected.mac[0], info.wifi_ap_stadisconnected.mac[1],
                 info.wifi_ap_stadisconnected.mac[2], info.wifi_ap_stadisconnected.mac[3],
                 info.wifi_ap_stadisconnected.mac[4], info.wifi_ap_stadisconnected.mac[5]);
        break;
    default:
        break;
    }
}

void setup()
{
    auto cfg = M5.config();
    M5.begin(cfg);

    M5.Display.setRotation(1);
    M5.Display.fillScreen(BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.println("Initializing...");

    // Set log level
    esp_log_level_set("*", ESP_LOG_INFO);         // Set all components to INFO level
    esp_log_level_set("BLE_SCALE", ESP_LOG_INFO); // Set BLE_SCALE to INFO level

    // Initialize BLE service
    bleService.begin();

    // Pass BLE service to web server
    webServer.setScaleBLEService(&bleService);

    // Manually parse key=value configuration
    File file = LittleFS.open("/config.txt", "r");
    if (!file)
    {
        ESP_LOGE(TAG, "Failed to open config file, using default configuration");
    }

    char line[128];
    while (file.available())
    {
        size_t len = file.readBytesUntil('\n', line, sizeof(line));
        line[len] = 0;
        char *key = strtok(line, "=");
        char *value = strtok(NULL, "=");
        if (key && value)
        {
            if (strcmp(key, "ssid") == 0)
            {
                ssid = strdup(value);
            }
            else if (strcmp(key, "password") == 0)
            {
                password = strdup(value);
            }
            else if (strcmp(key, "deviceName") == 0)
            {
                deviceName = strdup(value);
            }
            else if (strcmp(key, "userName") == 0)
            {
                userName = strdup(value);
            }
            else if (strcmp(key, "gender") == 0)
            {
                // Convert 'f'/'m' to 0/2
                gender = (tolower(value[0]) == 'f') ? 0 : 2;
            }
        }
    }

    file.close();

    ESP_LOGD(TAG, "SSID: %s", ssid);
    ESP_LOGD(TAG, "Password: %s", password);
    ESP_LOGD(TAG, "Device Name: %s", deviceName);

    // Register WiFi event handler before starting AP
    WiFi.onEvent(WiFiEventHandler);

    // Start WiFi Access Point
    WiFi.softAP(ssid, password);
    ESP_LOGI(TAG, "Access Point Started");
    ESP_LOGI(TAG, "AP IP address: %s", WiFi.softAPIP().toString().c_str());

    // Start DNS server
    if (!dnsServer.begin(ssid, password))
    {
        ESP_LOGE(TAG, "Failed to start DNS server!");
        return;
    }

    // Pass userName to web server
    webServer.setUserParams(userName, gender, age, height);

    // Configure and start web server
    webServer.begin();
}

void loop()
{
    M5.update();
    dnsServer.processNextRequest();
    webServer.handleClient();
    updateDisplay();
    // Get and log current RTC time every 10 seconds
    // static unsigned long lastPrint = 0;
    // if (millis() - lastPrint >= 10000)
    // {
    //     auto dt = M5.Rtc.getDateTime();
    //     ESP_LOGI(TAG, "Current time: %04d-%02d-%02d %02d:%02d:%02d",
    //              dt.date.year, dt.date.month, dt.date.date, dt.time.hours, dt.time.minutes, dt.time.seconds);
    //     lastPrint = millis();
    // }
}