#include <M5Cardputer.h>
#include <SPI.h>
#include <SD.h>
#include <algorithm>
#include <new>
#include <time.h>

#include "Config.h"
#include "ConfigManager.h"
#include "Logger.h"
#include "GPSManager.h"
#include "WiFiScanner.h"
#include "GeofenceFilter.h"
#include "DataLogger.h"
#include "Display.h"
#include "AlertManager.h"
#include "KeyboardHandler.h"
#include "WebPortal.h"

// ── Global Instances ────────────────────────────────────────────────────────
ConfigManager configManager;
Display display;
AlertManager alertManager;
GPSManager gpsManager;
WiFiScanner wifiScanner;
GeofenceFilter geofenceFilter;
DataLogger dataLogger;
WebPortal *webPortal = nullptr;
KeyboardHandler keyboardHandler;

// ── State ───────────────────────────────────────────────────────────────────
AppState currentState = STATE_INIT;
ScanSession session;
bool superDebug = false;

// ── Battery Monitoring ─────────────────────────────────────────
static const unsigned long BATTERY_CHECK_INTERVAL_MS = 60000UL;
static const unsigned long LOW_BATTERY_WARNING_DURATION_MS = 60000UL;

// ── Forward Declarations ────────────────────────────────────────────────────
void executeKeyAction(KeyAction action);
void handleToggleScanAction();
void handleTogglePauseAction();
void handleSearchingSatellites();
void handleActiveScan();
void handleConfigMode();
void handleLowBatteryWarning();
void enterConfigModeState();
void enterSearchingSatellitesState();
void enterActiveScanState();
void enterShutdownState(bool lowBattery = false);
void enterLowBatteryWarningState(int pct);
void checkBattery();
void logSystemStatsIfDue(unsigned long now);
void processCompletedScan();
std::vector<WiFiNetwork> getTopNetworks(const std::vector<WiFiNetwork> &nets, int count);

