#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <esp_log.h>

class RTC
{
public:
    RTC(TwoWire &wire = Wire);
    bool begin();
    void setTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second);
    void getTime(uint16_t &year, uint8_t &month, uint8_t &day, uint8_t &hour, uint8_t &minute, uint8_t &second);
    bool isRunning();

private:
    static constexpr const char *TAG = "RTC"; // Tag for ESP logging
    TwoWire &_wire;
    bool _init;
    static const uint8_t WRITE_ADDR = 0x51; // Write address
    static const uint8_t READ_ADDR = 0x51;  // Read address

    // Essential register addresses
    static const uint8_t REG_SECONDS = 0x02;
    static const uint8_t REG_DAYS = 0x05;

    uint8_t bcd2dec(uint8_t bcd);
    uint8_t dec2bcd(uint8_t dec);
    bool read_registers(uint8_t reg, uint8_t *data, size_t length);
    void write_registers(uint8_t reg, uint8_t *data, size_t length);
};