#include "web_server.h"
#include <esp_log.h>

static const char *TAG = "PORTAL";

// CRC16-XMODEM lookup table
const uint16_t CaptiveWebServer::crc16tab[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0};

// Helper function to pack integers in little-endian format
template <typename T>
void packLE(uint8_t *dest, T value)
{
    for (size_t i = 0; i < sizeof(T); i++)
    {
        dest[i] = (value >> (i * 8)) & 0xFF;
    }
}

const char CaptiveWebServer::responsePortal[] = R"===(
<!DOCTYPE html><html><head><title>ESP32 CaptivePortal</title></head><body>
<h1>Hello World!</h1><p>This is a captive portal example page. All unknown http requests will
be redirected here.</p></body></html>
)===";

CaptiveWebServer::CaptiveWebServer() : server(80) {}

void CaptiveWebServer::begin()
{
    ESP_LOGI(TAG, "Starting web server...");
    setupHandlers();
    server.begin();
    ESP_LOGI(TAG, "Web server started successfully");
}

void CaptiveWebServer::handleClient()
{
    server.handleClient();
}

void CaptiveWebServer::setupHandlers()
{
    ESP_LOGD(TAG, "Setting up HTTP request handlers");
    server.on("/", [this]()
              { handleRoot(); });
    server.on("/scale/register", [this]()
              { handleScaleRegister(); });
    server.on("/scale/validate", [this]()
              { handleScaleValidate(); });
    server.on("/scale/upload", HTTP_POST, [this]()
              { handleScaleUpload(); });
    server.onNotFound([this]()
                      { handleNotFound(); });
}

void CaptiveWebServer::handleRoot()
{
    ESP_LOGV(TAG, "GET /");
    server.send(200, "text/plain", "Hello from esp32!");
}

void CaptiveWebServer::handleScaleRegister()
{
    ESP_LOGV(TAG, "GET /scale/register query = %s", server.uri().c_str());
    String message = "";
    for (int i = 0; i < server.args(); i++)
    {
        message += server.argName(i) + ": " + server.arg(i) + "\n";
    }
    ESP_LOGV(TAG, "Query params: %s", message.c_str());
    server.send(200, "text/plain", "");
}

void CaptiveWebServer::handleScaleValidate()
{
    ESP_LOGV(TAG, "GET /scale/validate query = %s", server.uri().c_str());
    String message = "";
    for (int i = 0; i < server.args(); i++)
    {
        message += server.argName(i) + ": " + server.arg(i) + "\n";
    }
    ESP_LOGV(TAG, "Query params: %s", message.c_str());
    server.send(200, "text/plain", "T");
}

// Helper function to convert RTC time to Unix timestamp
uint32_t CaptiveWebServer::rtcToUnixTime()
{
    if (!rtc || !rtc->isRunning())
    {
        ESP_LOGW(TAG, "RTC not available or not running, using current time");
        return time(nullptr);
    }

    uint16_t year;
    uint8_t month, day, hour, minute, second;
    rtc->getTime(year, month, day, hour, minute, second);

    // Convert to Unix timestamp
    // Code from https://en.wikipedia.org/wiki/Unix_time#Encoding_time_as_a_number
    if (month <= 2)
    {
        year -= 1;
        month += 12;
    }
    uint32_t days = 365 * year + year / 4 - year / 100 + year / 400 + (153 * month - 457) / 5 + day - 1;
    uint32_t seconds = days * 86400 + hour * 3600 + minute * 60 + second;
    seconds -= static_cast<uint32_t>(719468) * 86400; // Adjust for Unix epoch (1970-01-01)

    return seconds;
}

