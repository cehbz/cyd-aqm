#pragma once

#include "sen55.hpp"
#include "lvgl.h"

#include <array>
#include <cstdint>
#include <string_view>

class Ui {
public:
    Ui();

    Ui(const Ui&) = delete;
    Ui& operator=(const Ui&) = delete;
    Ui(Ui&&) = delete;
    Ui& operator=(Ui&&) = delete;

    /** Update all measurement value labels. */
    void update_measurements(const Sen55::Measurement& data);

    /** Update the status text (e.g. last-update timestamp). */
    void set_status(std::string_view text);

private:
    struct Card {
        lv_obj_t* container{};
        lv_obj_t* name_label{};
        lv_obj_t* value_label{};
        lv_obj_t* unit_label{};
    };

    static constexpr size_t kCardCount = 8;
    static constexpr int kCols = 4;
    static constexpr int kRows = 2;
    static constexpr int kPad = 8;
    static constexpr int kStatusHeight = 28;

    Card make_card(lv_obj_t* parent, size_t index);

    std::array<Card, kCardCount> cards_{};
    lv_obj_t* status_label_{};
};
