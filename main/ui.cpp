#include "ui.hpp"

#include "lvgl.h"

#include <array>
#include <cstdio>
#include <string>

/* ── Design tokens ───────────────────────────────────────────────────── */

namespace tokens {
// Colours
const auto kBg      = lv_color_hex(0x1E1E2E);   // dark background
const auto kSurface = lv_color_hex(0x313244);    // card surface
const auto kText    = lv_color_hex(0xCDD6F4);    // primary text
const auto kMuted   = lv_color_hex(0xA6ADC8);    // secondary text
const auto kGood    = lv_color_hex(0x1B5E20);    // dark green
const auto kMod     = lv_color_hex(0x7B6C00);    // dark yellow
const auto kUsg     = lv_color_hex(0xBF360C);    // dark orange
const auto kUnhlt   = lv_color_hex(0xB71C1C);    // dark red

// Typography
constexpr const lv_font_t* kFontXl = &lv_font_montserrat_48;
constexpr const lv_font_t* kFontLg = &lv_font_montserrat_36;
constexpr const lv_font_t* kFontMd = &lv_font_montserrat_20;
constexpr const lv_font_t* kFontSm = &lv_font_montserrat_14;

// Spacing
constexpr int32_t kPad    = 8;
constexpr int32_t kRadius = 12;
} // namespace tokens

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

/* ── Shared styles ───────────────────────────────────────────────────── */

static lv_style_t style_card;
static lv_style_t style_title;
static lv_style_t style_value;
static lv_style_t style_secondary;
static lv_style_t style_status;

// Severity styles — only override bg_color, layered on top of style_card
static lv_style_t style_good;
static lv_style_t style_mod;
static lv_style_t style_usg;
static lv_style_t style_unhlt;

static constexpr size_t kSeverityCount = 4;
static lv_style_t* const kSeverityStyles[kSeverityCount] = {
    &style_good, &style_mod, &style_usg, &style_unhlt,
};

static void InitStyles()
{
    // Card container
    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, tokens::kSurface);
    lv_style_set_bg_opa(&style_card, LV_OPA_COVER);
    lv_style_set_radius(&style_card, tokens::kRadius);
    lv_style_set_pad_all(&style_card, 4);
    lv_style_set_pad_row(&style_card, 2);
    lv_style_set_border_width(&style_card, 0);

    // Title label
    lv_style_init(&style_title);
    lv_style_set_text_font(&style_title, tokens::kFontLg);
    lv_style_set_text_color(&style_title, tokens::kText);

    // Value label (big number)
    lv_style_init(&style_value);
    lv_style_set_text_font(&style_value, tokens::kFontXl);
    lv_style_set_text_color(&style_value, tokens::kText);

    // Secondary label (name, unit)
    lv_style_init(&style_secondary);
    lv_style_set_text_font(&style_secondary, tokens::kFontMd);
    lv_style_set_text_color(&style_secondary, tokens::kMuted);

    // Status label
    lv_style_init(&style_status);
    lv_style_set_text_font(&style_status, tokens::kFontSm);
    lv_style_set_text_color(&style_status, tokens::kMuted);

    // Severity backgrounds
    lv_style_init(&style_good);
    lv_style_set_bg_color(&style_good, tokens::kGood);

    lv_style_init(&style_mod);
    lv_style_set_bg_color(&style_mod, tokens::kMod);

    lv_style_init(&style_usg);
    lv_style_set_bg_color(&style_usg, tokens::kUsg);

    lv_style_init(&style_unhlt);
    lv_style_set_bg_color(&style_unhlt, tokens::kUnhlt);
}

/* ── Per-metric severity ─────────────────────────────────────────────── */

// Returns severity level 0–3 (good / moderate / unhealthy-sensitive / unhealthy)
static size_t MetricSeverity(size_t index, float v)
{
    switch (index) {
    case 0: // PM 1.0 (µg/m³)
        if (v <= 12.f)  return 0;
        if (v <= 35.f)  return 1;
        if (v <= 55.f)  return 2;
        return 3;
    case 1: // PM 2.5 (µg/m³, US EPA)
        if (v <= 12.f)  return 0;
        if (v <= 35.4f) return 1;
        if (v <= 55.4f) return 2;
        return 3;
    case 2: // PM 4.0 (µg/m³)
        if (v <= 25.f)  return 0;
        if (v <= 50.f)  return 1;
        if (v <= 75.f)  return 2;
        return 3;
    case 3: // PM 10 (µg/m³, US EPA)
        if (v <= 54.f)  return 0;
        if (v <= 154.f) return 1;
        if (v <= 254.f) return 2;
        return 3;
    case 4: // Temperature (°C) — comfort range
        if (v >= 18.f && v <= 24.f) return 0;
        if (v >= 15.f && v <= 28.f) return 1;
        if (v >= 10.f && v <= 32.f) return 2;
        return 3;
    case 5: // Humidity (%RH) — comfort range
        if (v >= 30.f && v <= 60.f) return 0;
        if (v >= 20.f && v <= 70.f) return 1;
        if (v >= 10.f && v <= 80.f) return 2;
        return 3;
    case 6: // VOC index (Sensirion 1–500)
        if (v <= 150.f) return 0;
        if (v <= 250.f) return 1;
        if (v <= 400.f) return 2;
        return 3;
    case 7: // NOx index (Sensirion 1–500)
        if (v <= 20.f)  return 0;
        if (v <= 150.f) return 1;
        if (v <= 250.f) return 2;
        return 3;
    default:
        return 0;
    }
}

