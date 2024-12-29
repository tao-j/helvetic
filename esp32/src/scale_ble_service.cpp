#include "scale_ble_service.h"
#include <esp_log.h>
#include <SPIFFS.h>

static const char *TAG = "BLE_SCALE";
const char *ScaleBLEService::LAST_MEASUREMENT_FILE = "/last_measurement.bin";

ScaleBLEService::ScaleBLEService()
{
    ESP_LOGI(TAG, "ScaleBLEService constructor called");
}

void ScaleBLEService::begin()
{
    ESP_LOGI(TAG, "Initializing ScaleBLEService");

    // Initialize SPIFFS
    if (!SPIFFS.begin(true))
    {
        ESP_LOGE(TAG, "Failed to mount SPIFFS");
        return;
    }
    ESP_LOGI(TAG, "SPIFFS mounted successfully");

    // Try to load last measurement
    ESP_LOGI(TAG, "Attempting to load last measurement");
    if (loadLastMeasurement())
    {
        ESP_LOGI(TAG, "Loaded last measurement - Weight: %.1f kg, Body Fat: %.1f%%, Impedance: %d ohms",
                 mLastMeasurement.weight, mLastMeasurement.bodyFat, mLastMeasurement.impedance);
    }
    else
    {
        ESP_LOGW(TAG, "Failed to load last measurement");
    }

    // Initialize BLE device
    NimBLEDevice::init("openScale");

    // Create the BLE Server
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(this); // Set server callbacks to handle disconnect

    // Setup all services
    setupWeightScaleService();
    setupBodyCompositionService();
    setupHm10WeightService();

    // Start advertising
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(WSS_SERVICE_UUID);
    pAdvertising->addServiceUUID(BCS_SERVICE_UUID);
    pAdvertising->addServiceUUID(HM10_SERVICE_UUID);

    // Add service data for 0x181B with loaded measurements
    setMeasurementServiceData(mLastMeasurement);
    pAdvertising->setServiceData(NimBLEUUID((uint16_t)0x181B), std::string((char *)mServiceData, 13));

    pAdvertising->setScanResponse(true);
    // Set minimum connection interval preference for better power efficiency
    // 0x12 = 18 * 1.25ms = 22.5ms minimum connection interval
    pAdvertising->setMinInterval(0x20); // Minimum advertising interval
    pAdvertising->setMaxInterval(0x40); // Maximum advertising interval
    pAdvertising->start();              // Start advertising

    ESP_LOGI(TAG, "BLE Scale Service Started with all services");
}

bool ScaleBLEService::loadLastMeasurement()
{
    ESP_LOGI(TAG, "Checking for measurement file: %s", LAST_MEASUREMENT_FILE);
    ESP_LOGI(TAG, "WeightHistoryRecord size: %d bytes", sizeof(WeightHistoryRecord));

    if (SPIFFS.exists(LAST_MEASUREMENT_FILE))
    {
        ESP_LOGI(TAG, "Found existing measurement file, reading...");
        File file = SPIFFS.open(LAST_MEASUREMENT_FILE, "rb");
        if (!file)
        {
            ESP_LOGE(TAG, "Failed to open measurement file for reading");
            return false;
        }

        size_t bytesRead = file.read((uint8_t *)&mLastMeasurement, sizeof(WeightHistoryRecord));
        file.close();

        if (bytesRead != sizeof(WeightHistoryRecord))
        {
            ESP_LOGE(TAG, "Failed to read measurement data (read %d bytes, expected %d)",
                     bytesRead, sizeof(WeightHistoryRecord));
            return false;
        }

        ESP_LOGI(TAG, "Successfully read %d bytes from measurement file", bytesRead);
        return true;
    }

    ESP_LOGI(TAG, "No saved measurement file found, creating default");

    // Initialize with default values
    mLastMeasurement = {
        .weight = 0.0f,
        .impedance = 0,
        .bodyFat = 0.0f,
        .water = 0.0f,
        .muscle = 0.0f,
        .timestamp = 0,
        .user_id = 0,
        .isStabilized = false};

    // Save the default measurement
    ESP_LOGI(TAG, "Saving default measurement to file");
    if (!saveLastMeasurement())
    {
        ESP_LOGE(TAG, "Failed to save default measurement");
        return false;
    }
    ESP_LOGI(TAG, "Default measurement saved successfully");
    return true;
}

bool ScaleBLEService::saveLastMeasurement()
{
    File file = SPIFFS.open(LAST_MEASUREMENT_FILE, "wb");
    if (!file)
    {
        ESP_LOGE(TAG, "Failed to open measurement file for writing");
        return false;
    }

    size_t bytesWritten = file.write((uint8_t *)&mLastMeasurement, sizeof(WeightHistoryRecord));
    file.close();

    if (bytesWritten != sizeof(WeightHistoryRecord))
    {
        ESP_LOGE(TAG, "Failed to write measurement data (wrote %d bytes, expected %d)",
                 bytesWritten, sizeof(WeightHistoryRecord));
        return false;
    }

    ESP_LOGI(TAG, "Saved measurement to file - Weight: %.1f kg, Body Fat: %.1f%%, Impedance: %u ohms",
             mLastMeasurement.weight, mLastMeasurement.bodyFat, mLastMeasurement.impedance);
    return true;
}

