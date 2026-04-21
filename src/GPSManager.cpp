#include "GPSManager.h"
#include "Logger.h"
#include <sys/time.h>
#include <time.h>

GPSManager::GPSManager()
    : gpsSerial(nullptr), initialized(false), timeSynced(false), timeLastSynced(0), syncedGmtOffsetHours(0), superDebug(false), everHadFix(false)
{
    lastKnownFix.lat = 0.0;
    lastKnownFix.lng = 0.0;
    lastKnownFix.altitude = 0.0f;
    lastKnownFix.accuracy = 999.0f;
    lastKnownFix.satellites = 0;
    lastKnownFix.isValid = false;
    lastKnownFix.has3DFix = false;
    lastKnownFix.dateTime = "0000-00-00 00:00:00";
}

void GPSManager::begin(int txPin, int rxPin, int baud, bool superDebugEnabled)
{
    superDebug = superDebugEnabled;
    // ESP32 HardwareSerial::begin(baud, config, rxPin, txPin)
    // Note: the ESP32 API takes RX pin first, then TX pin
    gpsSerial = &Serial1;
    gpsSerial->begin(baud, SERIAL_8N1, rxPin, txPin);
    initialized = true;

    logger.debugPrintln(String("[GPS] Initialized on TX=") + String(txPin) +
                        " RX=" + String(rxPin) + " @ " + String(baud));
}

void GPSManager::update()
{
    if (!initialized || !gpsSerial)
        return;

    while (gpsSerial->available() > 0)
    {
        gps.encode(gpsSerial->read());
    }
}

GPSData GPSManager::getData()
{
    GPSData data;
    data.lat = gps.location.isValid() ? gps.location.lat() : 0.0;
    data.lng = gps.location.isValid() ? gps.location.lng() : 0.0;
    data.altitude = gps.altitude.isValid() ? (float)gps.altitude.meters() : 0.0f;
    data.accuracy = gps.hdop.isValid() ? (float)gps.hdop.hdop() * HDOP_TO_METERS_MULTIPLIER : 999.0f;
    data.satellites = gps.satellites.isValid() ? gps.satellites.value() : 0;
    data.isValid = gps.location.isValid();
    data.has3DFix = has3DFix();
    data.dateTime = getFormattedDateTime(true); // Get UTC time for data timestamp

    if (data.isValid && data.has3DFix)
    {
        lastKnownFix = data;
        everHadFix = true;
    }

    return data;
}

GPSData GPSManager::getLastKnownData()
{
    return lastKnownFix;
}

bool GPSManager::hasEverHadFix()
{
    return everHadFix;
}

bool GPSManager::has3DFix()
{
    return gps.location.isValid() &&
           gps.satellites.isValid() &&
           gps.satellites.value() >= SATELLITES_FOR_FIX;
}

float GPSManager::getAccuracyMeters()
{
    if (gps.hdop.isValid())
    {
        return (float)gps.hdop.hdop() * HDOP_TO_METERS_MULTIPLIER;
    }
    return 999.0f;
}

String GPSManager::getFormattedDateTime(bool UTCTime)
{
    time_t now = time(nullptr);

    if (UTCTime)
    {
        // Return UTC time by applying GMT offset
        now -= (time_t)syncedGmtOffsetHours * 3600L;
    }

    struct tm timeInfo;
    gmtime_r(&now, &timeInfo);

    char formatted[20];
    snprintf(formatted, sizeof(formatted), "%04d-%02d-%02d %02d:%02d:%02d",
             timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday,
             timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
    return String(formatted);
}

int GPSManager::getSatellites()
{
    return gps.satellites.isValid() ? gps.satellites.value() : 0;
}

/// @brief Sync the system time with the GPS time
/// @param gmtOffsetHours The GMT offset in hours
/// @return true if the system time was successfully synced, false otherwise
bool GPSManager::syncSystemTime(int gmtOffsetHours)
{
    if (!gps.date.isValid() || !gps.time.isValid())
    {
        return false;
    }

    struct tm t;
    t.tm_year = gps.date.year() - 1900;
    t.tm_mon = gps.date.month() - 1;
    t.tm_mday = gps.date.day();
    t.tm_hour = gps.time.hour();
    t.tm_min = gps.time.minute();
    t.tm_sec = gps.time.second();
    t.tm_isdst = 0;

    // Convert to epoch (UTC), then apply GMT offset
    time_t epoch = mktime(&t);
    epoch += (long)gmtOffsetHours * 3600L;

    struct timeval tv;
    tv.tv_sec = epoch;
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);

    syncedGmtOffsetHours = gmtOffsetHours;
    timeSynced = true;
    timeLastSynced = millis();
    if (superDebug)
    {
        time_t syncedNow = time(nullptr);
        struct tm localTimeInfo;
        localtime_r(&syncedNow, &localTimeInfo);

        char localFormatted[20];
        snprintf(localFormatted, sizeof(localFormatted), "%04d-%02d-%02d %02d:%02d:%02d",
                 localTimeInfo.tm_year + 1900, localTimeInfo.tm_mon + 1, localTimeInfo.tm_mday,
                 localTimeInfo.tm_hour, localTimeInfo.tm_min, localTimeInfo.tm_sec);

        logger.debugPrintln(String("[GPS] System time synced: ") + String(localFormatted) +
                            " (GMT" + (gmtOffsetHours >= 0 ? "+" : "") + String(gmtOffsetHours) +
                            "), CSV GMT: " + getFormattedDateTime(true));
    }
    return true;
}

/// @brief Check if the system time is currently synced with the GPS
/// @return true if the system time is synced and the last sync was within 30 minutes, false otherwise
bool GPSManager::isTimeSynced()
{
    if (!timeSynced)
        return false;
    return (millis() - timeLastSynced) < 30UL * 60UL * 1000UL;
}