/* ── Grid layout ─────────────────────────────────────────────────────── */

static constexpr int32_t kColDsc[] = {
    LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
    LV_GRID_TEMPLATE_LAST,
};

// Row 0: title (content-sized), rows 1–2: cards (equal), row 3: status
static constexpr int32_t kRowDsc[] = {
    LV_GRID_CONTENT,
    LV_GRID_FR(1), LV_GRID_FR(1),
    LV_GRID_CONTENT,
    LV_GRID_TEMPLATE_LAST,
};

/* ── Ui::Impl ────────────────────────────────────────────────────────── */

struct Ui::Impl {
    static constexpr size_t kCardCount = 8;

    struct Card {
        lv_obj_t* container{};
        lv_obj_t* value_label{};
    };

    std::array<Card, kCardCount> cards{};
    lv_obj_t* status_label{};

    static Card MakeCard(lv_obj_t* parent, size_t index)
    {
        Card c;
        const auto& meta = kCards[index];

        c.container = lv_obj_create(parent);
        lv_obj_add_style(c.container, &style_card, 0);
        lv_obj_set_flex_flow(c.container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(c.container, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(c.container, LV_OBJ_FLAG_SCROLLABLE);

        auto* name = lv_label_create(c.container);
        lv_label_set_text(name, meta.name);
        lv_obj_add_style(name, &style_secondary, 0);

        c.value_label = lv_label_create(c.container);
        lv_label_set_text(c.value_label, "--");
        lv_obj_add_style(c.value_label, &style_value, 0);

        auto* unit = lv_label_create(c.container);
        lv_label_set_text(unit, meta.unit);
        lv_obj_add_style(unit, &style_secondary, 0);

        return c;
    }
};

/* ── Ui public methods ───────────────────────────────────────────────── */

Ui::Ui() : impl_(std::make_unique<Impl>())
{
    InitStyles();

    auto* scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, tokens::kBg, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr, tokens::kPad, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // Grid layout: 4 columns, 4 rows (title, cards, cards, status)
    lv_obj_set_layout(scr, LV_LAYOUT_GRID);
    lv_obj_set_grid_dsc_array(scr, kColDsc, kRowDsc);
    lv_obj_set_style_pad_row(scr, tokens::kPad, 0);
    lv_obj_set_style_pad_column(scr, tokens::kPad, 0);

    // Title — spans all 4 columns
    auto* title = lv_label_create(scr);
    lv_label_set_text(title, "Air Quality Monitor");
    lv_obj_add_style(title, &style_title, 0);
    lv_obj_set_grid_cell(title,
        LV_GRID_ALIGN_CENTER, 0, 4,
        LV_GRID_ALIGN_CENTER, 0, 1);

    // Cards — placed directly on the grid, no intermediate row objects
    for (size_t i = 0; i < Impl::kCardCount; ++i) {
        impl_->cards[i] = Impl::MakeCard(scr, i);
        lv_obj_set_grid_cell(impl_->cards[i].container,
            LV_GRID_ALIGN_STRETCH, static_cast<int32_t>(i % 4), 1,
            LV_GRID_ALIGN_STRETCH, static_cast<int32_t>(1 + i / 4), 1);
    }

    // Status — spans all 4 columns
    impl_->status_label = lv_label_create(scr);
    lv_label_set_text(impl_->status_label, "Waiting for first reading...");
    lv_obj_add_style(impl_->status_label, &style_status, 0);
    lv_obj_set_grid_cell(impl_->status_label,
        LV_GRID_ALIGN_CENTER, 0, 4,
        LV_GRID_ALIGN_CENTER, 3, 1);
}

Ui::~Ui() = default;

void Ui::UpdateMeasurements(const Sen55::Measurement& data)
{
    const std::array values = {
        data.pm1_0, data.pm2_5, data.pm4_0, data.pm10,
        data.temperature, data.humidity, data.voc_index, data.nox_index,
    };

    for (size_t i = 0; i < Impl::kCardCount; ++i) {
        auto text = (i >= 6)
            ? std::to_string(static_cast<int>(values[i]))
            : ([&] {
                char buf[16];
                std::snprintf(buf, sizeof(buf), "%.1f", values[i]);
                return std::string(buf);
            })();
        lv_label_set_text(impl_->cards[i].value_label, text.c_str());

        // Swap severity style (like toggling CSS classes)
        for (auto* sev : kSeverityStyles) {
            lv_obj_remove_style(impl_->cards[i].container, sev, 0);
        }
        auto level = MetricSeverity(i, values[i]);
        lv_obj_add_style(impl_->cards[i].container,
                         kSeverityStyles[level], 0);
    }
}

void Ui::SetStatus(std::string_view text)
{
    lv_label_set_text(impl_->status_label, std::string(text).c_str());
}
