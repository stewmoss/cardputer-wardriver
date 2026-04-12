#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include <Arduino.h>
#include <FS.h>
#include "Config.h"
#include "GPSManager.h"

class DataLogger
{
public:
    DataLogger();
    ~DataLogger();
    void setGPSManager(GPSManager *mgr) { gpsManager = mgr; }
    bool begin(fs::FS &fs);
    bool openLogFile();
    void logEntry(const WiFiNetwork &net, const GPSData &gps);
    void logFlagEntry(const GPSData &gps);
    void flush();
    void close();
    String getActiveFilename();
    uint32_t getTotalLogged();
    bool isOpen();

private:
    fs::FS *filesystem;
    GPSManager *gpsManager;
    File logFile;
    uint32_t totalLogged;
    String activeFilename;
    bool fileOpen;

    int findNextFileIndex();
    void writePreHeader();
    void writeCSVHeader();
};

#endif // DATA_LOGGER_H
