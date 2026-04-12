#include "Display.h"
#include <M5Cardputer.h>

// Colour shortcuts
#define COL_HEADER 0x001F // BLUE
#define COL_BG 0x0000     // BLACK
#define COL_TEXT 0xFFFF   // WHITE
#define COL_GREEN 0x07E0
#define COL_YELLOW 0xFFE0
#define COL_RED 0xF800
#define COL_CYAN 0x07FF
#define COL_ORANGE 0xFDA0
#define VIEW_NONE ((DisplayView) - 1)

Display::Display()
    : currentView(VIEW_DASHBOARD_A), _lastDrawnView(VIEW_NONE), _isBlank(false), _isHelpVisible(false), _batteryLevel(-1), _dashAOverlay({false, false, false})
{
}

void Display::begin()
{
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);
    _resetCaches();
    clearScreen();
}

void Display::showStartup()
{
    clearScreen();

    // Title
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setTextColor(COL_CYAN, COL_BG);
    M5Cardputer.Display.setCursor(30, 30);
    M5Cardputer.Display.print("CARDPUTER WARDRIVER");

    // Version
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);
    M5Cardputer.Display.setCursor(60, 60);
    M5Cardputer.Display.print("Firmware v" FIRMWARE_VERSION);

    // Init message
    M5Cardputer.Display.setCursor(55, 85);
    M5Cardputer.Display.setTextColor(COL_YELLOW, COL_BG);
    M5Cardputer.Display.print("Initializing...");
    M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);

    // Hint
    M5Cardputer.Display.setCursor(15, 115);
    M5Cardputer.Display.setTextColor(0x7BEF, COL_BG); // grey
    M5Cardputer.Display.print("Hold G0 for Config Mode");
    M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);
}

void Display::showShutdown(const SessionSummary &summary, bool lowBattery)
{
    clearScreen();

    // Red header bar
    M5Cardputer.Display.fillRect(0, 0, SCREEN_WIDTH, 20, COL_RED);
    M5Cardputer.Display.setTextColor(COL_TEXT, COL_RED);
    M5Cardputer.Display.setCursor(5, 5);
    M5Cardputer.Display.print("SHUTDOWN SUMMARY");
    M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);

    M5Cardputer.Display.setTextSize(1);
    const int leftX = 8;
    const int rightX = 126;

    auto drawMetric = [&](int x, int y, const char *label, const String &value)
    {
        M5Cardputer.Display.setTextColor(COL_CYAN, COL_BG);
        M5Cardputer.Display.setCursor(x, y);
        M5Cardputer.Display.print(label);

        M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);
        M5Cardputer.Display.setCursor(x, y + 9);
        M5Cardputer.Display.print(value);
    };

    drawMetric(leftX, 30, "Logged", String(summary.totalLogged));
    drawMetric(rightX, 30, "Seen", String((unsigned long long)summary.totalStationsSeen));
    drawMetric(leftX, 48, "Duration", formatUptime(summary.sessionDurationMs));
    drawMetric(rightX, 48, "Avg ms", String(summary.averageScanMs));
    drawMetric(leftX, 66, "Unique APs", String(summary.uniqueStations));
    drawMetric(leftX, 84, "Sweeps", String(summary.totalSweeps));
    drawMetric(rightX, 84, "Peak", String(summary.maxStationsPerSweep));

    M5Cardputer.Display.drawFastHLine(8, 106, SCREEN_WIDTH - 16, COL_RED);
    M5Cardputer.Display.setTextColor(COL_YELLOW, COL_BG);
    M5Cardputer.Display.setCursor(8, 114);
    if (lowBattery)
    {
        M5Cardputer.Display.print("Low battery shutdown complete");
    }
    else
    {
        M5Cardputer.Display.print("SD unmounted - safe to remove");
    }

    M5Cardputer.Display.setTextColor(0x7BEF, COL_BG); // grey
    M5Cardputer.Display.setCursor(10, 126);
    M5Cardputer.Display.print("G0=Reboot  B=Screen off");

    M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);
    M5Cardputer.Display.setTextSize(1);
}

void Display::setBatteryLevel(int pct)
{
    _batteryLevel = pct;
}

