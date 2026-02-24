#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

#include <array>
#include <cstdint>

class Sen55 {
public:
    struct Measurement {
        float pm1_0{};        // µg/m³
        float pm2_5{};        // µg/m³
        float pm4_0{};        // µg/m³
        float pm10{};         // µg/m³
        float humidity{};     // %RH
        float temperature{};  // °C
        float voc_index{};    // 1–500
        float nox_index{};    // 1–500
    };

    explicit Sen55(i2c_master_bus_handle_t bus);
    ~Sen55() = default;

    Sen55(const Sen55&) = delete;
    Sen55& operator=(const Sen55&) = delete;
    Sen55(Sen55&&) = delete;
    Sen55& operator=(Sen55&&) = delete;

    [[nodiscard]] esp_err_t start_measurement();
    [[nodiscard]] esp_err_t stop_measurement();
    [[nodiscard]] esp_err_t read_data_ready(bool& ready);
    [[nodiscard]] esp_err_t read_measured_values(Measurement& out);
    [[nodiscard]] esp_err_t device_reset();

private:
    static constexpr uint8_t kAddress = 0x69;
    static constexpr uint32_t kI2cSpeedHz = 100'000;
    static constexpr int kI2cTimeoutMs = 100;

    // I2C commands (big-endian 16-bit)
    static constexpr uint16_t kCmdStartMeasurement = 0x0021;
    static constexpr uint16_t kCmdStopMeasurement = 0x0104;
    static constexpr uint16_t kCmdReadDataReady = 0x0202;
    static constexpr uint16_t kCmdReadMeasuredValues = 0x03C4;
    static constexpr uint16_t kCmdDeviceReset = 0xD304;

    // Post-command delays (ms)
    static constexpr int kDelayStartMs = 50;
    static constexpr int kDelayStopMs = 200;
    static constexpr int kDelayResetMs = 100;
    static constexpr int kDelayCmdMs = 20;

    static uint8_t crc8(const uint8_t* data, size_t len);

    [[nodiscard]] esp_err_t send_command(uint16_t cmd);
    [[nodiscard]] esp_err_t read_words(uint16_t cmd, uint16_t* words, size_t count);

    i2c_master_dev_handle_t dev_{};
};
