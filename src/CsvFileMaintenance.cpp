#include "CsvFileMaintenance.h"
#include "Logger.h"
#include <string.h>

bool CsvFileMaintenance::superDebug = false;

void CsvFileMaintenance::setSuperDebug(bool enabled)
{
    superDebug = enabled;
}

String CsvFileMaintenance::basenameOf(const String &path)
{
    int slash = path.lastIndexOf('/');
    if (slash >= 0 && slash + 1 < (int)path.length())
    {
        return path.substring(slash + 1);
    }
    return path;
}

int CsvFileMaintenance::parseWardrivingCsvIndex(const String &filename)
{
    if (!filename.startsWith(CSV_PREFIX) || !filename.endsWith(".csv"))
    {
        return CSV_INDEX_NONE;
    }

    String numeric = filename.substring(strlen(CSV_PREFIX), filename.length() - 4);
    if (numeric.length() == 0)
    {
        return CSV_INDEX_NONE;
    }

    for (size_t index = 0; index < numeric.length(); ++index)
    {
        char digit = numeric.charAt(index);
        if (digit < '0' || digit > '9')
        {
            return CSV_INDEX_NONE;
        }
    }

    return numeric.toInt();
}

CsvPlaceholderCleanupResult CsvFileMaintenance::cleanupStaleZeroBytePlaceholders(fs::FS &fs)
{
    CsvPlaceholderCleanupResult result = {
        false,
        0,
        0,
        CSV_INDEX_NONE,
        CSV_INDEX_NONE,
        false};

    bool highestIsZeroByte = false;

    if (superDebug) logger.debugPrintln(String("[CSV] cleanupStaleZeroBytePlaceholders: starting, dir=") + CSV_DIR);

    // ── Pass 1: scan directory to find highest index and count zero-byte files ──
    File dir = fs.open(CSV_DIR);
    if (!dir)
    {
        logger.debugPrintln("[CSV] cleanup pass1: fs.open() returned null");
        return result;
    }
    if (!dir.isDirectory())
    {
        logger.debugPrintln(String("[CSV] cleanup pass1: ") + CSV_DIR + " is not a directory");
        dir.close();
        return result;
    }
    if (superDebug) logger.debugPrintln("[CSV] cleanup pass1: directory opened OK, scanning entries...");

    File entry;
    int scannedTotal = 0;
    while ((entry = dir.openNextFile()))
    {
        scannedTotal++;
        String rawName = entry.name();
        bool isDir = entry.isDirectory();
        size_t fileSize = isDir ? 0 : entry.size();
        entry.close();

        if (isDir)
        {
            if (superDebug) logger.debugPrintln(String("[CSV] cleanup pass1:   skip dir  name=") + rawName);
            continue;
        }

        String filename = basenameOf(rawName);
        int fileIndex = parseWardrivingCsvIndex(filename);
        bool isZeroByte = fileSize == 0;

        if (superDebug)
        {
            logger.debugPrintln(String("[CSV] cleanup pass1:   file=") + filename +
                                " size=" + String(fileSize) +
                                " index=" + String(fileIndex) +
                                " zero=" + String(isZeroByte ? 1 : 0));
        }

        if (fileIndex != CSV_INDEX_NONE)
        {
            if (isZeroByte)
            {
                result.zeroByteSeen++;
            }
            if (fileIndex > result.highestIndex)
            {
                result.highestIndex = fileIndex;
                highestIsZeroByte = isZeroByte;
                if (superDebug)
                {
                    logger.debugPrintln(String("[CSV] cleanup pass1:   new highest index=") +
                                        String(fileIndex) + " zeroByte=" + String(isZeroByte ? 1 : 0));
                }
            }
        }
        else
        {
            if (superDebug) logger.debugPrintln(String("[CSV] cleanup pass1:   skip (not a wardriving CSV): ") + filename);
        }
    }
    dir.close();

    if (superDebug)
    {
        logger.debugPrintln(String("[CSV] cleanup pass1 done: scanned=") + String(scannedTotal) +
                            " zeroByteSeen=" + String(result.zeroByteSeen) +
                            " highestIndex=" + String(result.highestIndex) +
                            " highestIsZeroByte=" + String(highestIsZeroByte ? 1 : 0));
    }

    if (result.highestIndex != CSV_INDEX_NONE && highestIsZeroByte)
    {
        result.retainedIndex = result.highestIndex;
        result.retainedPlaceholder = true;
        if (superDebug) logger.debugPrintln(String("[CSV] cleanup: will retain placeholder index=") + String(result.retainedIndex));
    }
    else if (result.highestIndex == CSV_INDEX_NONE)
    {
        if (superDebug) logger.debugPrintln("[CSV] cleanup: no wardriving CSVs found at all — nothing to do");
        result.success = true;
        return result;
    }
    else
    {
        if (superDebug) logger.debugPrintln("[CSV] cleanup: highest CSV is non-empty, no placeholder retained");
    }

    // ── Pass 2: remove stale zero-byte placeholders ──
    if (superDebug) logger.debugPrintln("[CSV] cleanup pass2: re-opening directory to remove stale placeholders...");

    dir = fs.open(CSV_DIR);
    if (!dir)
    {
        logger.debugPrintln("[CSV] cleanup pass2: fs.open() returned null on reopen");
        return result;
    }
    if (!dir.isDirectory())
    {
        logger.debugPrintln(String("[CSV] cleanup pass2: ") + CSV_DIR + " is not a directory on reopen");
        dir.close();
        return result;
    }
    if (superDebug) logger.debugPrintln("[CSV] cleanup pass2: directory reopened OK, scanning for removal...");

    while ((entry = dir.openNextFile()))
    {
        String rawName = entry.name();
        bool isDir = entry.isDirectory();
        size_t fileSize = isDir ? 0 : entry.size();
        String filename = basenameOf(rawName);
        int fileIndex = parseWardrivingCsvIndex(filename);
        String path = String(CSV_DIR) + "/" + filename;
        entry.close();

        if (isDir)
        {
            continue;
        }

        bool removePlaceholder = fileIndex != CSV_INDEX_NONE &&
                                 fileSize == 0 &&
                                 fileIndex != result.retainedIndex;

        if (superDebug)
        {
            logger.debugPrintln(String("[CSV] cleanup pass2:   file=") + filename +
                                " index=" + String(fileIndex) +
                                " size=" + String(fileSize) +
                                " retainedIdx=" + String(result.retainedIndex) +
                                " willRemove=" + String(removePlaceholder ? 1 : 0));
        }

        if (removePlaceholder)
        {
            bool removed = fs.remove(path);
            if (superDebug)
            {
                logger.debugPrintln(String("[CSV] cleanup pass2:   remove ") + path +
                                    " -> " + String(removed ? "OK" : "FAILED"));
            }
            if (!removed)
            {
                logger.debugPrintln(String("[CSV] cleanup pass2:   remove FAILED: ") + path);
            }
            if (removed)
            {
                result.zeroByteRemoved++;
            }
        }
    }
    dir.close();

    result.success = true;
    if (superDebug)
    {
        logger.debugPrintln(String("[CSV] cleanupStaleZeroBytePlaceholders done: success=1") +
                            " zeroByteSeen=" + String(result.zeroByteSeen) +
                            " zeroByteRemoved=" + String(result.zeroByteRemoved) +
                            " retainedIndex=" + String(result.retainedIndex) +
                            " highestIndex=" + String(result.highestIndex));
    }

    return result;
}