void Display::showLowBatteryWarning(int pct, int secondsRemaining)
{
    clearScreen();

    // Orange header
    M5Cardputer.Display.fillRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, COL_ORANGE);
    M5Cardputer.Display.setTextColor(COL_BG, COL_ORANGE);
    M5Cardputer.Display.setCursor(5, 5);
    M5Cardputer.Display.print("LOW BATTERY");
    M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);

    // Battery percentage — large centred
    M5Cardputer.Display.setTextSize(3);
    M5Cardputer.Display.setTextColor(COL_RED, COL_BG);
    char pctBuf[8];
    snprintf(pctBuf, sizeof(pctBuf), "%d%%", pct);
    int pctX = (SCREEN_WIDTH - (int)strlen(pctBuf) * 18) / 2;
    M5Cardputer.Display.setCursor(pctX < 0 ? 0 : pctX, 38);
    M5Cardputer.Display.print(pctBuf);

    // Countdown
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(COL_YELLOW, COL_BG);
    char cntBuf[32];
    snprintf(cntBuf, sizeof(cntBuf), "Shutting down in %ds", secondsRemaining);
    int cntX = (SCREEN_WIDTH - (int)strlen(cntBuf) * 6) / 2;
    M5Cardputer.Display.setCursor(cntX < 0 ? 0 : cntX, 95);
    M5Cardputer.Display.print(cntBuf);

    M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);
    M5Cardputer.Display.setTextSize(1);
}

void Display::showConfigMode(const String &ipAddress)
{
    clearScreen();
    drawHeader("CONFIG MODE");

    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);

    M5Cardputer.Display.setCursor(10, 30);
    M5Cardputer.Display.print("WiFi AP: " AP_SSID);

    M5Cardputer.Display.setCursor(10, 48);
    M5Cardputer.Display.print("IP: ");
    M5Cardputer.Display.print(ipAddress);

    M5Cardputer.Display.setCursor(10, 66);
    M5Cardputer.Display.setTextColor(COL_YELLOW, COL_BG);
    M5Cardputer.Display.print("Connect to WiFi AP and");

    M5Cardputer.Display.setCursor(10, 80);
    M5Cardputer.Display.print("open browser to configure");

    M5Cardputer.Display.setTextColor(0x7BEF, COL_BG);
    M5Cardputer.Display.setCursor(10, 105);
    M5Cardputer.Display.print("Press 'c' or G0 to exit");

    M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);
}

void Display::showSearchingSats(const GPSData &gps, int uniqueAPs, int lastScanCount)
{
    if (_isHelpVisible)
        return;

    // Base draw — static labels, drawn once per view entry
    if (_lastDrawnView != VIEW_SEARCHING_SATS)
    {
        clearScreen();
        drawHeader("SEARCHING SATS");

        M5Cardputer.Display.setTextSize(1);

        // Static labels — left column
        M5Cardputer.Display.setCursor(10, 28);
        M5Cardputer.Display.setTextColor(COL_YELLOW, COL_BG);
        M5Cardputer.Display.print("Satellites: ");

        M5Cardputer.Display.setCursor(10, 42);
        M5Cardputer.Display.setTextColor(COL_YELLOW, COL_BG);
        M5Cardputer.Display.print("Fix: ");

        M5Cardputer.Display.setCursor(10, 56);
        M5Cardputer.Display.setTextColor(COL_YELLOW, COL_BG);
        M5Cardputer.Display.print("Accur: ");

        // Static labels — right column
        M5Cardputer.Display.setCursor(130, 28);
        M5Cardputer.Display.setTextColor(COL_CYAN, COL_BG);
        M5Cardputer.Display.print("APs: ");

        M5Cardputer.Display.setCursor(130, 42);
        M5Cardputer.Display.setTextColor(COL_CYAN, COL_BG);
        M5Cardputer.Display.print("Scan: ");

        // Static waiting message
        M5Cardputer.Display.setCursor(10, 80);
        M5Cardputer.Display.setTextColor(COL_YELLOW, COL_BG);
        M5Cardputer.Display.print("Waiting for 3D fix to start");
        M5Cardputer.Display.setCursor(10, 94);
        M5Cardputer.Display.print("logging...");

        // Static hint
        M5Cardputer.Display.setTextColor(0x7BEF, COL_BG);
        M5Cardputer.Display.setCursor(10, 115);
        M5Cardputer.Display.print("ENT=Views  H = Help");

        M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);

        _resetCaches();
        _lastDrawnView = VIEW_SEARCHING_SATS;
    }

    // Format current values
    String satCount = String(gps.satellites);
    String fixText = gps.has3DFix ? "3D" : (gps.isValid ? "2D" : "No Fix");
    uint16_t fixColor = gps.has3DFix ? COL_GREEN : (gps.isValid ? COL_YELLOW : COL_RED);
    String accuracy = String(gps.accuracy, 1);
    String aps = String(uniqueAPs);
    String scan = String(lastScanCount);

    // Incremental field updates
    _drawField(_searchCache.satCount, satCount, 82, 28, COL_TEXT);
    _drawField(_searchCache.fixText, fixText, 40, 42, fixColor);
    _drawField(_searchCache.accuracy, accuracy, 94, 56, COL_TEXT);
    _drawField(_searchCache.uniqueAPs, aps, 160, 28, COL_TEXT);
    _drawField(_searchCache.lastScan, scan, 166, 42, COL_TEXT);

    // Update cache
    _searchCache.satCount = satCount;
    _searchCache.fixText = fixText;
    _searchCache.accuracy = accuracy;
    _searchCache.uniqueAPs = aps;
    _searchCache.lastScan = scan;
}

