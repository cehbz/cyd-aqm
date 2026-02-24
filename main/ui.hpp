#pragma once

#include "sen55.hpp"
#include "lvgl.h"

#include <array>
#include <cstdint>
#include <functional>
#include <string_view>

class Ui {
public:
    using RefreshCallback = std::function<void()>;

    Ui();

    Ui(const Ui&) = delete;
    Ui& operator=(const Ui&) = delete;
    Ui(Ui&&) = delete;
    Ui& operator=(Ui&&) = delete;

    /** Update all measurement value labels. */
    void update_measurements(const Sen55::Measurement& data);

    /** Update the status text (e.g. last-update timestamp). */
    void set_status(std::string_view text);

    /** Register a callback invoked when the Refresh button is pressed. */
    void on_refresh(RefreshCallback cb);

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
    static constexpr int kStatusHeight = 44;

    Card make_card(lv_obj_t* parent, size_t index);
    static lv_color_t pm25_color(float pm25);
    static void refresh_trampoline(lv_event_t* e);

    std::array<Card, kCardCount> cards_{};
    lv_obj_t* status_label_{};
    lv_obj_t* refresh_btn_{};
    RefreshCallback refresh_cb_;
};
