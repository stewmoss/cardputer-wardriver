#ifndef UPLOAD_MANAGER_H
#define UPLOAD_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <FS.h>
#include "Config.h"
#include "Display.h"
#include "AlertManager.h"
#include "WiFiManager.h"
#include "WiGLEUploader.h"
#include "GzipCompressor.h"

class UploadManager
{
public:
    enum class Mode : uint8_t
    {
        AUTO,           // Boot-time auto-upload
        MANUAL_REBOOT,  // Manual upload that reboots the device on exit
        MANUAL_SHUTDOWN // Manual upload triggered from the shutdown screen
    };

    UploadManager();

    void begin(Display *display,
               AlertManager *alerts,
               WiFiManager *wifi,
               const UploadConfig *uploadCfg,
               const ScanConfig *scanCfg,
               bool superDebug);
    bool shouldRun() const;
    void enter(Mode m = Mode::AUTO);
    bool tick();
    void requestCancel();
    bool isCancelRequested() const { return cancelRequested; }
    Mode getMode() const { return mode; }
    bool isManual() const { return mode != Mode::AUTO; }
    bool isShutdownMode() const { return mode == Mode::MANUAL_SHUTDOWN; }

private:
    enum class UploadPhase
    {
        SCAN_FOR_HOME,
        FOUND_COUNTDOWN,
        LOW_BATTERY_PROMPT,
        UPLOADING,
        COMPLETE_SUMMARY,
        ERROR_SCREEN,
        DONE
    };

    void phaseScanForHome();
    void phaseFoundCountdown();
    void phaseLowBatteryPrompt();
    void phaseUploading();
    void phaseCompleteSummary();
    void phaseErrorScreen();

    std::vector<String> discoverPendingFiles();
    bool moveOrDeleteAfterUpload(const String &csvPath,
                                 const String &uploadedPath = String(),
                                 const String &uploadedFilename = String());
    bool ensureDirectories();
    bool ensureThinDir();
    bool moveToThin(const String &csvPath);
    void purgeTempDir();
    bool connectHomeNetwork();
    void disconnectHomeNetwork();
    void doubleBeep();
    void cancelBeep();
    void writePlaceholderFor(const String &uploadedFilename);
    void recomputeStats();
    void setupRetry();

    bool countSweepsUpToThreshold(const String &csvPath, uint16_t threshold, uint16_t &sweepsFound);
    static String extractFirstSeenField(const String &line);

    UploadPhase phase;
    bool cancelRequested;
    bool scanAttempted;
    bool uploadConnected;
    Mode mode;
    bool rebootOnExit;
    uint32_t phaseStartMs;
    uint32_t lastProgressDrawMs;
    UploadStats stats;
    std::vector<String> pendingFiles;
    std::vector<UploadFileResult> results;
    size_t currentIndex;
    String lastErrorMessage;
    String maxIndexPath;
    bool retryMode;

    Display *display;
    AlertManager *alerts;
    WiFiManager *wifi;
    const UploadConfig *cfg;
    const ScanConfig *scanCfg;
    WiGLEUploader uploader;
    GzipCompressor compressor;
    bool superDebug;
};

#endif // UPLOAD_MANAGER_H