void Display::showMonitoring()
{
    clearScreen();
    drawHeader("WARDRIVING");
}

void Display::nextView()
{
    if (currentView == VIEW_DASHBOARD_A)
    {
        currentView = VIEW_DASHBOARD_B;
    }
    else if (currentView == VIEW_DASHBOARD_B)
    {
        currentView = VIEW_DASHBOARD_C;
    }
    else if (currentView == VIEW_DASHBOARD_C)
    {
        currentView = VIEW_DASHBOARD_D;
    }
    else if (currentView == VIEW_DASHBOARD_D)
    {
        currentView = VIEW_DASHBOARD_E;
    }
    else
    {
        currentView = VIEW_DASHBOARD_A;
    }
    _lastDrawnView = VIEW_NONE;
}

DisplayView Display::getCurrentView()
{
    return currentView;
}

void Display::clearScreen()
{
    M5Cardputer.Display.fillScreen(COL_BG);
}

void Display::_resetCaches()
{
    const String S = "\x01";

    _searchCache.satCount = _searchCache.fixText = _searchCache.accuracy =
        _searchCache.uniqueAPs = _searchCache.lastScan = S;

    _dashACache.headerText = _dashACache.uniqueAPs = _dashACache.totalLogged =
        _dashACache.uptime = _dashACache.accuracy = _dashACache.lat =
            _dashACache.lon = _dashACache.alt = S;

    _dashBCache.openCount = _dashBCache.wpa2Count = _dashBCache.wpa3Count =
        _dashBCache.otherCount = _dashBCache.visibleCount = S;
    for (int i = 0; i < 3; i++)
        _dashBCache.rows[i] = S;

    _dashAOverlay = {false, false, false};
}

void Display::_drawField(const String &oldVal, const String &newVal,
                         int x, int y, uint16_t fg, uint8_t textSize)
{
    if (oldVal == newVal)
        return;

    M5Cardputer.Display.setTextSize(textSize);

    // Erase old value by overwriting in background colour
    M5Cardputer.Display.setTextColor(COL_BG, COL_BG);
    M5Cardputer.Display.setCursor(x, y);
    M5Cardputer.Display.print(oldVal);

    // Draw new value in foreground colour
    M5Cardputer.Display.setTextColor(fg, COL_BG);
    M5Cardputer.Display.setCursor(x, y);
    M5Cardputer.Display.print(newVal);
}

void Display::showError(const String &msg)
{
    M5Cardputer.Display.fillScreen(COL_RED);
    M5Cardputer.Display.setTextSize(2);
    M5Cardputer.Display.setTextColor(COL_TEXT, COL_RED);
    M5Cardputer.Display.setCursor(10, 30);
    M5Cardputer.Display.print("ERROR!");
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setCursor(10, 65);
    M5Cardputer.Display.print(msg);
    M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);
}

