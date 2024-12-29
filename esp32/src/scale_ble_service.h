#pragma once

#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>
#include <vector>

// History command types
#define MI_HISTORY_CMD_START 0x01
#define MI_HISTORY_CMD_STOP 0x03
#define MI_HISTORY_CMD_REQUEST 0x02
#define MI_HISTORY_CMD_COMPLETE 0x04

// Measurement flags
#define MEASUREMENT_STABLE 0x20

struct WeightHistoryRecord
{
    float weight;
    uint32_t impedance;
    float bodyFat;
    float water;
    float muscle;
    uint32_t timestamp;
    uint8_t user_id;
    bool isStabilized;
};

class ScaleBLEService : public NimBLECharacteristicCallbacks, public NimBLEServerCallbacks
{
public:
    ScaleBLEService();
    void begin();
    void setAndNotifyMeasurement(const WeightHistoryRecord &measurement);

private:
    // Write callback
    void onWrite(NimBLECharacteristic *pCharacteristic) override;
    // Disconnect callback
    void onDisconnect(NimBLEServer *pServer) override;

    NimBLEServer *pServer = nullptr;

    // Weight Scale Service (WSS)
    NimBLEService *pWssService = nullptr;
    NimBLECharacteristic *pWssMeasurementCharacteristic = nullptr;

    // Body Composition Service (BCS)
    NimBLEService *pBcsService = nullptr;
    NimBLECharacteristic *pBcsCharacteristic = nullptr;

    // HM-10 Weight Measurement Service
    NimBLEService *pHm10Service = nullptr;
    NimBLECharacteristic *pHm10MeasurementCharacteristic = nullptr;

    // Service UUIDs
    const NimBLEUUID WSS_SERVICE_UUID = NimBLEUUID((uint16_t)0x181D);          // Weight Scale Service
    const NimBLEUUID WSS_MEASUREMENT_CHAR_UUID = NimBLEUUID((uint16_t)0x2A9D); // Weight Measurement Characteristic

    // Body Composition Service UUIDs (BCS)
    const NimBLEUUID BCS_SERVICE_UUID = NimBLEUUID((uint16_t)0x181B);          // Body Composition Service
    const NimBLEUUID BCS_MEASUREMENT_CHAR_UUID = NimBLEUUID((uint16_t)0x2A9C); // Body Composition Measurement

    // HM-10 Weight Measurement Service UUIDs
    const NimBLEUUID HM10_SERVICE_UUID = NimBLEUUID("FFE0");          // HM-10 Weight Service
    const NimBLEUUID HM10_MEASUREMENT_CHAR_UUID = NimBLEUUID("FFE1"); // HM-10 Weight Measurement

    // Service setup methods
    void setupWeightScaleService();
    void setupBodyCompositionService();
    void setupHm10WeightService();

    // Set and notify methods
    void setAndNotifyWssMeasurement(const WeightHistoryRecord &measurement);
    void setAndNotifyBcsMeasurement(const WeightHistoryRecord &measurement);
    void setAndNotifyHm10Measurement(const WeightHistoryRecord &measurement);
    void setMeasurementServiceData(const WeightHistoryRecord &measurement);

    // File operations for last measurement
    bool loadLastMeasurement();
    bool saveLastMeasurement();
    static const char *LAST_MEASUREMENT_FILE;

    // Service data for advertising
    uint8_t mServiceData[13] = {0};

    // Last measurement
    WeightHistoryRecord mLastMeasurement = {0};
};