// ═══════════════════════════════════════════════════════════════════════════
// SETUP
// ═══════════════════════════════════════════════════════════════════════════
void setup()
{
    session.reset();

    // 1. Initialize M5Cardputer hardware
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);

    Serial.begin(115200);
    delay(1000); // Wait for USB CDC re-enumeration after reset (was 1000) TODO
    Serial.println("\n\n=== M5 Cardputer Wardriver ===");
    Serial.println("Firmware v" FIRMWARE_VERSION);

    // 2. Display startup splash (before SD — no SD access)
    display.begin();
    display.showStartup();

    // 3. Initialize SD card
    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);

    if (!SD.begin(SD_SPI_CS_PIN, SPI, 25000000))
    {
        Serial.println("ERROR: SD Card initialization failed!");
        display.showError("SD CARD ERROR!");
        while (1)
            delay(1000);
    }
    Serial.println("SD Card initialized");

    // 4. Create application directory
    if (!SD.exists(APP_DIR))
    {
        if (!SD.mkdir(APP_DIR))
        {
            Serial.println("Failed to create " APP_DIR " directory");
            display.showError("Cant create " APP_DIR);
            while (1)
                delay(1000);
        }
    }

    // 4b. Create logs directory so Logger doesn't fail on first write
    if (!SD.exists(DEBUG_LOG_DIR))
    {
        SD.mkdir(DEBUG_LOG_DIR);
    }

    // 5. Initialize logger
    logger.begin();

    // 6. Load configuration
    bool configLoaded = configManager.loadConfig();
    AppConfig &appConfig = configManager.getConfig();

    // 7. Configure logger with loaded config
    logger.setConfig(&appConfig);

    // 8. Initialize alert manager (LED + buzzer + brightness)
    alertManager.begin(appConfig.hardware);

    // 9. Check G0 button — if held, enter Config Mode
    M5Cardputer.update();
    if (M5Cardputer.BtnA.isPressed())
    {
        logger.debugPrintln("G0 button held at boot — entering Config Mode");
        enterConfigModeState();
        return;
    }

    // 10. If config was invalid/missing, enter Config Mode
    if (!configLoaded || !configManager.isValid())
    {
        logger.debugPrintln("No valid config — entering Config Mode");
        enterConfigModeState();
        return;
    }

    // 12. Initialize GPS
    superDebug = appConfig.debug.enabled && appConfig.debug.super_debug_enabled;
    gpsManager.begin(appConfig.gps.tx_pin, appConfig.gps.rx_pin, superDebug);
    dataLogger.setGPSManager(&gpsManager);

    // 13. Initialize WiFi scanner
    wifiScanner.begin(appConfig.scan.wifi_country_code, appConfig.scan.wifi_tx_power);
    wifiScanner.configure(appConfig.scan.scan_mode,
                          appConfig.scan.active_dwell_ms,
                          appConfig.scan.passive_dwell_ms,
                          appConfig.scan.mac_randomize_interval,
                          appConfig.scan.mac_spoof_oui,
                          superDebug);

    // 14. Configure geofence filter
    geofenceFilter.configure(appConfig.filter, appConfig.gps.accuracy_threshold_meters);

    // 15. Record boot time; do initial battery read, then enter searching state
    session.bootTime = millis();
    checkBattery();
    session.lastBatteryCheck = millis();
    if (currentState != STATE_LOW_BATTERY_WARNING)
    {
        enterSearchingSatellitesState();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// LOOP
// ═══════════════════════════════════════════════════════════════════════════
void loop()
{
    M5Cardputer.update();
    alertManager.update();

    executeKeyAction(keyboardHandler.poll(currentState, display.isHelpVisible(), display.isBlank()));

    // Periodic battery check (skip during config mode and shutdown)
    if (currentState == STATE_SEARCHING_SATS || currentState == STATE_ACTIVE_SCAN)
    {
        if (millis() - session.lastBatteryCheck >= BATTERY_CHECK_INTERVAL_MS)
        {
            checkBattery();
            session.lastBatteryCheck = millis();
        }
        logSystemStatsIfDue(millis());
    }

    switch (currentState)
    {
    case STATE_SEARCHING_SATS:
        handleSearchingSatellites();
        break;

    case STATE_ACTIVE_SCAN:
        handleActiveScan();
        break;

    case STATE_CONFIG_MODE:
        handleConfigMode();
        break;

    case STATE_LOW_BATTERY_WARNING:
        handleLowBatteryWarning();
        break;

    case STATE_SHUTDOWN:
        // Screen locked — only G0 button reboots
        if (M5Cardputer.BtnA.wasPressed())
        {
            logger.debugPrintln("[Shutdown] G0 pressed — rebooting");
            ESP.restart();
        }
        delay(50);
        break;

    case STATE_ERROR:
        // Halted — do nothing
        break;

    default:
        break;
    }
    delay(1);
}

// ═══════════════════════════════════════════════════════════════════════════
// KEYBOARD ACTIONS
// ═══════════════════════════════════════════════════════════════════════════
void handleToggleScanAction()
{
    if (currentState != STATE_ACTIVE_SCAN && currentState != STATE_SEARCHING_SATS)
    {
        return;
    }

    if (!session.scanStopped)
    {
        // Stop scanning — low power mode
        session.scanStopped = true;
        session.loggingPaused = false; // Clear pause flag when stopping
        wifiScanner.stopScanning();

        if (dataLogger.isOpen())
        {
            dataLogger.flush();
            dataLogger.close();
        }
        alertManager.soundStop();
        if (superDebug)
        {
            logger.debugPrintln("[Input] 'x' pressed: scan STOPPED");
        }
        return;
    }

    // Resume scanning — re-init WiFi, open new CSV
    wifiScanner.initValues();
    wifiScanner.configure(configManager.getConfig().scan.scan_mode,
                          configManager.getConfig().scan.active_dwell_ms,
                          configManager.getConfig().scan.passive_dwell_ms,
                          configManager.getConfig().scan.mac_randomize_interval,
                          configManager.getConfig().scan.mac_spoof_oui,
                          superDebug);

    if (!dataLogger.begin(SD))
    {
        logger.debugPrintln("[Input] ERROR: Failed to open new log file on resume");
        display.showError("Log file open failed!");
        currentState = STATE_ERROR;
        alertManager.setStatusError();
        return;
    }
    logger.debugPrintln(String("[Input] Resumed scanning: new file: ") + dataLogger.getActiveFilename());

    alertManager.setStatusReady();
    alertManager.soundStart();
    session.lastScanTime = 0; // Trigger scan immediately
    session.lastDisplayUpdate = 0;
    session.loggingPaused = false;
    session.scanStopped = false;

    if (superDebug)
    {
        logger.debugPrintln("[Input] 'x' pressed: scan RESUMED");
    }

    enterActiveScanState();
}

void handleTogglePauseAction()
{
    if (currentState != STATE_ACTIVE_SCAN || session.scanStopped)
    {
        return;
    }

    session.loggingPaused = !session.loggingPaused;
    if (session.loggingPaused)
    {
        if (dataLogger.isOpen())
        {
            dataLogger.flush();
            dataLogger.close();
        }
        alertManager.soundPause();
    }
    else
    {
        bool loggingResumed = true;
        if (!dataLogger.isOpen())
        {
            loggingResumed = dataLogger.openLogFile();
        }

        if (loggingResumed)
        {
            alertManager.soundResume();
        }
        else
        {
            session.loggingPaused = true;
            logger.debugPrintln("[Input] 'p' pressed: failed to reopen log file, logging remains PAUSED");
        }
    }

    session.lastScanTime = 0;      // Trigger scan immediately
    session.lastDisplayUpdate = 0; // Force display update
    if (superDebug)
    {
        logger.debugPrintln(session.loggingPaused ? "[Input] 'p' pressed: logging PAUSED" : "[Input] 'p' pressed: logging RESUMED");
    }
}

void executeKeyAction(KeyAction action)
{
    switch (action)
    {
    case KEY_ACTION_NONE:
        return;

    case KEY_ACTION_NEXT_VIEW:
        alertManager.soundNextView();
        display.nextView();
        session.lastDisplayUpdate = 0;
        session.lastDashCUpdate = 0;
        session.lastDashDUpdate = 0;
        logger.debugPrintln("[Input] Dashboard toggled");
        return;

    case KEY_ACTION_TOGGLE_BLANK:
        display.toggleBlank(configManager.getConfig().hardware.screen_brightness);
        if (currentState == STATE_SHUTDOWN)
        {
            logger.debugPrintln(display.isBlank() ? "[Shutdown] 'b' pressed: display off" : "[Shutdown] 'b' pressed: display on");
            return;
        }

        if (display.isBlank())
        {
            alertManager.soundBlankDisplay();
        }
        else
        {
            alertManager.soundEnableDisplay();
        }
        logger.debugPrintln(display.isBlank() ? "[Input] 'b' pressed: display off" : "[Input] 'b' pressed: display on");
        return;

    case KEY_ACTION_ENTER_CONFIG:
        alertManager.soundConfigMode();
        logger.debugPrintln("[Input] 'c' pressed: entering Config Mode");
        enterConfigModeState();
        return;

    case KEY_ACTION_SHUTDOWN:
        alertManager.soundShutdown();
        logger.debugPrintln("[Input] 'q' pressed: entering Shutdown");
        enterShutdownState();
        return;

    case KEY_ACTION_TOGGLE_MUTE:
        alertManager.toggleMute();
        if (!alertManager.isMuted())
        {
            alertManager.beep();
        }
        logger.debugPrintln(alertManager.isMuted() ? "[Input] 's' pressed — sound OFF" : "[Input] 's' pressed — sound ON");
        return;

    case KEY_ACTION_SHOW_HELP:
        alertManager.soundHelp();
        display.showHelp();
        logger.debugPrintln("[Input] 'h' pressed: help shown");
        return;

    case KEY_ACTION_DISMISS_HELP:
        alertManager.soundQuitHelp();
        display.dismissHelp();
        session.lastDisplayUpdate = 0;
        session.lastDashCUpdate = 0;
        session.lastDashDUpdate = 0;
        logger.debugPrintln("[Input] 'h' pressed: help dismissed");
        return;

    case KEY_ACTION_TOGGLE_SCAN:
        handleToggleScanAction();
        return;

    case KEY_ACTION_TOGGLE_PAUSE:
        handleTogglePauseAction();
        return;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// STATE: SEARCHING SATELLITES
// ═══════════════════════════════════════════════════════════════════════════
void enterSearchingSatellitesState()
{
    currentState = STATE_SEARCHING_SATS;
    alertManager.setStatusSearching();
    session.lastScanTime = 0; // Trigger first scan immediately
    if (superDebug)
    {
        logger.debugPrintln("[State] Entering SEARCHING_SATS");
    }
}

void handleSearchingSatellites()
{
    AppConfig &config = configManager.getConfig();
    unsigned long now = millis();

    // Update GPS
    gpsManager.update();

    // Check if we have a valid 3D fix with acceptable accuracy
    session.gps = gpsManager.getData();
    if (session.gps.has3DFix && session.gps.accuracy <= config.gps.accuracy_threshold_meters)
    {

        // Sync system time from GPS on first fix
        if (!gpsManager.isTimeSynced())
        {
            gpsManager.syncSystemTime(config.gps.gmt_offset_hours);
            logger.debugPrintln("Time set");
        }
        logger.debugPrintln("GPS fix acquired with accuracy " + String(session.gps.accuracy) + "m");
        enterActiveScanState();
        return;
    }

    // Update display periodically

    if (now - session.lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL_MS)
    {
        display.showSearchingSats(session.gps, wifiScanner.getUniqueCount(), session.lastScanCount);
        session.lastDisplayUpdate = now;
    }

    // Periodic geofence area beep (every 10 seconds)
    if (session.gps.isValid && session.gps.has3DFix &&
        session.gps.accuracy <= config.gps.accuracy_threshold_meters &&
        !config.filter.geofence_boxes.empty() &&
        (now - session.lastGeofenceBeep >= GEOFENCE_BEEP_INTERVAL_MS))
    {
        if (geofenceFilter.isInsideGeofence(session.gps.lat, session.gps.lng))
        {
            alertManager.geofenceAreaBeep();
        }
        session.lastGeofenceBeep = now;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// STATE: ACTIVE SCAN
// ═══════════════════════════════════════════════════════════════════════════
void enterActiveScanState()
{
    currentState = STATE_ACTIVE_SCAN;
    alertManager.setStatusReady();

    // Open CSV log file
    if (!dataLogger.begin(SD))
    {
        logger.debugPrintln("ERROR: Failed to open log file");
        display.showError("Log file open failed!");
        currentState = STATE_ERROR;
        alertManager.setStatusError();
        return;
    }
    if (superDebug)
    {
        logger.debugPrintln(String("Logging to: ") + dataLogger.getActiveFilename());
    }
    session.lastScanTime = 0; // Trigger first scan immediately
    session.lastDisplayUpdate = 0;
}

void handleActiveScan()
{
    AppConfig &config = configManager.getConfig();
    unsigned long now = millis();

    // ── G0 button: drop FLAG marker entry ────────────────────────────────
    if (M5Cardputer.BtnA.wasPressed())
    {
        GPSData flagGps = gpsManager.getData();
        if (flagGps.isValid && flagGps.has3DFix && dataLogger.isOpen() && !session.scanStopped)
        {
            dataLogger.logFlagEntry(flagGps);
            alertManager.beep();
            String msg = "[FLAG] Dropped at " + String(flagGps.lat, 6) + ", " +
                         String(flagGps.lng, 6) + " at " + flagGps.dateTime;
            logger.debugPrintln(msg.c_str());
            Serial.println(msg);
        }
        else
        {
            logger.debugPrintln("[FLAG] G0 pressed but no GPS fix or scan stopped: flag not dropped");
        }
    }

    // ── When scan is stopped, skip GPS/WiFi/logging — low power mode ────
    if (session.scanStopped)
    {
        // Only update display
        DisplayView currentView = display.getCurrentView();
        if (currentView == VIEW_DASHBOARD_A)
        {
            if (now - session.lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL_MS)
            {
                session.gps = gpsManager.getData();
                display.updateDashboardA(
                    session.gps,
                    dataLogger.getActiveFilename(),
                    wifiScanner.getUniqueCount(),
                    dataLogger.getTotalLogged(),
                    now - session.bootTime,
                    config.gps.accuracy_threshold_meters,
                    true, session.loggingPaused,
                    config.gps.gps_log_mode);
                session.lastDisplayUpdate = now;
            }
        }
        else if (currentView == VIEW_DASHBOARD_E)
        {
            display.updateDashboardE(alertManager.isMuted());
        }

        // Extra delay for power saving when stopped
        delay(100);
        return;
    }

    // Start a new sweep after the inter-sweep delay
    if (!wifiScanner.isScanning() && (now - session.lastScanTime >= (unsigned long)config.scan.scan_delay_ms))
    {
        if (wifiScanner.isAllChannelMode())
        {
            wifiScanner.startFullScan();
        }
        else
        {
            wifiScanner.startScan();
        }
        session.startScanTime = now;

        // TODO is this in the right place? Should we get it before updating Dashboard A?
        // Update GPS
        gpsManager.update();
        session.gps = gpsManager.getData();
    }

    // Is it time to sync system time from GPS again?
    if (!gpsManager.isTimeSynced())
    {
        gpsManager.syncSystemTime(config.gps.gmt_offset_hours);
    }

    // Drive the scan state machine
    bool scanDone = false;
    if (wifiScanner.isScanning() && (now - session.startScanTime >= 250UL)) // Timeout for scan completion
    {
        scanDone = wifiScanner.isAllChannelMode()
                       ? wifiScanner.isFullScanComplete()
                       : wifiScanner.isScanComplete();
    }

    if (scanDone)
    {
        // Update GPS
        gpsManager.update();
        session.gps = gpsManager.getData();

        processCompletedScan();
        session.lastScanTime = now; // Start inter-sweep delay timer

        // Update recent AP list (Dashboard C feed - VIEW_DASHBOARD_C)
        {
            String _fdt = gpsManager.getFormattedDateTime(false);
            String seenTime = (_fdt.length() >= 19) ? _fdt.substring(11, 19) : "";

            // Clear the list so it represents only the latest sweep results.
            session.recentAPs.clear();

            // Process in reverse so that session.lastResults[0] ends up at the front
            for (int i = (int)session.lastResults.size() - 1; i >= 0; i--)
            {
                const WiFiNetwork &net = session.lastResults[i];
                session.recentAPs.insert(session.recentAPs.begin(), {net, seenTime});
            }
            if ((int)session.recentAPs.size() > MAX_RECENT_APS)
            {
                session.recentAPs.resize(MAX_RECENT_APS);
            }
        }

        // Log each network that passes filters (skip when logging is paused)
        // Determine GPS data to use based on gps_log_mode
        const String &gpsLogMode = config.gps.gps_log_mode;
        bool shouldLog = false;
        GPSData logGps = session.gps;

        if (session.gps.isValid && session.gps.has3DFix)
        {
            // Live fix available — all modes log identically
            shouldLog = true;
        }
        else if (gpsLogMode == "zero_gps")
        {
            // No fix: log with zeroed coordinates, keep last-known accuracy
            shouldLog = true;
            GPSData lastKnown = gpsManager.getLastKnownData();
            logGps.lat = 0.0;
            logGps.lng = 0.0;
            logGps.altitude = 0.0f;
            logGps.accuracy = lastKnown.accuracy;
            logGps.satellites = 0;
            logGps.isValid = false;
            logGps.has3DFix = false;
        }
        else if (gpsLogMode == "last_known")
        {
            // No fix: only log if a fix was previously obtained this session
            if (gpsManager.hasEverHadFix())
            {
                shouldLog = true;
                logGps = gpsManager.getLastKnownData();
            }
        }
        // else: "fix_only" — shouldLog stays false when no fix

        if (shouldLog && !session.loggingPaused)
        {
            bool isNewAp = false;
            bool isGeoFenced = false;
            String _fdt = gpsManager.getFormattedDateTime(false);
            String seenTime = (_fdt.length() >= 19) ? _fdt.substring(11, 19) : "";
            for (const WiFiNetwork &net : session.lastResults)
            {
                // SSID/BSSID filters always apply via shouldLog()
                // Geofence: bypass for zero_gps (coordinates are zeros), apply otherwise
                bool passesFilter;
                if (gpsLogMode == "zero_gps" && !(session.gps.isValid && session.gps.has3DFix))
                {
                    // Bypass geofence, but still check SSID/BSSID exclusions
                    passesFilter = !geofenceFilter.isSSIDExcluded(net.ssid) &&
                                   !geofenceFilter.isBSSIDExcluded(net.bssid);
                }
                else
                {
                    passesFilter = geofenceFilter.shouldLog(net, logGps);
                }

                if (passesFilter)
                {
                    dataLogger.logEntry(net, logGps);
                    // Beep on new unique BSSID inside the geofence.
                    if (wifiScanner.isNewBSSID(net.bssid))
                    {
                        isNewAp = true;
                        // Track for Dashboard D — last 10 unique APs
                        session.recentUniqueAPs.insert(session.recentUniqueAPs.begin(), {net, seenTime});
                        if ((int)session.recentUniqueAPs.size() > MAX_RECENT_APS)
                        {
                            session.recentUniqueAPs.resize(MAX_RECENT_APS);
                        }
                    }
                }
                else
                {
                    isGeoFenced = true;
                }
            }
            if (isNewAp)
            {
                alertManager.newAPBeep();
            }
            else
            {
                if (isGeoFenced)
                {
                    alertManager.geoFencedBeep();
                }
            }
        }

        // TODO
        // Dont blink the LED
        // alertManager.setStatusReady();
        // Maybe use the colour to indicate wifi density? (eg blue for many networks, red for few)
    }

    // Periodic buffer flush
    if (now - session.lastFlushTime >= SD_FLUSH_INTERVAL_MS && dataLogger.isOpen())
    {
        dataLogger.flush();
        session.lastFlushTime = now;
    }

    // Periodic geofence area beep (every 10 seconds)
    if (session.gps.isValid && session.gps.has3DFix &&
        session.gps.accuracy <= config.gps.accuracy_threshold_meters &&
        !config.filter.geofence_boxes.empty() &&
        (now - session.lastGeofenceBeep >= GEOFENCE_BEEP_INTERVAL_MS))
    {
        if (geofenceFilter.isInsideGeofence(session.gps.lat, session.gps.lng))
        {
            alertManager.geofenceAreaBeep();
        }
        session.lastGeofenceBeep = now;
    }

    // Update display periodically
    DisplayView currentView = display.getCurrentView();
    if (currentView == VIEW_DASHBOARD_A)
    {
        if (now - session.lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL_MS)
        {
            display.updateDashboardA(
                session.gps,
                dataLogger.getActiveFilename(),
                wifiScanner.getUniqueCount(),
                dataLogger.getTotalLogged(),
                now - session.bootTime,
                config.gps.accuracy_threshold_meters,
                session.scanStopped, session.loggingPaused,
                config.gps.gps_log_mode);
            session.lastDisplayUpdate = now;
        }
    }
    else if (currentView == VIEW_DASHBOARD_B)
    {
        if (now - session.lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL_MS)
        {
            std::vector<WiFiNetwork> topNets = getTopNetworks(session.lastResults, 3);
            display.updateDashboardB(session.lastStats, topNets, session.lastScanCount);
            session.lastDisplayUpdate = now;
        }
    }
    else if (currentView == VIEW_DASHBOARD_C)
    {
        if (now - session.lastDashCUpdate >= 2000UL)
        {
            display.updateDashboardC(session.recentAPs);
            session.lastDashCUpdate = now;
        }
    }
    else if (currentView == VIEW_DASHBOARD_D)
    {
        if (now - session.lastDashDUpdate >= 2000UL)
        {
            display.updateDashboardD(session.recentUniqueAPs);
            session.lastDashDUpdate = now;
        }
    }
    else if (currentView == VIEW_DASHBOARD_E)
    {
        if (now - session.lastDashEUpdate >= 2000UL)
        {
            display.updateDashboardE(alertManager.isMuted());
            session.lastDashEUpdate = now;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// STATE: SHUTDOWN (safe SD removal)
// ═══════════════════════════════════════════════════════════════════════════
void enterShutdownState(bool lowBattery)
{
    display.ensureUnblanked(configManager.getConfig().hardware.screen_brightness);
    logger.debugPrintln("[Shutdown] Started.");

    unsigned long sessionDurationMs = millis() - session.bootTime;
    uint32_t averageScanMs = (session.totalSweeps > 0)
                                 ? (uint32_t)(sessionDurationMs / session.totalSweeps)
                                 : 0;

    SessionSummary summary = {
        dataLogger.getTotalLogged(),
        sessionDurationMs,
        averageScanMs,
        session.maxStationsPerSweep,
        session.totalStationsSeen,
        (uint32_t)wifiScanner.getUniqueCount(),
        session.totalSweeps};

    // 1. Stop WiFi scanning
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    // 2. Close data logger (flush + close file)
    if (dataLogger.isOpen())
    {
        dataLogger.flush();
        dataLogger.close();
    }

    // 3. Flush debug log — log shutdown summary before unmounting SD
    {
        char shutdownBuf[SYSTEM_STATS_LINE_BUFFER_SIZE];
        snprintf(shutdownBuf, sizeof(shutdownBuf),
                 "[Shutdown][Summary] total_logged=%lu total_seen=%llu unique_aps=%lu sweeps=%lu peak_per_sweep=%lu duration_ms=%lu avg_scan_ms=%lu",
                 (unsigned long)summary.totalLogged,
                 (unsigned long long)summary.totalStationsSeen,
                 (unsigned long)summary.uniqueStations,
                 (unsigned long)summary.totalSweeps,
                 (unsigned long)summary.maxStationsPerSweep,
                 (unsigned long)summary.sessionDurationMs,
                 (unsigned long)summary.averageScanMs);
        logger.debugPrintln(shutdownBuf);
    }
    logger.debugPrintln("[Shutdown] Final log entry before SD unmount");

    // 4. Unmount SD card so it's safe to remove
    SD.end();

    // 5. Set state and show locked screen
    currentState = STATE_SHUTDOWN;
    alertManager.clearLED(); // LED off
    display.showShutdown(summary, lowBattery);
}

// ═══════════════════════════════════════════════════════════════════════════
// STATE: BATTERY MONITORING
// ═══════════════════════════════════════════════════════════════════════════
void checkBattery()
{
    int level = M5Cardputer.Power.getBatteryLevel();
    display.setBatteryLevel(level);

    int threshold = configManager.getConfig().hardware.low_battery_threshold;
    if (threshold > 0 && level >= 0 && level <= threshold)
    {
        logger.debugPrintln("[Battery] Below threshold — entering low battery warning");
        enterLowBatteryWarningState(level);
    }
}

void enterLowBatteryWarningState(int pct)
{
    session.lastBatteryLevel = pct;
    session.lowBatteryWarnStart = millis();
    currentState = STATE_LOW_BATTERY_WARNING;
    alertManager.setStatusError(); // Red LED
    display.showLowBatteryWarning(pct, LOW_BATTERY_WARNING_DURATION_MS / 1000);
    logger.debugPrintln(("[Battery] Low battery warning: " + String(pct) + "%").c_str());
}

void handleLowBatteryWarning()
{
    unsigned long elapsed = millis() - session.lowBatteryWarnStart;
    int secsRemaining = (int)((LOW_BATTERY_WARNING_DURATION_MS - elapsed) / 1000) + 1;
    if (secsRemaining < 1)
        secsRemaining = 1;

    static int lastShownSecs = -1;
    if (secsRemaining != lastShownSecs)
    {
        display.showLowBatteryWarning(session.lastBatteryLevel, secsRemaining);
        lastShownSecs = secsRemaining;
    }

    if (elapsed >= LOW_BATTERY_WARNING_DURATION_MS)
    {
        lastShownSecs = -1;
        enterShutdownState(true);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// STATE: CONFIG MODE
// ═══════════════════════════════════════════════════════════════════════════
void enterConfigModeState()
{
    display.ensureUnblanked(configManager.getConfig().hardware.screen_brightness);
    logger.debugPrintln("[State] Entering CONFIG_MODE");

    // Close data logger if active
    if (dataLogger.isOpen())
    {
        dataLogger.close();
    }

    currentState = STATE_CONFIG_MODE;
    alertManager.setStatusConfig();

    if (webPortal)
    {
        webPortal->stop();
        delete webPortal;
        webPortal = nullptr;
    }

    // Create and start web portal
    webPortal = new (std::nothrow) WebPortal(&configManager);
    if (!webPortal)
    {
        logger.debugPrintln("[ConfigMode] Failed to allocate WebPortal");
        currentState = STATE_ERROR;
        display.showError("CONFIG ERROR");
        return;
    }
    webPortal->begin(true);

    // Show config mode screen
    display.showConfigMode(WiFi.softAPIP().toString());
}

void handleConfigMode()
{
    if (!webPortal)
        return;

    webPortal->handle();

    // Check for timeout
    if (webPortal->hasTimedOut())
    {
        logger.debugPrintln("[ConfigMode] Portal timed out — rebooting");
        webPortal->stop();
        delete webPortal;
        webPortal = nullptr;
        ESP.restart();
    }

    // Check G0 button to exit config mode
    if (M5Cardputer.BtnA.wasPressed())
    {
        logger.debugPrintln("[ConfigMode] G0 pressed — rebooting");
        webPortal->stop();
        delete webPortal;
        webPortal = nullptr;
        ESP.restart();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// HELPERS
// ═══════════════════════════════════════════════════════════════════════════
void processCompletedScan()
{
    session.lastScanCount = wifiScanner.getResults(session.lastResults);
    session.totalSweeps++;

    if (session.lastScanCount > 0)
    {
        session.totalStationsSeen += (uint64_t)session.lastScanCount;
        if ((uint32_t)session.lastScanCount > session.maxStationsPerSweep)
        {
            session.maxStationsPerSweep = (uint32_t)session.lastScanCount;
        }
        session.lastStats = wifiScanner.getSecurityStats(session.lastResults);
    }
}

void logSystemStatsIfDue(unsigned long now)
{
    const AppConfig &cfg = configManager.getConfig();

    // Only log when debug is enabled and system stats are enabled
    if (!cfg.debug.enabled || !cfg.debug.system_stats_enabled)
    {
        return;
    }

    unsigned long intervalMs = (unsigned long)cfg.debug.system_stats_interval_s * 1000UL;
    if (session.lastSystemStatsLog != 0 &&
        (now - session.lastSystemStatsLog < intervalMs))
    {
        return;
    }

    const uint32_t heapSize = ESP.getHeapSize();
    const uint32_t heapFree = ESP.getFreeHeap();
    bool invalidCounters = false;
    uint32_t heapUsed = 0;
    if (heapSize >= heapFree)
    {
        heapUsed = heapSize - heapFree;
    }
    else
    {
        logger.debugPrintln("[SystemStats] Unexpected heap counters: heap_free > heap_size");
        invalidCounters = true;
    }
    const uint32_t minFreeHeap = ESP.getMinFreeHeap();
    const uint32_t maxAllocHeap = ESP.getMaxAllocHeap();
    const uint32_t psramSize = ESP.getPsramSize();
    const uint32_t psramFree = ESP.getFreePsram();
    const uint32_t totalMemoryKnown = heapSize + psramSize;
    const uint32_t totalFreeKnown = heapFree + psramFree;
    uint32_t totalUsedKnown = 0;
    if (totalMemoryKnown >= totalFreeKnown)
    {
        totalUsedKnown = totalMemoryKnown - totalFreeKnown;
    }
    else
    {
        logger.debugPrintln("[SystemStats] Unexpected memory counters: free > total known");
        invalidCounters = true;
    }

    if (invalidCounters)
    {
        logger.debugPrintln("[SystemStats] Skipping metrics line due to invalid counters");
        session.lastSystemStatsLog = now;
        return;
    }
    const uint32_t uniqueStations = (uint32_t)wifiScanner.getUniqueCount();
    const size_t bssidSetEstBytes = wifiScanner.getUniqueBSSIDMemoryEstimate();

    // SYSTEM_STATS_LINE_BUFFER_SIZE bytes leaves margin for current key/value lines with max-width numeric values.
    char line[SYSTEM_STATS_LINE_BUFFER_SIZE];
    int written = snprintf(
        line, sizeof(line),
        "[SystemStats][Memory] uptime_s=%lu mem_used=%lu mem_free=%lu heap_size=%lu heap_used=%lu heap_free=%lu",
        (unsigned long)(now / 1000UL),
        (unsigned long)totalUsedKnown,
        (unsigned long)totalFreeKnown,
        (unsigned long)heapSize,
        (unsigned long)heapUsed,
        (unsigned long)heapFree);
    if (written < 0 || written >= (int)SYSTEM_STATS_LINE_BUFFER_SIZE)
    {
        logger.debugPrintln("[SystemStats] Memory line truncated");
    }
    logger.debugPrintln(line);

    written = snprintf(
        line, sizeof(line),
        "[SystemStats][MemoryExt] min_heap_free=%lu max_alloc_heap=%lu psram_size=%lu psram_free=%lu",
        (unsigned long)minFreeHeap,
        (unsigned long)maxAllocHeap,
        (unsigned long)psramSize,
        (unsigned long)psramFree);
    if (written < 0 || written >= (int)SYSTEM_STATS_LINE_BUFFER_SIZE)
    {
        logger.debugPrintln("[SystemStats] MemoryExt line truncated");
    }
    logger.debugPrintln(line);

    written = snprintf(
        line, sizeof(line),
        "[SystemStats][Stations] max_stations_per_sweep=%lu total_stations_seen=%llu unique_stations=%lu last_scan_count=%d bssid_set_count=%lu bssid_set_est_bytes=%lu",
        (unsigned long)session.maxStationsPerSweep,
        (unsigned long long)session.totalStationsSeen,
        (unsigned long)uniqueStations,
        session.lastScanCount,
        (unsigned long)uniqueStations,
        (unsigned long)bssidSetEstBytes);
    if (written < 0 || written >= (int)SYSTEM_STATS_LINE_BUFFER_SIZE)
    {
        logger.debugPrintln("[SystemStats] Stations line truncated");
    }
    logger.debugPrintln(line);

    // Battery stats
    int battLevel = M5Cardputer.Power.getBatteryLevel();
    bool isCharging = M5Cardputer.Power.isCharging();
    written = snprintf(
        line, sizeof(line),
        "[SystemStats][Battery] batt_pct=%d is_charging=%d",
        battLevel, isCharging ? 1 : 0);
    if (written < 0 || written >= (int)SYSTEM_STATS_LINE_BUFFER_SIZE)
    {
        logger.debugPrintln("[SystemStats] Battery line truncated");
    }
    logger.debugPrintln(line);

    session.lastSystemStatsLog = now;
}

std::vector<WiFiNetwork> getTopNetworks(const std::vector<WiFiNetwork> &nets, int count)
{
    std::vector<WiFiNetwork> sorted = nets;
    std::sort(sorted.begin(), sorted.end(),
              [](const WiFiNetwork &a, const WiFiNetwork &b)
              {
                  return a.rssi > b.rssi;
              });

    if ((int)sorted.size() > count)
    {
        sorted.resize(count);
    }
    return sorted;
}
