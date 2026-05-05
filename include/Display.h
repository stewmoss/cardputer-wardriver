#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <vector>
#include "Config.h"

class Display
{
public:
    enum class ExitMode : uint8_t
    {
        RETURN,             // Continue normal flow on exit
        REBOOT,             // Reboot the device on exit
        RETURN_TO_SHUTDOWN  // Return to the locked shutdown summary on exit
    };

    Display();
    void begin();
    void showStartup(const char *modelName = nullptr, const char *source = nullptr);
    void showConfigMode(const String &ipAddress);
    void showSearchingSats(const GPSData &gps, int uniqueAPs, int lastScanCount);
    void showMonitoring();
    void nextView();
    DisplayView getCurrentView();
    void clearScreen();
    void showError(const String &msg);
    void showShutdownLocked(const SessionSummary &summary,
                            bool lowBattery,
                            bool uploadAvailable);
    void showLowBatteryWarning(int pct, int secondsRemaining);
    void setBatteryLevel(int pct);

    // Auto-upload screens
    void showUploadScanning(const String &ssid);
    void showUploadCountdown(int pendingFiles, int secondsRemaining);
    void showUploadLowBattery(int batteryPct, int secondsRemaining);
    void showUploadProgress(const String &filename,
                            uint32_t fileIndex,
                            uint32_t totalFiles,
                            uint32_t bytesSent,
                            uint32_t bytesTotal,
                            uint32_t elapsedSec);
    void showUploadComplete(const UploadStats &stats,
                            bool retryAvailable,
                            ExitMode exitMode = ExitMode::RETURN);
    void showUploadError(const String &message, ExitMode exitMode = ExitMode::RETURN);
    void showUploadSweepScanning(const String &filename, uint32_t fileIndex, uint32_t totalFiles);
    void showUploadCancelled();

    // Display blank / unblank
    void toggleBlank(int savedBrightness);
    void ensureUnblanked(int savedBrightness);
    bool isBlank() { return _isBlank; }

    // Help overlay
    void showHelp();
    void dismissHelp();
    bool isHelpVisible() { return _isHelpVisible; }

    // Dashboard updates
    void updateDashboardA(const GPSData &gps, const String &filename,
                          int uniqueAPs, uint32_t totalLogged,
                          unsigned long uptimeMs, float accuracyThreshold,
                          bool scanStopped = false, bool loggingPaused = false,
                          const String &gpsLogMode = "fix_only");
    void updateDashboardB(const SecurityStats &stats,
                          const std::vector<WiFiNetwork> &topNetworks,
                          int visibleCount = 0);
    void updateDashboardC(const std::vector<RecentEntry> &recent);
    void updateDashboardD(const std::vector<RecentEntry> &recentUnique);
    void updateDashboardE(const uint32_t sweepCounts[13],
                          const uint32_t sessionCounts[13],
                          const uint32_t uniqueCounts[13],
                          uint8_t viewMode);
    void updateDashboardF(bool soundMuted,
                          const char *modelName = nullptr,
                          const char *source = nullptr);

private:
    DisplayView currentView;
    DisplayView _lastDrawnView;
    bool _isBlank;
    bool _isHelpVisible;
    int _batteryLevel = -1;

    // ── Incremental-update caches ───────────────────────────────────────
    struct SearchingSatsCache
    {
        String satCount;
        String fixText;
        String accuracy;
        String uniqueAPs;
        String lastScan;
    };

    struct DashACache
    {
        String headerText;
        String uniqueAPs;
        String totalLogged;
        String uptime;
        String accuracy;
        String lat;
        String lon;
        String alt;
        String localTime;
    };

    struct DashBCache
    {
        String openCount;
        String wpa2Count;
        String wpa3Count;
        String otherCount;
        String rows[3];
        String visibleCount;
    };

    struct DashAOverlay
    {
        bool fixLost;
        bool scanStopped;
        bool loggingPaused;
    };

    SearchingSatsCache _searchCache;
    DashACache _dashACache;
    DashBCache _dashBCache;
    DashAOverlay _dashAOverlay;

    struct DashECache
    {
        uint16_t barHeights[13];
        uint16_t barColors[13];
        uint32_t maxY;
        uint8_t viewMode;
    };

    DashECache _dashECache;

    struct UploadCache
    {
        String screen;
        String line1;
        String line2;
        String line3;
        uint32_t bytesSent;
        uint32_t bytesTotal;
        uint32_t fileIndex;
        uint32_t totalFiles;
        uint32_t elapsedSec;
        bool retryAvailable;
    };

    UploadCache _uploadCache;

    void _resetCaches();
    void _drawField(const String &oldVal, const String &newVal,
                    int x, int y, uint16_t fg, uint8_t textSize = 1);

    void drawHeader(const String &title);
    void drawUploadHeader(const String &title, uint16_t color);
    void drawFooter(const String &status);
    String formatUptime(unsigned long ms);
    String formatDuration(uint32_t seconds);
    String formatBytes(uint64_t bytes);
    String truncateSSID(const String &ssid, int maxLen);
};

#endif // DISPLAY_H
