#pragma once

#include <WebServer.h>
#include <WiFi.h>
#include "scale_ble_service.h"
#include "rtc.h"

class CaptiveWebServer
{
public:
    CaptiveWebServer();
    void begin();
    void handleClient();
    void setScaleBLEService(ScaleBLEService *service) { bleService = service; }
    void setRTC(RTC *rtc_instance) { rtc = rtc_instance; }
    void setUserParams(const char *name, uint8_t gender, int age, int height)
    {
        userName = name;
        userGender = gender;
        userAge = age;
        userHeight = height;
    }

private:
    WebServer server;
    ScaleBLEService *bleService = nullptr;
    RTC *rtc = nullptr;
    static const char responsePortal[];
    static const uint16_t crc16tab[256];
    const char *userName;
    uint8_t userGender;
    int userAge;
    int userHeight;

    // Request handlers
    void handleRoot();
    void handleScaleRegister();
    void handleScaleValidate();
    void handleScaleUpload();
    void handleNotFound();
    void setupHandlers();
    uint32_t rtcToUnixTime();
};