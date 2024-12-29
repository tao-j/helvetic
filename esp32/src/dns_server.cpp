#include "dns_server.h"
#include <esp_log.h>

static const char *TAG = "DNS";

CaptiveDNSServer::CaptiveDNSServer() {}

bool CaptiveDNSServer::begin(const char *ssid, const char *password)
{
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);

    if (dnsServer.start(53, "*", WiFi.softAPIP()))
    {
        ESP_LOGI(TAG, "Started DNS server in captive portal-mode");
        return true;
    }
    else
    {
        ESP_LOGE(TAG, "Can't start DNS server!");
        return false;
    }
}

void CaptiveDNSServer::processNextRequest()
{
    dnsServer.processNextRequest();
}