#include "DataLogger.h"
#include "CsvFileMaintenance.h"
#include "WiFiScanner.h"
#include "Logger.h"
#include <SD.h>

// RFC 4180 field escaping: wrap in double quotes, escape internal quotes
static String csvEscape(const String &field)
{
    String escaped = field;
    escaped.replace("\"", "\"\"");
    return "\"" + escaped + "\"";
}

DataLogger::DataLogger()
    : filesystem(nullptr), gpsManager(nullptr), totalLogged(0), fileOpen(false)
{
}

DataLogger::~DataLogger()
{
}

bool DataLogger::begin(fs::FS &fs)
{
    filesystem = &fs;

    // Ensure directory exists
    if (!fs.exists(CSV_DIR))
    {
        if (!fs.mkdir(CSV_DIR))
        {
            logger.debugPrintln("[DataLogger] Failed to create CSV directory");
            return false;
        }
    }

    CsvFileMaintenance::cleanupStaleZeroBytePlaceholders(fs);

    // Find next available file index
    int idx = findNextFileIndex();
    char filename[64];
    snprintf(filename, sizeof(filename), "%s/%s%04d.csv", CSV_DIR, CSV_PREFIX, idx);
    activeFilename = String(filename);

   if (!openLogFile()) {
     return false;
   }
    totalLogged = 0;

    // Write WiGLE headers
    writePreHeader();
    writeCSVHeader();
    logFile.flush();

    logger.debugPrintln(String("[DataLogger] Logging to: ") + activeFilename);
    return true;
}


bool DataLogger::openLogFile(){
        // Open file for writing
    logFile = filesystem->open(activeFilename, FILE_APPEND);
    if (!logFile)
    {
        logger.debugPrintln(String("[DataLogger] Failed to open: ") + activeFilename);
        return false;
    }

    fileOpen = true;
    return true;
}


void DataLogger::logEntry(const WiFiNetwork &net, const GPSData &gps)
{
    if (!fileOpen)
        return;

    String capabilities = WiFiScanner::authModeToCapabilities(net.authMode);

    // Format numeric fields with fixed precision
    char latStr[20], lonStr[20], altStr[12], accStr[12];
    snprintf(latStr, sizeof(latStr), "%.8f", gps.lat);
    snprintf(lonStr, sizeof(lonStr), "%.8f", gps.lng);
    snprintf(altStr, sizeof(altStr), "%.1f", gps.altitude);
    snprintf(accStr, sizeof(accStr), "%.2f", gps.accuracy);

    // Build RFC 4180 compliant CSV row — every field quoted, internal " escaped
    String row;
    row.reserve(300);
    row += csvEscape(net.bssid); // MAC
    row += ',';
    row += csvEscape(net.ssid); // SSID
    row += ',';
    row += csvEscape(capabilities); // AuthMode
    row += ',';
    String firstSeen = (gpsManager != nullptr) ? gpsManager->getFormattedDateTime(true) : gps.dateTime;
    row += csvEscape(firstSeen); // FirstSeen
    row += ',';
    row += (String(net.channel)); // Channel
    row += ',';
    row += (String(net.frequency)); // Frequency
    row += ',';
    row += (String(net.rssi)); // RSSI
    row += ',';
    row += (String(latStr)); // CurrentLatitude
    row += ',';
    row += (String(lonStr)); // CurrentLongitude
    row += ',';
    row += (String(altStr)); // AltitudeMeters
    row += ',';
    row += (String(accStr)); // AccuracyMeters
    row += ',';
    row += (""); // RCOIs
    row += ',';
    row += (""); // MfgrId
    row += ',';
    row += ("WIFI"); // Type
    row += '\n';

    logFile.write((uint8_t *)row.c_str(), row.length());
    totalLogged++;
}

