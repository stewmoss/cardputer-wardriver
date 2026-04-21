#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include "Config.h"
#include "HardwareProfile.h"

class ConfigManager
{
public:
    ConfigManager();
    bool loadConfig(const char *filename = CONFIG_FILENAME);
    bool saveConfig(const char *filename = CONFIG_FILENAME);
    AppConfig &getConfig() { return config; }
    bool isValid() { return configValid; }

    // Resolve the user selection ("auto" | "cardputer" | "cardputer_adv") to
    // a concrete model, falling back to the detected model when "auto".
    CardputerModel effectiveModel(CardputerModel detected) const;

    // If the effective model differs from the stored detected_model, reseed
    // all profile-driven fields in the active config (GPS pins, etc.) to
    // the values from the supplied profile. Returns true when a reseed
    // occurred (caller should log and save).
    bool reseedFromProfile(const HardwareProfile &profile, CardputerModel effective);

private:
    AppConfig config;
    bool configValid;
    void setDefaults();
};

#endif // CONFIG_MANAGER_H