void Display::updateDashboardA(const GPSData &gps, const String &filename,
                               int uniqueAPs, uint32_t totalLogged,
                               unsigned long uptimeMs, float accuracyThreshold,
                               bool scanStopped, bool loggingPaused,
                               const String &gpsLogMode)
{
    if (_isHelpVisible)
        return;

    // Base draw — static labels, drawn once per view entry
    if (_lastDrawnView != VIEW_DASHBOARD_A)
    {
        clearScreen();

        // Header background bar (content drawn below as a cached field)
        M5Cardputer.Display.fillRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, COL_HEADER);

        M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);
        M5Cardputer.Display.setTextSize(1);

        // Static "Unique APs" label (below the large number)
        M5Cardputer.Display.setCursor(10, 48);
        M5Cardputer.Display.print("Unique APs");

        // Static labels — right column
        M5Cardputer.Display.setCursor(130, 28);
        M5Cardputer.Display.setTextColor(COL_CYAN, COL_BG);
        M5Cardputer.Display.print("Logged: ");

        M5Cardputer.Display.setCursor(130, 42);
        M5Cardputer.Display.setTextColor(COL_CYAN, COL_BG);
        M5Cardputer.Display.print("Up: ");

        M5Cardputer.Display.setCursor(130, 56);
        M5Cardputer.Display.setTextColor(COL_CYAN, COL_BG);
        M5Cardputer.Display.print("Accur: ");

        // Static labels — position
        M5Cardputer.Display.setCursor(10, 68);
        M5Cardputer.Display.setTextColor(COL_YELLOW, COL_BG);
        M5Cardputer.Display.print("Lat: ");

        M5Cardputer.Display.setCursor(10, 82);
        M5Cardputer.Display.setTextColor(COL_YELLOW, COL_BG);
        M5Cardputer.Display.print("Lon: ");

        M5Cardputer.Display.setCursor(10, 96);
        M5Cardputer.Display.setTextColor(COL_YELLOW, COL_BG);
        M5Cardputer.Display.print("Alt: ");

        // Static label — local time
        M5Cardputer.Display.setCursor(10, 110);
        M5Cardputer.Display.setTextColor(COL_CYAN, COL_BG);
        M5Cardputer.Display.print("Time: ");

        M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);

        // Footer
        drawFooter("ENT=snap  c=cfg  b=off");

        _resetCaches();
        _lastDrawnView = VIEW_DASHBOARD_A;
    }

    // ── Header (blue background, redraw whole bar on change) ────────────
    String fixStr = gps.has3DFix ? "3D" : (gps.isValid ? "2D" : "??");
    String shortFile = filename;
    int lastSlash = shortFile.lastIndexOf('/');
    if (lastSlash >= 0)
        shortFile = shortFile.substring(lastSlash + 1);
    // Include GPS log mode in header cache key so it redraws on mode change
    String modeTag = "";
    if (gpsLogMode == "zero_gps")
        modeTag = "GPS:0";
    else if (gpsLogMode == "last_known")
        modeTag = "GPS:LAST";
    String headerKey = "SAT:" + String(gps.satellites) + " " + fixStr + modeTag + "|" + shortFile;

    if (_dashACache.headerText != headerKey)
    {
        M5Cardputer.Display.fillRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, COL_HEADER);
        M5Cardputer.Display.setTextColor(COL_TEXT, COL_HEADER);
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setCursor(5, 5);
        M5Cardputer.Display.print("SAT:");
        M5Cardputer.Display.print(gps.satellites);
        M5Cardputer.Display.print(" ");
        M5Cardputer.Display.print(fixStr);

        // Show GPS log mode indicator (only for non-default modes)
        if (modeTag.length() > 0)
        {
            M5Cardputer.Display.print(" ");
            M5Cardputer.Display.setTextColor(COL_YELLOW, COL_HEADER);
            M5Cardputer.Display.print(modeTag);
            M5Cardputer.Display.setTextColor(COL_TEXT, COL_HEADER);
        }

        int fileX = SCREEN_WIDTH - (shortFile.length() * 6) - 5;
        M5Cardputer.Display.setCursor(fileX, 5);
        M5Cardputer.Display.print(shortFile);
        M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);
        _dashACache.headerText = headerKey;
    }

    // ── Dynamic value fields ────────────────────────────────────────────
    String apStr = String(uniqueAPs);
    String loggedStr = String(totalLogged);
    String uptimeStr = formatUptime(uptimeMs);
    String accStr = String(gps.accuracy, 1);
    String latStr = String(gps.lat, 6);
    String lonStr = String(gps.lng, 6);
    String altStr = String(gps.altitude, 0) + "m";

    _drawField(_dashACache.uniqueAPs, apStr, 10, 28, COL_GREEN, 2);
    _drawField(_dashACache.totalLogged, loggedStr, 178, 28, COL_TEXT);
    _drawField(_dashACache.uptime, uptimeStr, 154, 42, COL_TEXT);
    _drawField(_dashACache.accuracy, accStr, 202, 56, COL_TEXT);
    _drawField(_dashACache.lat, latStr, 40, 68, COL_TEXT);
    _drawField(_dashACache.lon, lonStr, 40, 82, COL_TEXT);
    _drawField(_dashACache.alt, altStr, 40, 96, COL_TEXT);

    // Local time (not UTC)
    String timeStr = "--:--:--";
    time_t now = time(nullptr);
    if (now > 100000L)
    {
        struct tm ti;
        gmtime_r(&now, &ti);
        char tbuf[9];
        snprintf(tbuf, sizeof(tbuf), "%02d:%02d:%02d", ti.tm_hour, ti.tm_min, ti.tm_sec);
        timeStr = String(tbuf);
    }
    _drawField(_dashACache.localTime, timeStr, 46, 110, COL_TEXT);

    // Update cache
    _dashACache.uniqueAPs = apStr;
    _dashACache.totalLogged = loggedStr;
    _dashACache.uptime = uptimeStr;
    _dashACache.accuracy = accStr;
    _dashACache.lat = latStr;
    _dashACache.lon = lonStr;
    _dashACache.alt = altStr;
    _dashACache.localTime = timeStr;

    // ── GPS Fix Lost overlay (red warning) ──────────────────────────────
    bool fixLost = !gps.isValid || !gps.has3DFix || gps.accuracy > accuracyThreshold;
    if (fixLost != _dashAOverlay.fixLost)
    {
        // Erase/draw in the area right of "Accur:" label, y=56 area
        if (fixLost)
        {
            // Draw prominent red NO FIX warning
            M5Cardputer.Display.setTextSize(2);
            M5Cardputer.Display.setTextColor(COL_RED, COL_BG);
            M5Cardputer.Display.setCursor(130, 68);
            M5Cardputer.Display.print("NO FIX");
            M5Cardputer.Display.setTextSize(1);
            M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);
        }
        else
        {
            // Erase the NO FIX warning
            M5Cardputer.Display.setTextSize(2);
            M5Cardputer.Display.setTextColor(COL_BG, COL_BG);
            M5Cardputer.Display.setCursor(130, 68);
            M5Cardputer.Display.print("NO FIX");
            M5Cardputer.Display.setTextSize(1);
            M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);
        }
        _dashAOverlay.fixLost = fixLost;
    }

    // ── Scan Stopped overlay ────────────────────────────────────────────
    if (scanStopped != _dashAOverlay.scanStopped)
    {
        if (scanStopped)
        {
            M5Cardputer.Display.setTextSize(2);
            M5Cardputer.Display.setTextColor(COL_RED, COL_BG);
            M5Cardputer.Display.setCursor(130, 86);
            M5Cardputer.Display.print("STOPPED");
            M5Cardputer.Display.setTextSize(1);
            M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);
        }
        else
        {
            M5Cardputer.Display.setTextSize(2);
            M5Cardputer.Display.setTextColor(COL_BG, COL_BG);
            M5Cardputer.Display.setCursor(130, 86);
            M5Cardputer.Display.print("STOPPED");
            M5Cardputer.Display.setTextSize(1);
            M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);
        }
        _dashAOverlay.scanStopped = scanStopped;
    }

    // ── Logging Paused overlay ──────────────────────────────────────────
    if (loggingPaused != _dashAOverlay.loggingPaused)
    {
        if (loggingPaused)
        {
            M5Cardputer.Display.setTextSize(1);
            M5Cardputer.Display.setTextColor(COL_ORANGE, COL_BG);
            M5Cardputer.Display.setCursor(130, 108);
            M5Cardputer.Display.print("LOG PAUSED");
            M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);
        }
        else
        {
            M5Cardputer.Display.setTextSize(1);
            M5Cardputer.Display.setTextColor(COL_BG, COL_BG);
            M5Cardputer.Display.setCursor(130, 108);
            M5Cardputer.Display.print("LOG PAUSED");
            M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);
        }
        _dashAOverlay.loggingPaused = loggingPaused;
    }
}

