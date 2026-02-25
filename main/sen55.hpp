#pragma once

#include <cstdint>
#include <functional>
#include <memory>

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

    using Callback = std::function<void(const Measurement&)>;

    /** Starts measurement and spawns a polling task. Callback fires from the task. */
    Sen55(void* i2c_bus, Callback cb);

    /** Stops measurement and deletes the polling task. */
    ~Sen55();

    Sen55(const Sen55&) = delete;
    Sen55& operator=(const Sen55&) = delete;
    Sen55(Sen55&&) = delete;
    Sen55& operator=(Sen55&&) = delete;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