void CaptiveWebServer::handleScaleUpload()
{
    ESP_LOGV(TAG, "POST /scale/upload");
    ESP_LOGV(TAG, "Method: %s", (server.method() == HTTP_POST) ? "POST" : "OTHER");

    String body = server.arg("plain");
    ESP_LOGV(TAG, "Upload body length: %d", body.length());

    // Debug print body content in hex
    ESP_LOGV(TAG, "Body hex dump:");

    // Helper function to print a block of bytes in hex format
    auto printHexBlock = [&](const char *label, size_t start, size_t length)
    {
        char hex_buf[100]; // Max 32 bytes * 3 chars each (2 hex digits + space) + null
        ESP_LOGV(TAG, "%s (%d bytes):", label, length);

        for (size_t i = 0; i < length; i++)
        {
            snprintf(hex_buf + (i * 3), 4, "%02X ", (uint8_t)body[start + i]);
        }
        ESP_LOGV(TAG, "%s", hex_buf);
    };

    // Print header blocks
    printHexBlock("Header", 0, 30);
    printHexBlock("Measurement header", 30, 16);

    // Print measurement data blocks
    ESP_LOGV(TAG, "Measurement data:");
    for (size_t i = 46; i < body.length(); i += 32)
    {
        int bytes_to_print = min(32, (int)(body.length() - i));
        printHexBlock("", i, bytes_to_print);
    }

    // Parse protocol version 3 format
    if (body.length() >= 46)
    { // 30 bytes header + 16 bytes measurement header
        uint32_t proto_ver, battery_pc;
        uint8_t mac[6];
        uint8_t authcode[16];
        uint32_t fw_ver, unknown2, ts_scale, measurement_count;

        // Copy first 30 bytes into variables
        memcpy(&proto_ver, body.c_str(), 4);
        memcpy(&battery_pc, body.c_str() + 4, 4);
        memcpy(mac, body.c_str() + 8, 6);
        memcpy(authcode, body.c_str() + 14, 16);

        // Copy next 16 bytes into measurement header variables
        memcpy(&fw_ver, body.c_str() + 30, 4);
        memcpy(&unknown2, body.c_str() + 34, 4);
        memcpy(&ts_scale, body.c_str() + 38, 4);
        memcpy(&measurement_count, body.c_str() + 42, 4);

        // Log parsed data
        ESP_LOGV(TAG, "Protocol: v%d, Battery: %d%%, MAC: %02X:%02X:%02X:%02X:%02X:%02X, Auth: %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
                 proto_ver, battery_pc,
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                 authcode[0], authcode[1], authcode[2], authcode[3],
                 authcode[4], authcode[5], authcode[6], authcode[7],
                 authcode[8], authcode[9], authcode[10], authcode[11],
                 authcode[12], authcode[13], authcode[14], authcode[15]);

        ESP_LOGV(TAG, "Firmware: v%d, Unknown: %d, Timestamp: %d, Measurements: %d",
                 fw_ver, unknown2, ts_scale, measurement_count);

        // Parse measurement blocks
        const char *curr_pos = body.c_str() + 46; // Start after headers
        uint32_t id2, imp, weight, measure_ts, uid, fat1, covar, fat2;
        for (uint32_t i = 0; i < measurement_count; i++)
        {
            if (curr_pos + 32 > body.c_str() + body.length())
            {
                ESP_LOGV(TAG, "Not enough bytes to decode measurement %d!", i + 1);
                break;
            }

            memcpy(&id2, curr_pos, 4);
            memcpy(&imp, curr_pos + 4, 4);
            memcpy(&weight, curr_pos + 8, 4);
            memcpy(&measure_ts, curr_pos + 12, 4);
            memcpy(&uid, curr_pos + 16, 4);
            memcpy(&fat1, curr_pos + 20, 4);
            memcpy(&covar, curr_pos + 24, 4);
            memcpy(&fat2, curr_pos + 28, 4);

            ESP_LOGV(TAG, "Measurement %d:", i + 1);
            ESP_LOGV(TAG, "  id2 = %d / imp = %d / weight = %.3f / ts_scale = %d",
                     id2, imp, weight / 1000.0f, measure_ts);
            ESP_LOGV(TAG, "  uid = %d / fat1 = %d / covar = %d / fat2 = %d",
                     uid, fat1, covar, fat2);

            // Broadcast measurement over BLE if service is available
            if (bleService)
            {
                WeightHistoryRecord measurement = {
                    .weight = weight / 1000.0f, // Convert mg to kg (weight is in milligrams)
                    .impedance = imp,           // Impedance is already in ohms
                    .bodyFat = fat1 / 1000.0f,  // Convert to percentage (fat1 is in 0.001%)
                    .timestamp = measure_ts,    // Unix timestamp
                    .isStabilized = true        // Saved measurements are always stable
                };

                ESP_LOGI(TAG, "Broadcasting measurement - Weight: %.3f kg, Body Fat: %.3f%%, Impedance: %u Ω, Time: %u",
                         measurement.weight, measurement.bodyFat, measurement.impedance, measure_ts);

                bleService->setAndNotifyMeasurement(measurement);
            }

            curr_pos += 32;
        }

        // Generate response
        uint8_t response[104];                 // 100 bytes data + 2 bytes CRC + 2 bytes size
        memset(response, 0, sizeof(response)); // Zero out the buffer

        // Fill in the first 100 bytes according to the format
        uint32_t curr_time = rtcToUnixTime(); // Use RTC time instead of request timestamp
        packLE(response, curr_time);

        response[4] = 0x00; // units (KG, 0x02) (lbs, 0x00)
        response[5] = 0x32; // status (configured)
        response[6] = 0x01; // unknown
        // user count = 1
        packLE(response + 7, (uint32_t)1);

        // userid = 1
        packLE(response + 11, (uint32_t)0x1234);
        // 16 bytes padding (zeroed by memset)
        // 20 bytes name (zeroed by memset)
        memcpy(response + 31, userName, strlen(userName) + 1);

        // Set min/max tolerance to be current weight +/- 4kg
        uint32_t min_tol = weight - 4000; // 4kg = 4000g
        packLE(response + 51, min_tol);
        uint32_t max_tol = weight + 4000; // 4kg = 4000g
        packLE(response + 55, max_tol);

        // age (4 bytes), gender (1 byte), height (4 bytes)
        packLE(response + 59, (uint32_t)userAge);    // age is 4 bytes
        response[63] = userGender;                   // gender is 1 byte at offset 63
        packLE(response + 64, (uint32_t)userHeight); // height is 4 bytes at offset 64

        packLE(response + 68, (uint32_t)0);     // some weight
        packLE(response + 72, (uint32_t)0);     // body fat
        packLE(response + 76, (uint32_t)0);     // covariance
        packLE(response + 80, (uint32_t)0);     // some other weight
        packLE(response + 84, ts_scale - 1000); // timestamp moved to offset 84

        // Rest of the fields are 0 (already set by memset)
        // Last 3 uint32_t values, second one is 3
        packLE(response + 88, (uint32_t)0); // unknown
        packLE(response + 92, (uint32_t)3); // always 3 (now 4 bytes)
        packLE(response + 96, (uint32_t)0); // unknown

        // Calculate CRC16-XMODEM
        uint16_t crc = 0;
        for (int i = 0; i < 100; i++)
        {
            crc = (crc << 8) ^ crc16tab[((crc >> 8) ^ response[i]) & 0xFF];
        }
        packLE(response + 100, crc);

        // Set message size
        uint16_t msg_size = 0x19 + (1 * 0x4d);
        packLE(response + 102, msg_size);

        // Send response
        server.send(200, "application/octet-stream", String((const char *)response, sizeof(response)));
        return;
    }

    // If we get here, something went wrong
    server.send(400, "text/plain", "Invalid request");
}

void CaptiveWebServer::handleNotFound()
{
    ESP_LOGV(TAG, "GET %s (redirecting to portal)", server.uri().c_str());
    server.sendHeader("Location", "/portal");
    server.send(302, "text/plain", "redirect to captive portal");
}