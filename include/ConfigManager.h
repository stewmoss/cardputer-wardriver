#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include "Config.h"

class ConfigManager
{
public:
    ConfigManager();
    bool loadConfig(const char *filename = CONFIG_FILENAME);
    bool saveConfig(const char *filename = CONFIG_FILENAME);
    AppConfig &getConfig() { return config; }
    bool isValid() { return configValid; }

private:
    AppConfig config;
    bool configValid;
    void setDefaults();
};

#endif // CONFIG_MANAGER_H