void Display::updateDashboardB(const SecurityStats &stats,
                               const std::vector<WiFiNetwork> &topNetworks,
                               int visibleCount)
{
    if (_isHelpVisible)
        return;

    // Base draw — static labels, drawn once per view entry
    if (_lastDrawnView != VIEW_DASHBOARD_B)
    {
        clearScreen();
        drawHeader("SNAPSHOT");

        M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);
        M5Cardputer.Display.setTextSize(1);

        // Static labels — security row
        M5Cardputer.Display.setCursor(10, 28);
        M5Cardputer.Display.setTextColor(COL_GREEN, COL_BG);
        M5Cardputer.Display.print("Open: ");

        M5Cardputer.Display.setCursor(90, 28);
        M5Cardputer.Display.setTextColor(COL_CYAN, COL_BG);
        M5Cardputer.Display.print("WPA2: ");

        M5Cardputer.Display.setCursor(170, 28);
        M5Cardputer.Display.setTextColor(COL_YELLOW, COL_BG);
        M5Cardputer.Display.print("WPA3: ");

        // Divider
        M5Cardputer.Display.drawLine(10, 52, SCREEN_WIDTH - 10, 52, 0x7BEF);

        // Static "Top Signals:" label
        M5Cardputer.Display.setCursor(10, 58);
        M5Cardputer.Display.setTextColor(COL_CYAN, COL_BG);
        M5Cardputer.Display.print("Top Signals:");

        // Static "In Range" label for visible AP count (lower right)
        M5Cardputer.Display.setCursor(178, 58);
        M5Cardputer.Display.setTextColor(COL_CYAN, COL_BG);
        M5Cardputer.Display.print("Visible:");

        M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);

        // Footer
        drawFooter("ENT=next H = Help");

        _resetCaches();
        _lastDrawnView = VIEW_DASHBOARD_B;
    }

    // ── Security count fields ───────────────────────────────────────────
    String openStr = String(stats.openCount);
    String wpa2Str = String(stats.wpa2Count);
    String wpa3Str = String(stats.wpa3Count);

    _drawField(_dashBCache.openCount, openStr, 46, 28, COL_TEXT);
    _drawField(_dashBCache.wpa2Count, wpa2Str, 126, 28, COL_TEXT);
    _drawField(_dashBCache.wpa3Count, wpa3Str, 206, 28, COL_TEXT);

    _dashBCache.openCount = openStr;
    _dashBCache.wpa2Count = wpa2Str;
    _dashBCache.wpa3Count = wpa3Str;

    // ── "Other:" conditional field ──────────────────────────────────────
    String newOther = (stats.otherCount > 0) ? String(stats.otherCount) : "";
    if (_dashBCache.otherCount != newOther)
    {
        // Erase old label + value
        if (_dashBCache.otherCount.length() > 0 && _dashBCache.otherCount != "\x01")
        {
            M5Cardputer.Display.setTextSize(1);
            M5Cardputer.Display.setTextColor(COL_BG, COL_BG);
            M5Cardputer.Display.setCursor(10, 42);
            M5Cardputer.Display.print("Other: ");
            M5Cardputer.Display.print(_dashBCache.otherCount);
        }
        // Draw new label + value
        if (newOther.length() > 0)
        {
            M5Cardputer.Display.setTextSize(1);
            M5Cardputer.Display.setCursor(10, 42);
            M5Cardputer.Display.setTextColor(COL_ORANGE, COL_BG);
            M5Cardputer.Display.print("Other: ");
            M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);
            M5Cardputer.Display.print(newOther);
        }
        _dashBCache.otherCount = newOther;
    }

    // ── Top-signal rows ─────────────────────────────────────────────────
    const int rowY[3] = {72, 86, 100};
    int count = min((int)topNetworks.size(), 3);

    for (int i = 0; i < 3; i++)
    {
        String newRow;
        if (count == 0 && i == 0)
        {
            newRow = "No networks in range";
        }
        else if (i < count)
        {
            String name = topNetworks[i].ssid;
            if (name.length() == 0)
                name = "<hidden>";
            newRow = String(topNetworks[i].rssi) + "dBm " + truncateSSID(name, 22);
        }
        // else newRow stays empty

        if (_dashBCache.rows[i] != newRow)
        {
            // Erase old row
            if (_dashBCache.rows[i] != "\x01" && _dashBCache.rows[i].length() > 0)
            {
                M5Cardputer.Display.setTextSize(1);
                M5Cardputer.Display.setTextColor(COL_BG, COL_BG);
                M5Cardputer.Display.setCursor(10, rowY[i]);
                M5Cardputer.Display.print(_dashBCache.rows[i]);
            }
            // Draw new row
            if (newRow.length() > 0)
            {
                M5Cardputer.Display.setTextSize(1);
                M5Cardputer.Display.setCursor(10, rowY[i]);
                if (count == 0 && i == 0)
                {
                    // "No networks in range" in grey
                    M5Cardputer.Display.setTextColor(0x7BEF, COL_BG);
                    M5Cardputer.Display.print(newRow);
                }
                else
                {
                    // RSSI with colour coding + SSID in white
                    int rssi = topNetworks[i].rssi;
                    uint16_t rssiColor = (rssi > -50) ? COL_GREEN : (rssi > -70) ? COL_YELLOW
                                                                                 : COL_ORANGE;
                    M5Cardputer.Display.setTextColor(rssiColor, COL_BG);
                    M5Cardputer.Display.print(rssi);
                    M5Cardputer.Display.print("dBm ");
                    M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);
                    String name = topNetworks[i].ssid;
                    if (name.length() == 0)
                        name = "<hidden>";
                    M5Cardputer.Display.print(truncateSSID(name, 22));
                }
            }
            _dashBCache.rows[i] = newRow;
        }
    }

    // ── Visible AP count (lower right, large font) ──────────────────────
    String visStr = String(visibleCount);
    _drawField(_dashBCache.visibleCount, visStr, 184, 76, COL_TEXT, 2);
    _dashBCache.visibleCount = visStr;

    M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);
}

