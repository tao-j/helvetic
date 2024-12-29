#include "rtc.h"

RTC::RTC(TwoWire &wire) : _wire(wire)
{
    ESP_LOGI(TAG, "RTC initialized with I2C read address: 0x%02X, write address: 0x%02X", READ_ADDR, WRITE_ADDR);
}

bool RTC::begin()
{
    ESP_LOGI(TAG, "Starting RTC initialization");
    _wire.begin();

    // Initialize the RTC
    uint8_t init_buf[] = {0x00}; // Normal operation, no test mode, no alarm
    write_registers(0x00, init_buf, sizeof(init_buf));
    write_registers(0x00, init_buf, sizeof(init_buf));
    // Set control register 0x0E to 0x03
    uint8_t ctrl_buf[] = {0x03};
    write_registers(0x0E, ctrl_buf, sizeof(ctrl_buf));

    bool running = isRunning();
    if (running)
    {
        ESP_LOGI(TAG, "RTC initialization successful");
    }
    else
    {
        ESP_LOGW(TAG, "RTC initialization failed or battery is low");
    }
    return running;
}

void RTC::setTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t second)
{
    ESP_LOGI(TAG, "Setting date and time to: %04d-%02d-%02d %02d:%02d:%02d",
             year, month, day, hour, minute, second);

    // Calculate weekday (0 = Sunday, 6 = Saturday)
    int32_t y = year;
    int32_t m = month;
    if (m < 3)
    {
        y--;
        m += 12;
    }
    int32_t ydiv100 = y / 100;
    uint8_t weekday = (y + (y >> 2) - ydiv100 + (ydiv100 >> 2) + (13 * m + 8) / 5 + day) % 7;

    // Prepare buffer for time
    uint8_t time_buf[] = {
        dec2bcd(second),
        dec2bcd(minute),
        dec2bcd(hour)};

    // Prepare buffer for date
    uint8_t date_buf[] = {
        dec2bcd(day),
        weekday,
        static_cast<uint8_t>(dec2bcd(month) + (year < 2000 ? 0x80 : 0)),
        dec2bcd(year % 100)};

    // Write time registers
    write_registers(REG_SECONDS, time_buf, sizeof(time_buf));

    // Write date registers
    write_registers(REG_DAYS, date_buf, sizeof(date_buf));

    ESP_LOGD(TAG, "Date and time set complete (weekday: %d)", weekday);
}

void RTC::getTime(uint16_t &year, uint8_t &month, uint8_t &day, uint8_t &hour, uint8_t &minute, uint8_t &second)
{
    uint8_t time_buf[3] = {0};
    uint8_t date_buf[4] = {0};

    // Read time registers
    if (!read_registers(REG_SECONDS, time_buf, sizeof(time_buf)))
    {
        ESP_LOGW(TAG, "Failed to read time");
        return;
    }

    // Read date registers
    if (!read_registers(REG_DAYS, date_buf, sizeof(date_buf)))
    {
        ESP_LOGW(TAG, "Failed to read date");
        return;
    }

    second = bcd2dec(time_buf[0] & 0x7F);
    minute = bcd2dec(time_buf[1] & 0x7F);
    hour = bcd2dec(time_buf[2] & 0x3F);
    day = bcd2dec(date_buf[0] & 0x3F);
    month = bcd2dec(date_buf[2] & 0x1F);
    year = bcd2dec(date_buf[3]) + ((date_buf[2] & 0x80) ? 1900 : 2000);

    ESP_LOGD(TAG, "Date and time read: %04d-%02d-%02d %02d:%02d:%02d",
             year, month, day, hour, minute, second);
}

bool RTC::isRunning()
{
    uint8_t seconds;
    if (!read_registers(REG_SECONDS, &seconds, 1))
    {
        ESP_LOGW(TAG, "Failed to read RTC status");
        return false;
    }

    bool running = !(seconds & 0x80); // Check VL bit
    if (!running)
    {
        ESP_LOGW(TAG, "RTC not running or battery is low (VL bit set)");
    }
    return running;
}

uint8_t RTC::bcd2dec(uint8_t bcd)
{
    uint8_t dec = (bcd >> 4) * 10 + (bcd & 0x0F);
    ESP_LOGV(TAG, "BCD to DEC conversion: 0x%02X -> %d", bcd, dec);
    return dec;
}

uint8_t RTC::dec2bcd(uint8_t dec)
{
    uint8_t bcd = ((dec / 10) << 4) | (dec % 10);
    ESP_LOGV(TAG, "DEC to BCD conversion: %d -> 0x%02X", dec, bcd);
    return bcd;
}

bool RTC::read_registers(uint8_t reg, uint8_t *data, size_t length)
{
    _wire.beginTransmission(WRITE_ADDR);
    _wire.write(reg);
    if (_wire.endTransmission() != 0)
    {
        ESP_LOGW(TAG, "Failed to write register address for read");
        return false;
    }

    size_t bytes_read = _wire.requestFrom(READ_ADDR, length);
    if (bytes_read != length)
    {
        ESP_LOGW(TAG, "Requested %d bytes but received %d bytes", length, bytes_read);
        return false;
    }

    for (size_t i = 0; i < length; i++)
    {
        data[i] = _wire.read();
    }

    return true;
}

void RTC::write_registers(uint8_t reg, uint8_t *data, size_t length)
{
    _wire.beginTransmission(WRITE_ADDR);
    _wire.write(reg);
    for (size_t i = 0; i < length; i++)
    {
        _wire.write(data[i]);
    }
    if (_wire.endTransmission() != 0)
    {
        ESP_LOGW(TAG, "Failed to write registers");
    }
}