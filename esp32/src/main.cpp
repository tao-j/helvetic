#include <Arduino.h>
#include <WiFi.h>
#include <esp_log.h>
#include <Wire.h>
#include "dns_server.h"
#include "web_server.h"
#include "scale_ble_service.h"
#include "rtc.h"
#include <SPIFFS.h>

const char *ssid = "DefaultSSID";         // Your desired SSID
const char *password = "DefaultPassword"; // Your desired password (min 8 chars)
const char *deviceName = "DefaultDevice"; // Add this line to declare deviceName
const char *userName = "You";             // Default userName
uint8_t gender = 2;                       // Default gender (2=male, 0=female)
int age = 18;                             // Default age
int height = 1800;                        // Default height in mm

static const char *TAG = "MAIN"; // Tag for ESP logging

// I2C pins for M5StickC Plus
const int I2C_SDA_PIN = 21;
const int I2C_SCL_PIN = 22;

CaptiveDNSServer dnsServer;
CaptiveWebServer webServer;
ScaleBLEService bleService;
RTC rtc(Wire);

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
    // Set log level
    esp_log_level_set("*", ESP_LOG_INFO);         // Set all components to INFO level
    esp_log_level_set("BLE_SCALE", ESP_LOG_INFO); // Set BLE_SCALE to INFO level

    Serial.begin(115200);
    delay(2000); // Give serial port time to initialize

    // Set log level to ESP_LOG_VERBOSE
    // esp_log_level_set("RTC", ESP_LOG_VERBOSE);

    // Add I2C scanner code
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    ESP_LOGD(TAG, "I2C Scanner starting...");

    byte error, address;
    int devices = 0;

    for (address = 1; address < 127; address++)
    {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();

        if (error == 0)
        {
            ESP_LOGD(TAG, "I2C device found at address 0x%02X", address);
            devices++;
        }
    }

    if (devices == 0)
    {
        ESP_LOGE(TAG, "No I2C devices found");
    }

    // Initialize RTC
    if (!rtc.begin())
    {
        ESP_LOGE(TAG, "Failed to initialize BM8563!");
    }
    else
    {
        ESP_LOGI(TAG, "BM8563 initialized successfully");
    }
    // Uncomment and set time if needed:
    // rtc.setTime(2024, 12, 28, 10, 33, 0); // Year, Month, Day, Hour, Minute, Second

    // Initialize BLE service
    bleService.begin();

    // Pass BLE service to web server
    webServer.setScaleBLEService(&bleService);
    // Pass RTC to web server
    webServer.setRTC(&rtc);

    // Manually parse key=value configuration
    File file = SPIFFS.open("/config.txt", "r");
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
    dnsServer.processNextRequest();
    webServer.handleClient();
    // Get and log current RTC time every 10 seconds
    // static unsigned long lastPrint = 0;
    // if (millis() - lastPrint >= 10000)
    // {
    //     uint16_t year;
    //     uint8_t month, day, hour, minute, second;
    //     rtc.getTime(year, month, day, hour, minute, second);
    //     ESP_LOGI(TAG, "Current time: %04d-%02d-%02d %02d:%02d:%02d",
    //              year, month, day, hour, minute, second);
    //     lastPrint = millis();
    // }
}