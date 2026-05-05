#include "GeofenceFilter.h"
#include "Logger.h"

GeofenceFilter::GeofenceFilter(bool superDebug)
    : accuracyThreshold(DEFAULT_ACCURACY_THRESHOLD), superDebug(superDebug)
{
}

void GeofenceFilter::configure(const FilterConfig &filter, float threshold)
{
    excludedSSIDs = filter.excluded_ssids;
    excludedBSSIDs = filter.excluded_bssids;
    geofenceBoxes = filter.geofence_boxes;
    accuracyThreshold = threshold;

    if (superDebug)
    {
    logger.debugPrintln(String("[PrivacyFilter] Configured: ") +
                        String(excludedSSIDs.size()) + " excluded SSIDs, " +
                        String(excludedBSSIDs.size()) + " excluded BSSIDs, " +
                        String(geofenceBoxes.size()) + " geofence boxes, " +
                        "accuracy threshold=" + String(accuracyThreshold) + "m");
    } else 
    {
        logger.debugPrintln(String("[PrivacyFilter] Configured: ") +
                            String(excludedSSIDs.size()) + " excluded SSIDs, " +
                            String(excludedBSSIDs.size()) + " excluded BSSIDs, " +
                            String(geofenceBoxes.size()) + " geofence boxes");
    }
}

bool GeofenceFilter::isSSIDExcluded(const String &ssid)
{
    for (const String &excluded : excludedSSIDs)
    {
        if (ssid == excluded)
        {
            return true;
        }
    }
    return false;
}

bool GeofenceFilter::isBSSIDExcluded(const String &bssid)
{
    String bssidLower = bssid;
    bssidLower.toLowerCase();

    for (const String &excluded : excludedBSSIDs)
    {
        String excludedLower = excluded;
        excludedLower.toLowerCase();
        if (bssidLower == excludedLower)
        {
            return true;
        }
    }
    return false;
}

bool GeofenceFilter::isInsideGeofence(double lat, double lon)
{
    for (const GeofenceBox &box : geofenceBoxes)
    {
        // AABB collision check
        // topLat >= lat >= bottomLat  AND  leftLon <= lon <= rightLon
        float minLat = min(box.topLat, box.bottomLat);
        float maxLat = max(box.topLat, box.bottomLat);
        float minLon = min(box.leftLon, box.rightLon);
        float maxLon = max(box.leftLon, box.rightLon);

        if (lat >= minLat && lat <= maxLat && lon >= minLon && lon <= maxLon)
        {
            return true;
        }
    }
    return false;
}

bool GeofenceFilter::isAccuracyAcceptable(float accuracyMeters)
{
    return accuracyMeters <= accuracyThreshold;
}

bool GeofenceFilter::shouldLog(const WiFiNetwork &net, const GPSData &gps)
{
    // Check SSID exclusion
    if (isSSIDExcluded(net.ssid))
        return false;

    // Check BSSID exclusion
    if (isBSSIDExcluded(net.bssid))
        return false;

    // Check geofence — if inside any geofence box, suppress logging
    if (gps.isValid && isInsideGeofence(gps.lat, gps.lng))
        return false;

    // Check accuracy
    if (!isAccuracyAcceptable(gps.accuracy))
        return false;

    return true;
}
