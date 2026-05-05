#ifndef GZIP_COMPRESSOR_H
#define GZIP_COMPRESSOR_H

#include <Arduino.h>
#include <FS.h>
#include "Config.h"

class GzipCompressor
{
public:
    GzipCompressor();

    struct Result
    {
        bool success;
        uint32_t bytesIn;
        uint32_t bytesOut;
        uint32_t recordCount;
        String errorMessage;
    };

    Result compress(fs::FS &fs, const String &srcPath, const String &dstPath);
};

#endif // GZIP_COMPRESSOR_H
