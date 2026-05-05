#ifndef WIGLE_UPLOADER_H
#define WIGLE_UPLOADER_H

#include <Arduino.h>
#include <FS.h>
#include <functional>

class WiGLEUploader
{
public:
    WiGLEUploader(bool superDebug);

    // Return false from the callback to abort the active upload.
    typedef std::function<bool(uint32_t, uint32_t)> ProgressCb;

    void setSuperDebug(bool enabled);
    void setCredentials(const String &apiName, const String &apiToken);
    int uploadFile(fs::FS &fs,
                   const String &localPath,
                   const String &remoteFilename,
                   const char *mimeType,
                   ProgressCb progress);
    String getLastErrorMessage() const;

private:
    String apiName;
    String apiToken;
    String authHeaderCached;
    String lastError;

    void rebuildAuthHeader();
    String mapHttpCodeToMessage(int httpCode);
    bool superDebug;
};

#endif // WIGLE_UPLOADER_H
