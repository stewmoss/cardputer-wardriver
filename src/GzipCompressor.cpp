#include "GzipCompressor.h"
#include "Logger.h"

#include <ESP32-targz.h>

namespace
{
// Single-pass scan of the source file to capture the original byte count and
// the WiGLE record count (newlines minus the two header rows). LZPacker does
// not expose either value, so we compute them once here before compressing.
struct SourceStats
{
    bool success;
    uint32_t bytes;
    uint32_t records;
};

SourceStats scanSource(fs::FS &fs, const String &path)
{
    SourceStats stats = {false, 0, 0};

    File file = fs.open(path, FILE_READ);
    if (!file)
    {
        return stats;
    }

    uint8_t buffer[256];
    uint32_t newlineCount = 0;
    uint32_t totalBytes = 0;
    while (file.available())
    {
        int bytesRead = file.read(buffer, sizeof(buffer));
        if (bytesRead <= 0)
        {
            file.close();
            return stats;
        }
        totalBytes += (uint32_t)bytesRead;
        for (int index = 0; index < bytesRead; ++index)
        {
            if (buffer[index] == '\n')
            {
                newlineCount++;
            }
        }
    }
    file.close();

    stats.success = true;
    stats.bytes = totalBytes;
    stats.records = (newlineCount > 2) ? (newlineCount - 2) : 0;
    return stats;
}
} // namespace

GzipCompressor::GzipCompressor()
{
}

GzipCompressor::Result GzipCompressor::compress(fs::FS &fs, const String &srcPath, const String &dstPath)
{
    Result result = {false, 0, 0, 0, ""};

    SourceStats stats = scanSource(fs, srcPath);
    if (!stats.success)
    {
        result.errorMessage = "Source open failed";
        return result;
    }

    result.bytesIn = stats.bytes;
    result.recordCount = stats.records;

    // Ensure no stale destination remains; we open both files ourselves and use
    // LZPacker's stream-to-stream overload because the library's file-to-file
    // overload has an inverted length check that rejects any non-empty source.
    fs.remove(dstPath);

    File src = fs.open(srcPath, FILE_READ);
    if (!src)
    {
        result.errorMessage = "Source open failed";
        return result;
    }

    File dst = fs.open(dstPath, FILE_WRITE);
    if (!dst)
    {
        src.close();
        result.errorMessage = "Destination open failed";
        return result;
    }

    size_t compressedSize = LZPacker::compress(&src, src.size(), &dst);

    dst.flush();
    dst.close();
    src.close();

    if (compressedSize == 0)
    {
        fs.remove(dstPath);
        result.errorMessage = "Gzip compression failed";
        logger.debugPrintln(String("[Upload] gzip failed: ") + result.errorMessage);
        return result;
    }

    result.bytesOut = (uint32_t)compressedSize;
    result.success = true;
    return result;
}
