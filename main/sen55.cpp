#include "sen55.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstring>

static const char* TAG = "sen55";

/* ── CRC-8: polynomial 0x31, init 0xFF ───────────────────────────────── */

uint8_t Sen55::crc8(const uint8_t* data, size_t len)
{
    constexpr uint8_t kPoly = 0x31;
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x80) ? ((crc << 1) ^ kPoly) : (crc << 1);
        }
    }
    return crc;
}

/* ── Construction ────────────────────────────────────────────────────── */

Sen55::Sen55(i2c_master_bus_handle_t bus)
{
    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = kAddress,
        .scl_speed_hz = kI2cSpeedHz,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &cfg, &dev_));
    ESP_LOGI(TAG, "Initialised on I2C addr 0x%02X", kAddress);
}

/* ── Low-level I2C ───────────────────────────────────────────────────── */

esp_err_t Sen55::send_command(uint16_t cmd)
{
    const std::array<uint8_t, 2> buf = {
        static_cast<uint8_t>(cmd >> 8),
        static_cast<uint8_t>(cmd & 0xFF),
    };
    return i2c_master_transmit(dev_, buf.data(), buf.size(), kI2cTimeoutMs);
}

esp_err_t Sen55::read_words(uint16_t cmd, uint16_t* words, size_t count)
{
    const std::array<uint8_t, 2> cmd_buf = {
        static_cast<uint8_t>(cmd >> 8),
        static_cast<uint8_t>(cmd & 0xFF),
    };

    // Each word is 3 bytes on the wire: MSB, LSB, CRC
    const auto rx_len = count * 3;
    uint8_t rx[24]; // max 8 words × 3 bytes
    assert(rx_len <= sizeof(rx));

    auto err = i2c_master_transmit_receive(dev_, cmd_buf.data(), cmd_buf.size(),
                                           rx, rx_len, kI2cTimeoutMs);
    if (err != ESP_OK) {
        return err;
    }

    for (size_t i = 0; i < count; ++i) {
        const auto* triplet = &rx[i * 3];
        if (auto expected = crc8(triplet, 2); triplet[2] != expected) {
            ESP_LOGE(TAG, "CRC mismatch at word %zu: got 0x%02X, expected 0x%02X",
                     i, triplet[2], expected);
            return ESP_ERR_INVALID_CRC;
        }
        words[i] = (static_cast<uint16_t>(triplet[0]) << 8) | triplet[1];
    }
    return ESP_OK;
}

/* ── Public API ──────────────────────────────────────────────────────── */

esp_err_t Sen55::start_measurement()
{
    auto err = send_command(kCmdStartMeasurement);
    if (err == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(kDelayStartMs));
        ESP_LOGI(TAG, "Measurement started");
    }
    return err;
}

esp_err_t Sen55::stop_measurement()
{
    auto err = send_command(kCmdStopMeasurement);
    if (err == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(kDelayStopMs));
        ESP_LOGI(TAG, "Measurement stopped");
    }
    return err;
}

esp_err_t Sen55::read_data_ready(bool& ready)
{
    uint16_t word{};
    vTaskDelay(pdMS_TO_TICKS(kDelayCmdMs));
    auto err = read_words(kCmdReadDataReady, &word, 1);
    if (err == ESP_OK) {
        ready = (word & 0x01) != 0;
    }
    return err;
}

esp_err_t Sen55::read_measured_values(Measurement& out)
{
    std::array<uint16_t, 8> words{};
    vTaskDelay(pdMS_TO_TICKS(kDelayCmdMs));
    auto err = read_words(kCmdReadMeasuredValues, words.data(), words.size());
    if (err != ESP_OK) {
        return err;
    }

    out.pm1_0       = static_cast<float>(words[0]) / 10.0f;
    out.pm2_5       = static_cast<float>(words[1]) / 10.0f;
    out.pm4_0       = static_cast<float>(words[2]) / 10.0f;
    out.pm10        = static_cast<float>(words[3]) / 10.0f;
    out.humidity    = static_cast<float>(static_cast<int16_t>(words[4])) / 100.0f;
    out.temperature = static_cast<float>(static_cast<int16_t>(words[5])) / 200.0f;
    out.voc_index   = static_cast<float>(static_cast<int16_t>(words[6])) / 10.0f;
    out.nox_index   = static_cast<float>(static_cast<int16_t>(words[7])) / 10.0f;

    return ESP_OK;
}

esp_err_t Sen55::device_reset()
{
    auto err = send_command(kCmdDeviceReset);
    if (err == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(kDelayResetMs));
        ESP_LOGI(TAG, "Device reset complete");
    }
    return err;
}