void ScaleBLEService::setupWeightScaleService()
{
    // Create Weight Scale Service (WSS)
    pWssService = pServer->createService(WSS_SERVICE_UUID);

    // Create Weight Measurement Characteristic
    pWssMeasurementCharacteristic = pWssService->createCharacteristic(
        WSS_MEASUREMENT_CHAR_UUID,
        NIMBLE_PROPERTY::READ |
            NIMBLE_PROPERTY::NOTIFY);

    pWssMeasurementCharacteristic->setCallbacks(this);

    // Start the service
    pWssService->start();
    ESP_LOGI(TAG, "Weight Scale Service (WSS) setup complete");
}

void ScaleBLEService::setupBodyCompositionService()
{
    // Create Body Composition Service (BCS)
    pBcsService = pServer->createService(BCS_SERVICE_UUID);

    // Create Body Composition Measurement Characteristic
    pBcsCharacteristic = pBcsService->createCharacteristic(
        BCS_MEASUREMENT_CHAR_UUID,
        NIMBLE_PROPERTY::READ |
            NIMBLE_PROPERTY::NOTIFY);

    pBcsCharacteristic->setCallbacks(this);

    // Start the service
    pBcsService->start();
    ESP_LOGI(TAG, "Body Composition Service (BCS) setup complete");
}

void ScaleBLEService::setupHm10WeightService()
{
    // Create HM-10 Weight Service
    pHm10Service = pServer->createService(HM10_SERVICE_UUID);

    // Create Weight Measurement Characteristic
    pHm10MeasurementCharacteristic = pHm10Service->createCharacteristic(
        HM10_MEASUREMENT_CHAR_UUID,
        NIMBLE_PROPERTY::READ |
            NIMBLE_PROPERTY::NOTIFY |
            NIMBLE_PROPERTY::WRITE);

    pHm10MeasurementCharacteristic->setCallbacks(this);

    // Start the service
    pHm10Service->start();
    ESP_LOGI(TAG, "HM-10 Weight Service setup complete");
}

// Helper function to format timestamp into byte array
void formatTimestamp(time_t rawtime, uint8_t *data)
{
    struct tm timeinfo;
    gmtime_r(&rawtime, &timeinfo);
    uint16_t year = timeinfo.tm_year + 1900;
    data[0] = year & 0xFF;
    data[1] = (year >> 8) & 0xFF;
    data[2] = timeinfo.tm_mon + 1;
    data[3] = timeinfo.tm_mday;
    data[4] = timeinfo.tm_hour;
    data[5] = timeinfo.tm_min;
    data[6] = timeinfo.tm_sec;
}

void ScaleBLEService::setAndNotifyWssMeasurement(const WeightHistoryRecord &measurement)
{
    if (pWssMeasurementCharacteristic)
    {
        uint8_t weightData[10]; // 1 byte flags + 2 bytes weight + 7 bytes timestamp
        memset(weightData, 0, sizeof(weightData));

        // Flags field (uint8_t)
        weightData[0] = 0x02; // Weight in kg, timestamp present

        // Weight in kg (uint16_t), multiply by 200 to get resolution of 0.005
        uint16_t weightValue = (uint16_t)(measurement.weight * 200);
        weightData[1] = weightValue & 0xFF;
        weightData[2] = (weightValue >> 8) & 0xFF;

        // Format timestamp
        formatTimestamp(measurement.timestamp, &weightData[3]);

        pWssMeasurementCharacteristic->setValue(weightData, sizeof(weightData));
        pWssMeasurementCharacteristic->notify();
        ESP_LOGI(TAG, "WSS measurement sent - Weight: %.2f kg", measurement.weight);
    }
}

void ScaleBLEService::setAndNotifyBcsMeasurement(const WeightHistoryRecord &measurement)
{
    if (pBcsCharacteristic)
    {
        const size_t dataSize = 19;
        uint8_t bodyCompData[dataSize];
        memset(bodyCompData, 0, dataSize);

        // Flags (uint16_t)
        uint16_t flags = 0x0021; // Timestamp and Body Fat present
        bodyCompData[0] = flags & 0xFF;
        bodyCompData[1] = (flags >> 8) & 0xFF;

        // Body Fat Percentage (resolution 0.1%)
        uint16_t bodyFatValue = (uint16_t)(measurement.bodyFat * 10); // Convert from % to 0.1%
        bodyCompData[2] = bodyFatValue & 0xFF;
        bodyCompData[3] = (bodyFatValue >> 8) & 0xFF;

        // Format timestamp
        formatTimestamp(measurement.timestamp, &bodyCompData[4]);

        pBcsCharacteristic->setValue(bodyCompData, dataSize);
        pBcsCharacteristic->notify();
        ESP_LOGI(TAG, "BCS measurement sent - Body Fat: %.1f%%", measurement.bodyFat);
    }
}

