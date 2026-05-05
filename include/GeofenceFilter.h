#ifndef GEOFENCE_FILTER_H
#define GEOFENCE_FILTER_H

#include <Arduino.h>
#include "Config.h"

class GeofenceFilter
{
public:
    GeofenceFilter(bool superDebug = false);
    void configure(const FilterConfig &filter, float accuracyThreshold);

    bool isSSIDExcluded(const String &ssid);
    bool isBSSIDExcluded(const String &bssid);
    bool isInsideGeofence(double lat, double lon);
    bool isAccuracyAcceptable(float accuracyMeters);
    bool shouldLog(const WiFiNetwork &net, const GPSData &gps);

private:
    std::vector<String> excludedSSIDs;
    std::vector<String> excludedBSSIDs;
    std::vector<GeofenceBox> geofenceBoxes;
    float accuracyThreshold;
    bool superDebug;
};

#endif // GEOFENCE_FILTER_H
