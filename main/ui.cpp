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
const auto accent  = lv_color_hex(0x89B4FA);   // button

// PM2.5 AQI thresholds (US EPA breakpoints)
const auto good    = lv_color_hex(0xA6E3A1);   // 0–12   µg/m³
const auto mod     = lv_color_hex(0xF9E2AF);   // 12.1–35.4
const auto usg     = lv_color_hex(0xFAB387);   // 35.5–55.4
const auto unhlt   = lv_color_hex(0xF38BA8);   // >55.4
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

/* ── Ui implementation ───────────────────────────────────────────────── */

lv_color_t Ui::pm25_color(float pm25)
{
    if (pm25 <= 12.0f)  return colour::good;
    if (pm25 <= 35.4f)  return colour::mod;
    if (pm25 <= 55.4f)  return colour::usg;
    return colour::unhlt;
}

void Ui::refresh_trampoline(lv_event_t* e)
{
    auto* self = static_cast<Ui*>(lv_event_get_user_data(e));
    if (self->refresh_cb_) {
        self->refresh_cb_();
    }
}

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
    lv_obj_set_style_pad_all(c.container, 8, 0);
    lv_obj_set_style_border_width(c.container, 0, 0);

    c.name_label = lv_label_create(c.container);
    lv_label_set_text(c.name_label, meta.name);
    lv_obj_set_style_text_font(c.name_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(c.name_label, colour::label, 0);

    c.value_label = lv_label_create(c.container);
    lv_label_set_text(c.value_label, "--");
    lv_obj_set_style_text_font(c.value_label, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(c.value_label, colour::text, 0);

    c.unit_label = lv_label_create(c.container);
    lv_label_set_text(c.unit_label, meta.unit);
    lv_obj_set_style_text_font(c.unit_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(c.unit_label, colour::label, 0);

    return c;
}

Ui::Ui()
{
    auto* scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, colour::bg, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr, kPad, 0);

    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(scr, kPad, 0);

    /* Title */
    auto* title = lv_label_create(scr);
    lv_label_set_text(title, "Air Quality Monitor");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
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

        for (int col = 0; col < kCols; ++col) {
            auto idx = static_cast<size_t>(row * kCols + col);
            cards_[idx] = make_card(row_obj, idx);
        }
    }

    /* Status bar */
    auto* status_bar = lv_obj_create(scr);
    lv_obj_set_width(status_bar, LV_PCT(100));
    lv_obj_set_height(status_bar, kStatusHeight);
    lv_obj_set_flex_flow(status_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status_bar, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(status_bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_set_style_pad_all(status_bar, 0, 0);

    status_label_ = lv_label_create(status_bar);
    lv_label_set_text(status_label_, "Waiting for first reading...");
    lv_obj_set_style_text_font(status_label_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_label_, colour::label, 0);

    refresh_btn_ = lv_button_create(status_bar);
    lv_obj_set_size(refresh_btn_, 120, 36);
    lv_obj_set_style_bg_color(refresh_btn_, colour::accent, 0);
    lv_obj_set_style_radius(refresh_btn_, 8, 0);
    lv_obj_add_event_cb(refresh_btn_, refresh_trampoline, LV_EVENT_CLICKED, this);

    auto* btn_label = lv_label_create(refresh_btn_);
    lv_label_set_text(btn_label, "Refresh");
    lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_14, 0);
    lv_obj_center(btn_label);
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
    }

    // Colour-code PM2.5
    lv_obj_set_style_text_color(cards_[1].value_label, pm25_color(data.pm2_5), 0);
}

void Ui::set_status(std::string_view text)
{
    // lv_label_set_text copies internally, so the view is safe here
    lv_label_set_text(status_label_, std::string(text).c_str());
}

void Ui::on_refresh(RefreshCallback cb)
{
    refresh_cb_ = std::move(cb);
}