void Display::updateDashboardC(const std::vector<RecentEntry> &recent)
{
    if (_isHelpVisible)
        return;

    drawHeader("Recent APs [3/5]");

    // Full content-area clear on every call (2-second refresh, no flicker concern)
    M5Cardputer.Display.fillRect(0, HEADER_HEIGHT, SCREEN_WIDTH,
                                 SCREEN_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT, COL_BG);

    M5Cardputer.Display.setTextSize(1);

    int count = min((int)recent.size(), MAX_RECENT_APS);
    for (int i = 0; i < count; i++)
    {
        const RecentEntry &entry = recent[i];
        int y = HEADER_HEIGHT + 2 + (i * 10);

        // Time column — grey "HH:MM:SS", or "--:--:--" when unavailable
        String timeStr = (entry.seenTime.length() > 0) ? entry.seenTime : "--:--:--";
        M5Cardputer.Display.setTextColor(0x7BEF, COL_BG); // grey
        M5Cardputer.Display.setCursor(4, y);
        M5Cardputer.Display.print(timeStr);

        // SSID / MAC column — SSID if non-empty, BSSID otherwise; white
        String name = (entry.net.ssid.length() > 0) ? entry.net.ssid : entry.net.bssid;
        M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);
        M5Cardputer.Display.setCursor(57, y);
        M5Cardputer.Display.print(truncateSSID(name, 19));

        // RSSI column — colour-coded: green >-50, yellow >-70, orange <=-70
        int rssi = entry.net.rssi;
        uint16_t rssiCol = (rssi > -50) ? COL_GREEN : (rssi > -70) ? COL_YELLOW
                                                                   : COL_ORANGE;
        M5Cardputer.Display.setTextColor(rssiCol, COL_BG);
        M5Cardputer.Display.setCursor(183, y);
        M5Cardputer.Display.print(rssi);
    }

    M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);
    drawFooter("ENT=next H = Help");
}