void ScaleBLEService::setAndNotifyHm10Measurement(const WeightHistoryRecord &measurement)
{
    if (pHm10MeasurementCharacteristic)
    {
        // Extract time components
        time_t rawtime = measurement.timestamp;
        struct tm timeinfo;
        gmtime_r(&rawtime, &timeinfo);

        // Calculate checksum
        int checksum = 0;
        checksum ^= measurement.user_id;
        checksum ^= timeinfo.tm_year + 1900;
        checksum ^= timeinfo.tm_mon + 1;
        checksum ^= timeinfo.tm_mday;
        checksum ^= timeinfo.tm_hour;
        checksum ^= timeinfo.tm_min;
        checksum ^= (int)(measurement.weight);
        checksum ^= (int)(measurement.bodyFat);
        checksum ^= 0; // water
        checksum ^= 0; // muscle

        // Format the string according to the protocol
        char buffer[128];
        int len = snprintf(buffer, sizeof(buffer),
                           "$D$%d,%d,%d,%d,%d,%d,%.1f,%.1f,0.0,0.0,%d\n",
                           measurement.user_id,
                           timeinfo.tm_year + 1900,
                           timeinfo.tm_mon + 1,
                           timeinfo.tm_mday,
                           timeinfo.tm_hour,
                           timeinfo.tm_min,
                           measurement.weight,
                           measurement.bodyFat,
                           checksum);

        if (len > 0 && len < (int)sizeof(buffer))
        {
            ESP_LOGD(TAG, "Sending HM-10 measurement str: %s", buffer);
            pHm10MeasurementCharacteristic->setValue((uint8_t *)buffer, len);
            pHm10MeasurementCharacteristic->notify();
            ESP_LOGI(TAG, "HM-10 measurement sent: %s", buffer);
        }
    }
}

void ScaleBLEService::setMeasurementServiceData(const WeightHistoryRecord &measurement)
{
    // byte[0] = unit (0x02 for kg, 0x03 for lbs)
    mServiceData[0] = 0x02; // Use kg as unit

    // Set flags based on impedance using ternary operator
    mServiceData[1] = (1 << 5) | ((measurement.impedance != 0) ? (1 << 1) : 0);

    // bytes[2-8] = timestamp
    formatTimestamp(measurement.timestamp, &mServiceData[2]);

    // bytes[9-10] = impedance (truncate to 16 bits for BLE advertisement)
    uint16_t impedance = (uint16_t)(measurement.impedance & 0xFFFF); // Explicitly truncate to 16 bits
    mServiceData[9] = impedance & 0xFF;
    mServiceData[10] = (impedance >> 8) & 0xFF;

    // bytes[11-12] = weight (weight * 200 for kg with 0.01/2.0 scale factor)
    uint16_t weight = (uint16_t)(measurement.weight * 200);
    mServiceData[11] = weight & 0xFF;
    mServiceData[12] = (weight >> 8) & 0xFF;

    // Log the service data for debugging
    ESP_LOGD(TAG, "Service Data: Unit: %02X, Flags: %02X, Timestamp: %02X%02X%02X%02X%02X%02X%02X, Impedance: %02X%02X, Weight: %02X%02X",
             mServiceData[0], mServiceData[1],
             mServiceData[2], mServiceData[3], mServiceData[4], mServiceData[5], mServiceData[6], mServiceData[7], mServiceData[8],
             mServiceData[9], mServiceData[10],
             mServiceData[11], mServiceData[12]);

    NimBLEDevice::getAdvertising()->setServiceData(NimBLEUUID((uint16_t)0x181B), std::string((char *)mServiceData, 13));
    // Stop and restart advertising to update service data
    NimBLEDevice::getAdvertising()->stop();
    NimBLEDevice::getAdvertising()->start();

    // // Set service data to an empty string
    // NimBLEDevice::getAdvertising()->setServiceData(NimBLEUUID((uint16_t)0x181B), "");

    // // Stop and restart advertising to update service data
    // NimBLEDevice::getAdvertising()->stop();
    // NimBLEDevice::getAdvertising()->start();
}

void ScaleBLEService::setAndNotifyMeasurement(const WeightHistoryRecord &measurement)
{
    // Store the measurement
    mLastMeasurement = measurement;
    saveLastMeasurement();

    setMeasurementServiceData(measurement);
    setAndNotifyWssMeasurement(measurement);
    setAndNotifyBcsMeasurement(measurement);
    setAndNotifyHm10Measurement(measurement);
}

void ScaleBLEService::onWrite(NimBLECharacteristic *pCharacteristic)
{
    if (pCharacteristic == pHm10MeasurementCharacteristic)
    {
        std::string value = pCharacteristic->getValue();
        ESP_LOGI(TAG, "Received write on HM-10 characteristic, length: %d", value.length());

        if (value.length() > 0)
        {
            ESP_LOGI(TAG, "Received data: ");
            for (int i = 0; i < value.length(); i++)
            {
                ESP_LOGI(TAG, "byte[%d]: 0x%02X", i, (uint8_t)value[i]);
            }
        }
    }
}

void ScaleBLEService::onDisconnect(NimBLEServer *pServer)
{
    ESP_LOGI(TAG, "Client disconnected");
    // Start advertising again to allow a new client to connect
    NimBLEDevice::getAdvertising()->start();
}