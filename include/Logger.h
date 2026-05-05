#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include "Config.h"

class Logger
{
public:
    Logger();
    bool begin();
    void setConfig(AppConfig *cfg);

    void debugPrint(const char *msg);
    void debugPrintln(const char *msg);
    void debugPrintln(const String &msg);
    void debugPrintln();
    
    bool logStatsCSV(const String& csvRow);

    String getDebugLogFile();
    bool clearDebugLog();

private:
    AppConfig *config;
    String debugFile;
    bool sdReady;

    void writeToFile(const char *msg, bool newline);
    bool ensureLogDirectory();
};

extern Logger logger;

#endif // LOGGER_H
