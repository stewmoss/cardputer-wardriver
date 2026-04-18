#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <vector>
#include "Config.h"

class Display
{
public:
    Display();
    void begin();
    void showStartup();
    void showConfigMode(const String &ipAddress);
    void showSearchingSats(const GPSData &gps, int uniqueAPs, int lastScanCount);
    void showMonitoring();
    void nextView();
    DisplayView getCurrentView();
    void clearScreen();
    void showError(const String &msg);
    void showShutdown(const SessionSummary &summary, bool lowBattery = false);
    void showLowBatteryWarning(int pct, int secondsRemaining);
    void setBatteryLevel(int pct);

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
    void updateDashboardE(const uint16_t sweepCounts[13],
                          const uint16_t sessionCounts[13],
                          const uint16_t uniqueCounts[13],
                          uint8_t viewMode);
    void updateDashboardF(bool soundMuted);

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
        uint16_t maxY;
        uint8_t viewMode;
    };

    DashECache _dashECache;

    void _resetCaches();
    void _drawField(const String &oldVal, const String &newVal,
                    int x, int y, uint16_t fg, uint8_t textSize = 1);

    void drawHeader(const String &title);
    void drawFooter(const String &status);
    String formatUptime(unsigned long ms);
    String truncateSSID(const String &ssid, int maxLen);
};

#endif // DISPLAY_H
