#pragma once

#include "sen55.hpp"

#include <memory>
#include <string_view>

class Ui {
public:
    Ui();
    ~Ui();

    Ui(const Ui&) = delete;
    Ui& operator=(const Ui&) = delete;
    Ui(Ui&&) = delete;
    Ui& operator=(Ui&&) = delete;

    void UpdateMeasurements(const Sen55::Measurement& data);
    void SetStatus(std::string_view text);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
