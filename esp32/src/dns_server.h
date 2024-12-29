#pragma once

#include <DNSServer.h>
#include <WiFi.h>

class CaptiveDNSServer
{
public:
    CaptiveDNSServer();
    bool begin(const char *ssid, const char *password);
    void processNextRequest();

private:
    DNSServer dnsServer;
};