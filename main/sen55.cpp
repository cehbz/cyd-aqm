#include "sen55.hpp"

#include "esp_check.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <array>
#include <cassert>

static const char* TAG = "sen55";

/* ── Implementation ──────────────────────────────────────────────────── */

struct Sen55::Impl {
    static constexpr uint8_t kAddress = 0x69;
    static constexpr uint32_t kI2cSpeedHz = 10'000;
    static constexpr int kI2cTimeoutMs = 100;
    static constexpr int kPollIntervalMs = 1'000;

    static constexpr uint16_t kCmdStartMeasurement = 0x0021;
    static constexpr uint16_t kCmdStopMeasurement = 0x0104;
    static constexpr uint16_t kCmdReadDataReady = 0x0202;
    static constexpr uint16_t kCmdReadMeasuredValues = 0x03C4;

    static constexpr int kDelayStartMs = 50;
    static constexpr int kDelayStopMs = 200;
    static constexpr int kDelayCmdMs = 20;

    i2c_master_dev_handle_t dev{};
    Callback cb;
    TaskHandle_t task{};

    static uint8_t Crc8(const uint8_t* data, size_t len)
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

    esp_err_t SendCommand(uint16_t cmd)
    {
        const std::array<uint8_t, 2> buf = {
            static_cast<uint8_t>(cmd >> 8),
            static_cast<uint8_t>(cmd & 0xFF),
        };
        return i2c_master_transmit(dev, buf.data(), buf.size(), kI2cTimeoutMs);
    }

    esp_err_t ReadWords(uint16_t cmd, uint16_t* words, size_t count)
    {
        const std::array<uint8_t, 2> cmd_buf = {
            static_cast<uint8_t>(cmd >> 8),
            static_cast<uint8_t>(cmd & 0xFF),
        };

        const auto rx_len = count * 3;
        uint8_t rx[24];
        assert(rx_len <= sizeof(rx));

        auto err = i2c_master_transmit(dev, cmd_buf.data(), cmd_buf.size(), kI2cTimeoutMs);
        if (err != ESP_OK) {
            return err;
        }
        vTaskDelay(pdMS_TO_TICKS(kDelayCmdMs));
        err = i2c_master_receive(dev, rx, rx_len, kI2cTimeoutMs);
        if (err != ESP_OK) {
            return err;
        }

        for (size_t i = 0; i < count; ++i) {
            const auto* triplet = &rx[i * 3];
            if (auto expected = Crc8(triplet, 2); triplet[2] != expected) {
                ESP_LOGE(TAG, "CRC mismatch at word %zu: got 0x%02X, expected 0x%02X",
                         i, triplet[2], expected);
                return ESP_ERR_INVALID_CRC;
            }
            words[i] = (static_cast<uint16_t>(triplet[0]) << 8) | triplet[1];
        }
        return ESP_OK;
    }

    static void Poll(void* arg)
    {
        auto& self = *static_cast<Impl*>(arg);
        Measurement meas{};

        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(kPollIntervalMs));

            uint16_t ready_word{};
            if (auto err = self.ReadWords(kCmdReadDataReady, &ready_word, 1); err != ESP_OK) {
                ESP_LOGE(TAG, "data-ready failed: %s", esp_err_to_name(err));
                continue;
            }
            if ((ready_word & 0x01) == 0) {
                continue;
            }

            std::array<uint16_t, 8> words{};
            if (auto err = self.ReadWords(kCmdReadMeasuredValues, words.data(), words.size());
                err != ESP_OK) {
                ESP_LOGE(TAG, "Read failed: %s", esp_err_to_name(err));
                continue;
            }

            meas.pm1_0       = static_cast<float>(words[0]) / 10.0f;
            meas.pm2_5       = static_cast<float>(words[1]) / 10.0f;
            meas.pm4_0       = static_cast<float>(words[2]) / 10.0f;
            meas.pm10        = static_cast<float>(words[3]) / 10.0f;
            meas.humidity    = static_cast<float>(static_cast<int16_t>(words[4])) / 100.0f;
            meas.temperature = static_cast<float>(static_cast<int16_t>(words[5])) / 200.0f;
            meas.voc_index   = static_cast<float>(static_cast<int16_t>(words[6])) / 10.0f;
            meas.nox_index   = static_cast<float>(static_cast<int16_t>(words[7])) / 10.0f;

            ESP_LOGI(TAG, "PM2.5=%.1f  T=%.1f  RH=%.1f  VOC=%.0f  NOx=%.0f",
                     meas.pm2_5, meas.temperature, meas.humidity,
                     meas.voc_index, meas.nox_index);

            self.cb(meas);
        }
    }
};

/* ── Construction / destruction ──────────────────────────────────────── */

Sen55::Sen55(void* i2c_bus, Callback cb)
    : impl_(std::make_unique<Impl>())
{
    impl_->cb = std::move(cb);

    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = Impl::kAddress,
        .scl_speed_hz = Impl::kI2cSpeedHz,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(
        static_cast<i2c_master_bus_handle_t>(i2c_bus), &cfg, &impl_->dev));
    ESP_ERROR_CHECK(impl_->SendCommand(Impl::kCmdStartMeasurement));
    vTaskDelay(pdMS_TO_TICKS(Impl::kDelayStartMs));
    ESP_LOGI(TAG, "Measurement started on I2C addr 0x%02X", Impl::kAddress);

    xTaskCreatePinnedToCore(Impl::Poll, "sen55", 4096,
                            impl_.get(), 5, &impl_->task, tskNO_AFFINITY);
}

Sen55::~Sen55()
{
    if (impl_->task) {
        vTaskDelete(impl_->task);
    }
    impl_->SendCommand(Impl::kCmdStopMeasurement);
    vTaskDelay(pdMS_TO_TICKS(Impl::kDelayStopMs));
    ESP_LOGI(TAG, "Measurement stopped");
}
