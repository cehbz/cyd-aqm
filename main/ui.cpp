#include "ui.hpp"
#include "display.hpp"

#include <cstdio>
#include <string>

/* ── Colour palette ──────────────────────────────────────────────────── */

namespace colour {
const auto bg      = lv_color_hex(0x1E1E2E);  // dark background
const auto card    = lv_color_hex(0x313244);   // card surface
const auto text    = lv_color_hex(0xCDD6F4);   // primary text
const auto label   = lv_color_hex(0xA6ADC8);   // secondary text

// AQI-style severity backgrounds (dark, saturated for white text)
const auto good    = lv_color_hex(0x1B5E20);   // dark green
const auto mod     = lv_color_hex(0x7B6C00);   // dark yellow
const auto usg     = lv_color_hex(0xBF360C);   // dark orange
const auto unhlt   = lv_color_hex(0xB71C1C);   // dark red
} // namespace colour

/* ── Card metadata ───────────────────────────────────────────────────── */

struct CardMeta {
    const char* name;
    const char* unit;
};

static constexpr std::array<CardMeta, 8> kCards = {{
    {"PM 1.0", "ug/m3"},
    {"PM 2.5", "ug/m3"},
    {"PM 4.0", "ug/m3"},
    {"PM 10",  "ug/m3"},
    {"Temp",   "\xC2\xB0""C"},
    {"Humidity", "%RH"},
    {"VOC",    "index"},
    {"NOx",    "index"},
}};

/* ── Per-metric colour helpers ───────────────────────────────────────── */

// Returns green/yellow/orange/red based on value and metric index
static lv_color_t metric_color(size_t index, float v)
{
    switch (index) {
    case 0: // PM 1.0 (µg/m³)
        if (v <= 12.f)  return colour::good;
        if (v <= 35.f)  return colour::mod;
        if (v <= 55.f)  return colour::usg;
        return colour::unhlt;
    case 1: // PM 2.5 (µg/m³, US EPA)
        if (v <= 12.f)  return colour::good;
        if (v <= 35.4f) return colour::mod;
        if (v <= 55.4f) return colour::usg;
        return colour::unhlt;
    case 2: // PM 4.0 (µg/m³)
        if (v <= 25.f)  return colour::good;
        if (v <= 50.f)  return colour::mod;
        if (v <= 75.f)  return colour::usg;
        return colour::unhlt;
    case 3: // PM 10 (µg/m³, US EPA)
        if (v <= 54.f)  return colour::good;
        if (v <= 154.f) return colour::mod;
        if (v <= 254.f) return colour::usg;
        return colour::unhlt;
    case 4: // Temperature (°C) — comfort range
        if (v >= 18.f && v <= 24.f) return colour::good;
        if (v >= 15.f && v <= 28.f) return colour::mod;
        if (v >= 10.f && v <= 32.f) return colour::usg;
        return colour::unhlt;
    case 5: // Humidity (%RH) — comfort range
        if (v >= 30.f && v <= 60.f) return colour::good;
        if (v >= 20.f && v <= 70.f) return colour::mod;
        if (v >= 10.f && v <= 80.f) return colour::usg;
        return colour::unhlt;
    case 6: // VOC index (Sensirion 1–500)
        if (v <= 150.f) return colour::good;
        if (v <= 250.f) return colour::mod;
        if (v <= 400.f) return colour::usg;
        return colour::unhlt;
    case 7: // NOx index (Sensirion 1–500)
        if (v <= 20.f)  return colour::good;
        if (v <= 150.f) return colour::mod;
        if (v <= 250.f) return colour::usg;
        return colour::unhlt;
    default:
        return colour::text;
    }
}

/* ── Ui implementation ───────────────────────────────────────────────── */

Ui::Card Ui::make_card(lv_obj_t* parent, size_t index)
{
    Card c;
    const auto& meta = kCards[index];

    c.container = lv_obj_create(parent);
    lv_obj_set_flex_flow(c.container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(c.container, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_flex_grow(c.container, 1);
    lv_obj_set_height(c.container, LV_PCT(100));
    lv_obj_set_style_bg_color(c.container, colour::card, 0);
    lv_obj_set_style_bg_opa(c.container, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(c.container, 12, 0);
    lv_obj_set_style_pad_all(c.container, 4, 0);
    lv_obj_set_style_pad_row(c.container, 2, 0);
    lv_obj_set_style_border_width(c.container, 0, 0);
    lv_obj_clear_flag(c.container, LV_OBJ_FLAG_SCROLLABLE);

    c.name_label = lv_label_create(c.container);
    lv_label_set_text(c.name_label, meta.name);
    lv_obj_set_style_text_font(c.name_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(c.name_label, colour::label, 0);

    c.value_label = lv_label_create(c.container);
    lv_label_set_text(c.value_label, "--");
    lv_obj_set_style_text_font(c.value_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(c.value_label, colour::text, 0);

    c.unit_label = lv_label_create(c.container);
    lv_label_set_text(c.unit_label, meta.unit);
    lv_obj_set_style_text_font(c.unit_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(c.unit_label, colour::label, 0);

    return c;
}

Ui::Ui()
{
    auto* scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, colour::bg, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr, kPad, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(scr, kPad, 0);

    /* Title */
    auto* title = lv_label_create(scr);
    lv_label_set_text(title, "Air Quality Monitor");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(title, colour::text, 0);

    /* Card rows */
    for (int row = 0; row < kRows; ++row) {
        auto* row_obj = lv_obj_create(scr);
        lv_obj_set_width(row_obj, LV_PCT(100));
        lv_obj_set_flex_grow(row_obj, 1);
        lv_obj_set_flex_flow(row_obj, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row_obj, LV_FLEX_ALIGN_SPACE_EVENLY,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(row_obj, kPad, 0);
        lv_obj_set_style_pad_all(row_obj, 0, 0);
        lv_obj_set_style_bg_opa(row_obj, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row_obj, 0, 0);
        lv_obj_clear_flag(row_obj, LV_OBJ_FLAG_SCROLLABLE);

        for (int col = 0; col < kCols; ++col) {
            auto idx = static_cast<size_t>(row * kCols + col);
            cards_[idx] = make_card(row_obj, idx);
        }
    }

    /* Status line */
    status_label_ = lv_label_create(scr);
    lv_label_set_text(status_label_, "Waiting for first reading...");
    lv_obj_set_style_text_font(status_label_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_label_, colour::label, 0);
}

void Ui::update_measurements(const Sen55::Measurement& data)
{
    const std::array values = {
        data.pm1_0, data.pm2_5, data.pm4_0, data.pm10,
        data.temperature, data.humidity, data.voc_index, data.nox_index,
    };

    for (size_t i = 0; i < kCardCount; ++i) {
        // PM and environmental values get 1 decimal; indices get none
        auto text = (i >= 6)
            ? std::to_string(static_cast<int>(values[i]))
            : ([&] {
                char buf[16];
                std::snprintf(buf, sizeof(buf), "%.1f", values[i]);
                return std::string(buf);
            })();
        lv_label_set_text(cards_[i].value_label, text.c_str());

        // Tint card background by severity; keep value text white
        lv_obj_set_style_bg_color(cards_[i].container,
                                  metric_color(i, values[i]), 0);
    }
}

void Ui::set_status(std::string_view text)
{
    // lv_label_set_text copies internally, so the view is safe here
    lv_label_set_text(status_label_, std::string(text).c_str());
}