void Display::updateDashboardD(const std::vector<RecentEntry> &recentUnique)
{
    if (_isHelpVisible)
        return;

    drawHeader("Unique APs [4/5]");

    // Full content-area clear on every call (2-second refresh, no flicker concern)
    M5Cardputer.Display.fillRect(0, HEADER_HEIGHT, SCREEN_WIDTH,
                                 SCREEN_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT, COL_BG);

    M5Cardputer.Display.setTextSize(1);

    int count = min((int)recentUnique.size(), MAX_RECENT_APS);
    for (int i = 0; i < count; i++)
    {
        const RecentEntry &entry = recentUnique[i];
        int y = HEADER_HEIGHT + 2 + (i * 10);

        // Time column — grey "HH:MM:SS", or "--:--:--" when unavailable
        String timeStr = (entry.seenTime.length() > 0) ? entry.seenTime : "--:--:--";
        M5Cardputer.Display.setTextColor(0x7BEF, COL_BG); // grey
        M5Cardputer.Display.setCursor(4, y);
        M5Cardputer.Display.print(timeStr);

        // SSID / MAC column — SSID if non-empty, BSSID otherwise; white
        String name = (entry.net.ssid.length() > 0) ? entry.net.ssid : entry.net.bssid;
        M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);
        M5Cardputer.Display.setCursor(57, y);
        M5Cardputer.Display.print(truncateSSID(name, 19));

        // RSSI column — colour-coded: green >-50, yellow >-70, orange <=-70
        int rssi = entry.net.rssi;
        uint16_t rssiCol = (rssi > -50) ? COL_GREEN : (rssi > -70) ? COL_YELLOW
                                                                   : COL_ORANGE;
        M5Cardputer.Display.setTextColor(rssiCol, COL_BG);
        M5Cardputer.Display.setCursor(183, y);
        M5Cardputer.Display.print(rssi);
    }

    M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);
    drawFooter("ENT=next H = Help");
}

void Display::updateDashboardE(bool soundMuted)
{
    if (_isHelpVisible)
        return;

    // Full redraw every time the view is entered — data is read on entry only
    if (_lastDrawnView != VIEW_DASHBOARD_E)
    {
        clearScreen();
        drawHeader("SETTINGS [5/5]");

        // ── Battery (large, prominent) ──────────────────────────────────
        int bat = _batteryLevel;
        uint16_t batColor = (bat > 50) ? COL_GREEN : (bat > 20) ? COL_YELLOW
                                                                : COL_RED;
        String batStr = (bat < 0) ? "---%" : (String(bat) + "%");

        M5Cardputer.Display.setTextSize(3);
        M5Cardputer.Display.setTextColor(batColor, COL_BG);
        // Centre the battery text
        int batWidth = batStr.length() * 18; // textSize 3 = 18px per char
        int batX = (SCREEN_WIDTH - batWidth) / 2;
        M5Cardputer.Display.setCursor(batX, 26);
        M5Cardputer.Display.print(batStr);

        // "Battery" label
        M5Cardputer.Display.setTextSize(1);
        M5Cardputer.Display.setTextColor(0x7BEF, COL_BG); // grey
        M5Cardputer.Display.setCursor((SCREEN_WIDTH - 42) / 2, 52);
        M5Cardputer.Display.print("Battery");

        // ── RAM Info ────────────────────────────────────────────────────
        uint32_t totalHeap = ESP.getHeapSize();
        uint32_t freeHeap = ESP.getFreeHeap();
        uint32_t totalPsram = ESP.getPsramSize();
        uint32_t freePsram = ESP.getFreePsram();

        uint32_t totalRAM = totalHeap + totalPsram;
        uint32_t freeRAM = freeHeap + freePsram;

        M5Cardputer.Display.setTextColor(COL_CYAN, COL_BG);
        M5Cardputer.Display.setCursor(10, 68);
        M5Cardputer.Display.print("Total RAM: ");
        M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);
        M5Cardputer.Display.print(String(totalRAM / 1024) + " KB");

        M5Cardputer.Display.setTextColor(COL_CYAN, COL_BG);
        M5Cardputer.Display.setCursor(10, 80);
        M5Cardputer.Display.print("Free RAM:  ");
        M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);
        M5Cardputer.Display.print(String(freeRAM / 1024) + " KB");

        // ── Sound Status ────────────────────────────────────────────────
        M5Cardputer.Display.setTextColor(COL_CYAN, COL_BG);
        M5Cardputer.Display.setCursor(10, 96);
        M5Cardputer.Display.print("Sound: ");
        if (soundMuted)
        {
            M5Cardputer.Display.setTextColor(COL_RED, COL_BG);
            M5Cardputer.Display.print("MUTED");
        }
        else
        {
            M5Cardputer.Display.setTextColor(COL_GREEN, COL_BG);
            M5Cardputer.Display.print("ON");
        }

        M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);
        drawFooter("ENT=next H = Help");

        _lastDrawnView = VIEW_DASHBOARD_E;
    }
}