void DataLogger::logFlagEntry(const GPSData &gps)
{
    if (!fileOpen)
        return;

    // Format numeric fields with fixed precision
    char latStr[20], lonStr[20], altStr[12], accStr[12];
    snprintf(latStr, sizeof(latStr), "%.8f", gps.lat);
    snprintf(lonStr, sizeof(lonStr), "%.8f", gps.lng);
    snprintf(altStr, sizeof(altStr), "%.1f", gps.altitude);
    snprintf(accStr, sizeof(accStr), "%.2f", gps.accuracy);

    // Build FLAG marker CSV row — non-real AP marker entry
    String row;
    row.reserve(300);
    row += csvEscape("00:00:00:00:00:00"); // MAC — clearly non-real
    row += ',';
    row += csvEscape("FLAG"); // SSID
    row += ',';
    row += csvEscape("[FLAG]"); // AuthMode — flag marker
    row += ',';
    String firstSeen = (gpsManager != nullptr) ? gpsManager->getFormattedDateTime(true) : gps.dateTime;
    row += csvEscape(firstSeen); // FirstSeen
    row += ',';
    row += ("0"); // Channel
    row += ',';
    row += ("0"); // Frequency
    row += ',';
    row += ("0"); // RSSI
    row += ',';
    row += (String(latStr)); // CurrentLatitude
    row += ',';
    row += (String(lonStr)); // CurrentLongitude
    row += ',';
    row += (String(altStr)); // AltitudeMeters
    row += ',';
    row += (String(accStr)); // AccuracyMeters
    row += ',';
    row += (""); // RCOIs
    row += ',';
    row += (""); // MfgrId
    row += ',';
    row += ("FLAG"); // Type
    row += '\n';

    logFile.write((uint8_t *)row.c_str(), row.length());
    totalLogged++;
}

void DataLogger::flush()
{
    if (!fileOpen)
        return;

    logFile.flush();
}

void DataLogger::close()
{
    if (!fileOpen)
        return;

    logFile.flush();
    logFile.close();
    fileOpen = false;
    logger.debugPrintln(String("[DataLogger] Closed file. Total logged: ") + String(totalLogged));
}

String DataLogger::getActiveFilename()
{
    return activeFilename;
}

uint32_t DataLogger::getTotalLogged()
{
    return totalLogged;
}

bool DataLogger::isOpen()
{
    return fileOpen;
}

int DataLogger::findNextFileIndex()
{
    int maxIdx = 0;

    File dir = filesystem->open(CSV_DIR);
    if (!dir || !dir.isDirectory())
    {
        return 1;
    }

    File entry;
    while ((entry = dir.openNextFile()))
    {
        String name = entry.name();
        int slash = name.lastIndexOf('/');
        if (slash >= 0 && slash + 1 < (int)name.length())
        {
            name = name.substring(slash + 1);
        }
        // Look for files matching wardriving_NNN.csv pattern
        if (name.startsWith(CSV_PREFIX) && name.endsWith(".csv"))
        {
            // Extract the number portion
            String numStr = name.substring(strlen(CSV_PREFIX));
            numStr = numStr.substring(0, numStr.length() - 4); // remove .csv
            bool validIndex = numStr.length() > 0;
            for (size_t index = 0; index < numStr.length(); ++index)
            {
                char digit = numStr.charAt(index);
                if (digit < '0' || digit > '9')
                {
                    validIndex = false;
                    break;
                }
            }
            if (validIndex)
            {
                int idx = numStr.toInt();
                if (idx > maxIdx)
                {
                    maxIdx = idx;
                }
            }
        }
        entry.close();
    }
    dir.close();

    return maxIdx + 1;
}

void DataLogger::writePreHeader()
{
    char preheader[256];
    snprintf(preheader, sizeof(preheader), WIGLE_PREHEADER_FMT, FIRMWARE_VERSION);
    String line = String(preheader) + "\n";
    logFile.write((uint8_t *)line.c_str(), line.length());
}

void DataLogger::writeCSVHeader()
{
    String line = String(WIGLE_CSV_HEADER) + "\n";
    logFile.write((uint8_t *)line.c_str(), line.length());
}


