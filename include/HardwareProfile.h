#ifndef HARDWARE_PROFILE_H
#define HARDWARE_PROFILE_H

#include <Arduino.h>

// Enumerates the supported M5 Cardputer hardware variants. Extend this enum
// and add a matching entry to the profile table in HardwareProfile.cpp to
// add support for a new board.
enum CardputerModel
{
    MODEL_UNKNOWN = 0,
    MODEL_CARDPUTER = 1,     // Cardputer v1.1 (original)
    MODEL_CARDPUTER_ADV = 2  // Cardputer ADV
};

// Per-model hardware profile. All runtime pin assignments and
// hardware-dependent defaults live here so a single firmware binary can
// support multiple Cardputer variants.
struct HardwareProfile
{
    CardputerModel model;
    const char *model_name;          // Human-readable, e.g. "Cardputer (v1.1)"
    const char *model_key;           // Config key, e.g. "cardputer"

    // GPS UART (Grove port)
    int gps_tx_pin;                  // ESP32 TX -> GPS RX
    int gps_rx_pin;                  // ESP32 RX <- GPS TX
    int gps_default_baud;

    // Buzzer / speaker (M5Unified Speaker abstracts the pin internally,
    // but we record it for documentation and potential override use)
    int buzzer_pin;

    // RGB LED (SK6812)
    int led_pin;

    // SD card (SPI)
    int sd_cs_pin;
    int sd_sck_pin;
    int sd_miso_pin;
    int sd_mosi_pin;

    // Keyboard abstraction flag (reserved for future use)
    bool keyboard_uses_m5unified;
};

// Detect the running hardware via M5.getBoard(). Returns MODEL_UNKNOWN if
// the running board is not one of the supported Cardputer variants.
CardputerModel detectCardputerModel();

// Map a string ("auto", "cardputer", "cardputer_adv") to a CardputerModel.
// "auto" and unknown strings return MODEL_UNKNOWN.
CardputerModel parseModelKey(const String &key);

// Reverse of parseModelKey — returns "cardputer", "cardputer_adv" or "auto".
const char *modelKey(CardputerModel model);

// Look up the hardware profile for a given model. Always returns a valid
// reference; MODEL_UNKNOWN falls back to the Cardputer v1.1 profile.
const HardwareProfile &getProfile(CardputerModel model);

#endif // HARDWARE_PROFILE_H