void Display::drawHeader(const String &title)
{
    M5Cardputer.Display.fillRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, COL_HEADER);
    M5Cardputer.Display.setTextColor(COL_TEXT, COL_HEADER);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setCursor(5, 5);
    M5Cardputer.Display.print(title);
    M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);
}

void Display::drawFooter(const String &status)
{
    int footerY = SCREEN_HEIGHT - FOOTER_HEIGHT;
    M5Cardputer.Display.fillRect(0, footerY, SCREEN_WIDTH, FOOTER_HEIGHT, 0x18E3); // dark grey
    M5Cardputer.Display.setTextColor(0x7BEF, 0x18E3);

    // Left: status hint
    M5Cardputer.Display.setCursor(5, footerY + 3);
    M5Cardputer.Display.print(status);

    // Right: battery level
    String batStr = (_batteryLevel < 0) ? "B=---%" : ("B=" + String(_batteryLevel) + "%");
    int batX = SCREEN_WIDTH - (int)(batStr.length() * 6) - 4;
    M5Cardputer.Display.setCursor(batX, footerY + 3);
    M5Cardputer.Display.print(batStr);

    M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);
}

String Display::formatUptime(unsigned long ms)
{
    unsigned long secs = ms / 1000;
    unsigned long mins = secs / 60;
    unsigned long hrs = mins / 60;

    char buf[12];
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", hrs, mins % 60, secs % 60);
    return String(buf);
}

String Display::truncateSSID(const String &ssid, int maxLen)
{
    if ((int)ssid.length() <= maxLen)
        return ssid;
    return ssid.substring(0, maxLen - 2) + "..";
}

// ─── Display blank / unblank ────────────────────────────────────────────────

void Display::toggleBlank(int savedBrightness)
{
    if (_isBlank)
    {
        M5Cardputer.Display.setBrightness(savedBrightness);
        _isBlank = false;
    }
    else
    {
        M5Cardputer.Display.setBrightness(0);
        _isBlank = true;
    }
}

void Display::ensureUnblanked(int savedBrightness)
{
    if (_isBlank)
    {
        toggleBlank(savedBrightness);
    }
}

// ─── Help overlay ────────────────────────────────────────────────────────────

void Display::showHelp()
{
    _isHelpVisible = true;
    clearScreen();
    drawHeader("KEYBOARD HELP");

    M5Cardputer.Display.setTextSize(1);

    // Column headers
    M5Cardputer.Display.setTextColor(0x7BEF, COL_BG); // grey
    M5Cardputer.Display.setCursor(10, 23);
    M5Cardputer.Display.print("Key");
    M5Cardputer.Display.setCursor(62, 23);
    M5Cardputer.Display.print("Action");

    // Divider line
    M5Cardputer.Display.drawFastHLine(10, 31, SCREEN_WIDTH - 20, 0x2945);

    // Key binding rows: { key label, action description }
    struct Binding
    {
        const char *key;
        const char *action;
    };
    const Binding bindings[] = {
        {"ENTER", "Cycle dashboard views"},
        {"H", "Toggle this help page"},
        {"B", "Toggle display blank"},
        {"C", "Enter config mode"},
        {"S", "Toggle sound mute"},
        {"Q", "Safe shutdown"},
        {"X", "Stop / Start scan"},
        {"P", "Pause / Resume logging"},
        {"G0", "Drop FLAG marker"},
    };

    int y = 33;
    for (const auto &b : bindings)
    {
        M5Cardputer.Display.setTextColor(COL_CYAN, COL_BG);
        M5Cardputer.Display.setCursor(10, y);
        M5Cardputer.Display.print(b.key);
        M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);
        M5Cardputer.Display.setCursor(62, y);
        M5Cardputer.Display.print(b.action);
        y += 11;
    }

    M5Cardputer.Display.setTextColor(COL_TEXT, COL_BG);
    drawFooter("Press H to close");
}

void Display::dismissHelp()
{
    _isHelpVisible = false;
    _lastDrawnView = VIEW_NONE; // Force full redraw of the current dashboard view
    _resetCaches();
    clearScreen();
}
