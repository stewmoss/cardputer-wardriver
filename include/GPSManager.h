#ifndef GPS_MANAGER_H
#define GPS_MANAGER_H

#include <Arduino.h>
#include <TinyGPSPlus.h>
#include "Config.h"

class GPSManager
{
public:
    GPSManager();
    void begin(int txPin, int rxPin, bool superDebugEnabled = false);
    void update();
    GPSData getData();
    GPSData getLastKnownData();
    bool hasEverHadFix();
    bool has3DFix();
    float getAccuracyMeters();
    String getFormattedDateTime(bool UTCTime = false);
    int getSatellites();
    bool syncSystemTime(int gmtOffsetHours);
    bool isTimeSynced();

private:
    TinyGPSPlus gps;
    HardwareSerial *gpsSerial;
    bool initialized;
    bool timeSynced;
    unsigned long timeLastSynced;
    int syncedGmtOffsetHours;
    bool superDebug;
    GPSData lastKnownFix;
    bool everHadFix;
};

#endif // GPS_MANAGER_H
