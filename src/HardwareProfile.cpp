#include "HardwareProfile.h"
#include <M5Unified.h>

// ── Profile tables ──────────────────────────────────────────────────────────

static const HardwareProfile PROFILE_CARDPUTER = {
    MODEL_CARDPUTER,
    "Cardputer (v1.1)",
    "cardputer",
    // GPS UART (Grove port)
    /* gps_tx_pin      */ 2,
    /* gps_rx_pin      */ 1,
    /* gps_default_baud*/ 115200,
    // Buzzer / Speaker
    /* buzzer_pin      */ 2,
    // RGB LED
    /* led_pin         */ 21,
    // SD SPI
    /* sd_cs_pin       */ 12,
    /* sd_sck_pin      */ 40,
    /* sd_miso_pin     */ 39,
    /* sd_mosi_pin     */ 14,
    /* keyboard_uses_m5unified */ true,
};

// NOTE: Actual ADV pin values are not yet verified on real hardware. Any
// value marked TODO holds the Cardputer v1.1 equivalent as a safe placeholder.
// Phase 2 of the ADV rollout replaces these TODOs with verified values.
static const HardwareProfile PROFILE_CARDPUTER_ADV = {
    MODEL_CARDPUTER_ADV,
    "Cardputer ADV",
    "cardputer_adv",
    // GPS UART (Grove port)
    /* gps_tx_pin      */ 13,  
    /* gps_rx_pin      */ 15,  
    /* gps_default_baud*/ 115200,
    // Buzzer / Speaker
    /* buzzer_pin      */ 2,   
    // RGB LED
    /* led_pin         */ 21,  
    // SD SPI — assumed identical to v1.1 per PRD D8
    /* sd_cs_pin       */ 12,
    /* sd_sck_pin      */ 40,
    /* sd_miso_pin     */ 39,
    /* sd_mosi_pin     */ 14,
    /* keyboard_uses_m5unified */ true,
};

// ── Public API ──────────────────────────────────────────────────────────────

CardputerModel detectCardputerModel()
{
    m5::board_t b = M5.getBoard();
    switch (b)
    {
    case m5::board_t::board_M5Cardputer:
        return MODEL_CARDPUTER;
    case m5::board_t::board_M5CardputerADV:
        return MODEL_CARDPUTER_ADV;
    default:
        return MODEL_UNKNOWN;
    }
}

CardputerModel parseModelKey(const String &key)
{
    if (key == "cardputer")
        return MODEL_CARDPUTER;
    if (key == "cardputer_adv")
        return MODEL_CARDPUTER_ADV;
    return MODEL_UNKNOWN; // "auto" and anything else
}

const char *modelKey(CardputerModel model)
{
    switch (model)
    {
    case MODEL_CARDPUTER:     return "cardputer";
    case MODEL_CARDPUTER_ADV: return "cardputer_adv";
    default:                  return "auto";
    }
}

const HardwareProfile &getProfile(CardputerModel model)
{
    switch (model)
    {
    case MODEL_CARDPUTER_ADV:
        return PROFILE_CARDPUTER_ADV;
    case MODEL_CARDPUTER:
    default:
        return PROFILE_CARDPUTER;
    }
}
