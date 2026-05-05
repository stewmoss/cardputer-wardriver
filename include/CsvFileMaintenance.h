#ifndef CSV_FILE_MAINTENANCE_H
#define CSV_FILE_MAINTENANCE_H

#include <Arduino.h>
#include <FS.h>
#include "Config.h"

class CsvFileMaintenance
{
public:
    static void setSuperDebug(bool enabled);
    static CsvPlaceholderCleanupResult cleanupStaleZeroBytePlaceholders(fs::FS &fs);

private:
    static bool superDebug;
    static String basenameOf(const String &path);
    static int parseWardrivingCsvIndex(const String &filename);
};

#endif // CSV_FILE_MAINTENANCE_H
