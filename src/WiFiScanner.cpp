#include "WiFiScanner.h"
#include "Logger.h"
#include <M5Cardputer.h>
#include "esp_wifi.h"
#include <algorithm>

WiFiScanner::WiFiScanner()
    : sweepInProgress(false),
      channelScanActive(false),
      currentChannel(WIFI_MIN_CHANNEL),
      scanFailureCount(0),
      sweepStartTime(0),
      passiveMode(false),
      allChannelMode(false),
      activeDwellTimeMs(DEFAULT_ACTIVE_DWELL_TIME_MS),
      passiveDwellTimeMs(DEFAULT_PASSIVE_DWELL_TIME_MS),
      fullScanActive(false),
      macRandomizeInterval(0),
      macSpoofOUI(false),
      scansSinceLastMacChange(0),
      superDebug(false),
      maxChannel(WIFI_MAX_CHANNEL)
{
}

int WiFiScanner::getMaxChannelForCountry(const String &cc)
{
    // Countries limited to channels 1-11
    if (cc == "US" || cc == "CA" || cc == "TW" || cc == "CO" || cc == "DO" ||
        cc == "GT" || cc == "MX" || cc == "PA" || cc == "PR" || cc == "UZ")
    {
        return 11;
    }
    // Japan: channels 1-14
    if (cc == "JP")
    {
        return 14;
    }
    // Most countries (EU, AU, NZ, CN, KR, IN, BR, etc.): channels 1-13
    return 13;
}

void WiFiScanner::applyCountryCode(const String &cc)
{
    if (cc == "Default" || cc.length() < 2)
    {
        maxChannel = WIFI_MAX_CHANNEL;
        return;
    }

    maxChannel = getMaxChannelForCountry(cc);

    wifi_country_t country = {};
    // Copy 2-char code
    country.cc[0] = cc.charAt(0);
    country.cc[1] = cc.charAt(1);
    country.cc[2] = '\0';
    country.schan = 1;
    country.nchan = (uint8_t)maxChannel;
    country.max_tx_power = 84; // Will be overridden by esp_wifi_set_max_tx_power if needed
    country.policy = WIFI_COUNTRY_POLICY_MANUAL;

    esp_err_t err = esp_wifi_set_country(&country);

    char buf[128];
    snprintf(buf, sizeof(buf),
             "[WiFiScanner] Country code set to '%s': channels 1-%d (err=%d)",
             cc.c_str(), maxChannel, err);
    logger.debugPrintln(buf);
}

void WiFiScanner::begin(const String &countryCode, int txPower)
{
    logger.debugPrintln("[WiFiScanner] Begin");

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    esp_wifi_init(&cfg);
    esp_wifi_start();

    // Apply country code (must be done after esp_wifi_start)
    applyCountryCode(countryCode);

    // Set TX power from config
    esp_wifi_set_max_tx_power(txPower);

    char buf[96];
    snprintf(buf, sizeof(buf), "[WiFiScanner] TX power set to %d (%.1f dBm)",
             txPower, txPower / 4.0f);
    logger.debugPrintln(buf);

    WiFi.mode(WIFI_STA);
    initValues();
}

void WiFiScanner::initValues()
{
    uniqueBSSIDs.reserve(MAX_UNIQUE_BSSIDS_TRACKED);

    currentChannel = WIFI_MIN_CHANNEL;
    sweepInProgress = false;
    channelScanActive = false;
    fullScanActive = false;
    scanFailureCount = 0;
    scansSinceLastMacChange = 0;
    sweepResults.clear();
}

void WiFiScanner::setMacRandomizeConfig(int interval, bool spoofOUI)
{
    macRandomizeInterval = (interval >= 0) ? interval : 0;
    macRandomizeInterval = macRandomizeInterval > 65500 ? 0 : macRandomizeInterval; // Cap to almost max uint16_t
    macSpoofOUI = spoofOUI;
    scansSinceLastMacChange = 0;
}

void WiFiScanner::configure(const String &mode, int activeDwellMs, int passiveDwellMs, int randomizeMACInterval, bool randomizeMACSpoofOUI, bool superDebugEnabled)
{
    activeDwellTimeMs = activeDwellMs;
    passiveDwellTimeMs = passiveDwellMs;
    passiveMode = (mode == "passive_hop" || mode == "passive_all");
    allChannelMode = (mode == "active_all" || mode == "passive_all");
    superDebug = superDebugEnabled;
    setMacRandomizeConfig(randomizeMACInterval, randomizeMACSpoofOUI);

    char buf[160];
    snprintf(buf, sizeof(buf),
             "[WiFiScanner] Configure: Mode: %s | passive=%d | all_channel=%d | active_dwell=%dms | passive_dwell=%dms | mac_randomize_interval=%d | mac_spoof_oui=%d",
             mode.c_str(), passiveMode, allChannelMode, activeDwellTimeMs, passiveDwellTimeMs, macRandomizeInterval, macSpoofOUI);
    logger.debugPrintln(buf);
}

void WiFiScanner::randomizeMAC()
{
    uint8_t newMAC[6];
    uint32_t r0 = esp_random();
    uint32_t r1 = esp_random();
    newMAC[0] = (r0 >> 0) & 0xFF;
    newMAC[1] = (r0 >> 8) & 0xFF;
    newMAC[2] = (r0 >> 16) & 0xFF;
    newMAC[3] = (r0 >> 24) & 0xFF;
    newMAC[4] = (r1 >> 0) & 0xFF;
    newMAC[5] = (r1 >> 8) & 0xFF;

    if (macSpoofOUI)
    {
        newMAC[0] &= 0xFE; // Clear multicast bit only
    }
    else
    {
        newMAC[0] = (newMAC[0] & 0xFE) | 0x02; // Locally-administered unicast
    }

    esp_err_t err = esp_wifi_set_mac(WIFI_IF_STA, newMAC);

    if (superDebug)
    {
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "[WiFiScanner] MAC randomized to %02x:%02x:%02x:%02x:%02x:%02x (err=%d)",
                 newMAC[0], newMAC[1], newMAC[2], newMAC[3], newMAC[4], newMAC[5],
                 err);
        logger.debugPrintln(buf);
    }
}

void WiFiScanner::maybeRandomizeMAC()
{
    if (macRandomizeInterval <= 0)
        return;

    scansSinceLastMacChange++;
    if (scansSinceLastMacChange >= (uint32_t)macRandomizeInterval)
    {
        randomizeMAC();
        scansSinceLastMacChange = 0;
    }
}

// ── Start a full sweep across channels 1-13 ────────────────────────────────
void WiFiScanner::startScan()
{
    if (sweepInProgress)
        return;

    maybeRandomizeMAC();

    sweepResults.clear();
    currentChannel = WIFI_MIN_CHANNEL;
    scanFailureCount = 0;
    sweepStartTime = millis();
    sweepInProgress = true;
    startChannelScan();
}

// ── Kick off an async scan on the current channel ──────────────────────────
void WiFiScanner::startChannelScan()
{
    // channel param = currentChannel, so ESP32 only scans that one channel
    uint32_t dwell = passiveMode ? passiveDwellTimeMs : activeDwellTimeMs;
    int result = WiFi.scanNetworks(true, true, passiveMode, dwell, currentChannel);
    if (result == WIFI_SCAN_FAILED)
    {
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "[WiFiScanner] Failed to start scan on channel %d: error %d",
                 currentChannel, result);
        logger.debugPrintln(buf);
    }
    else
    {
        channelScanActive = true;
    }
}

bool WiFiScanner::isScanning()
{
    if (allChannelMode)
    {
        return fullScanActive;
    }
    else
    {
        return sweepInProgress || channelScanActive;
    }
}

bool WiFiScanner::stopScanning()
{
    if (sweepInProgress || channelScanActive || fullScanActive)
    {
        // Abort any in-flight IDF scan. WiFi.scanDelete() alone only frees the
        // cached AP list; it does NOT stop a running scan. Without scan_stop()
        // the IDF scan completes in the background and leaves the Arduino
        // WiFiScanClass state out of sync with our flags, which breaks the
        // next startFullScan()/isFullScanComplete() cycle.
        esp_err_t stopErr = esp_wifi_scan_stop();

        // Drain any pending SCAN_DONE so Arduino's internal state converges
        // before we clear the cache.
        unsigned long t0 = millis();
        int finalArduinoState = WiFi.scanComplete();
        while (finalArduinoState == WIFI_SCAN_RUNNING && (millis() - t0) < 150UL)
        {
            delay(5);
            finalArduinoState = WiFi.scanComplete();
        }

        WiFi.scanDelete();
        sweepInProgress = false;
        channelScanActive = false;
        fullScanActive = false;
        sweepResults.clear();

        if (superDebug)
        {
            char buf[160];
            snprintf(buf, sizeof(buf),
                     "[WiFiScanner] stopScanning: esp_wifi_scan_stop err=%d drain_ms=%lu final_arduino_state=%d",
                     (int)stopErr, (unsigned long)(millis() - t0), finalArduinoState);
            logger.debugPrintln(buf);
        }

        logger.debugPrintln("[WiFiScanner] Scanning stopped");

        return true;
    }
    return false;
}

// ── Start a single all-channel async scan ───────────────────────────────────
void WiFiScanner::startFullScan()
{
    if (fullScanActive)
        return;

    maybeRandomizeMAC();

    sweepResults.clear();
    scanFailureCount = 0;

    // Re-sync Arduino's WiFiScanClass state. startFullScan() calls
    // esp_wifi_scan_start() directly (not WiFi.scanNetworks()), so Arduino's
    // internal scan state machine is never reset by us. If a previous scan
    // left stale state (e.g. after a user stop/start cycle), WiFi.scanComplete()
    // will return stale values and we will either log ghost APs or treat every
    // future scan as instantly-complete with 0 APs. Calling scanDelete() here
    // forces Arduino's state back to a known-empty baseline before we kick
    // off the IDF scan.
    WiFi.scanDelete();

    if (superDebug)
    {
        char dbg[160];
        snprintf(dbg, sizeof(dbg),
                 "[WiFiScanner] startFullScan entry: arduinoScanComplete=%d heap_free=%u",
                 WiFi.scanComplete(), (unsigned)ESP.getFreeHeap());
        logger.debugPrintln(dbg);
    }

    // int result = WiFi.scanNetworks(true, true, passiveMode, dwellTimeMs);

    // Been having trouble, use the esp_wifi_scan_start() directly with a
    // custom config to set the home_chan_dwell_time more reliably.
    wifi_scan_config_t config = {};
    config.channel = 0;
    config.show_hidden = true;

    if (passiveMode)
    {
        config.scan_type = WIFI_SCAN_TYPE_PASSIVE;
        config.scan_time.passive = passiveDwellTimeMs;
    }
    else
    {
        config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
        config.scan_time.active.min = 20;
        config.scan_time.active.max = activeDwellTimeMs;
    }

    config.home_chan_dwell_time = 0;
    // This call is ultimately called by WiFi.scanNetworks() internally.
    esp_err_t result = esp_wifi_scan_start(&config, false);

    if (result != ESP_OK)
    {
        // Any non-ESP_OK return means the scan did not start. Do NOT set
        // fullScanActive = true or the state machine will wait forever for a
        // completion that never comes.
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "[WiFiScanner] Failed to start full scan: err=%d",
                 (int)result);
        logger.debugPrintln(buf);
        fullScanActive = false;
        return;
    }

    if (superDebug)
    {
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "[WiFiScanner] Started full scan result: %d",
                 (int)result);
        logger.debugPrintln(buf);
    }

    sweepStartTime = millis();
    fullScanActive = true;
}

// ── Check if the all-channel scan is done ───────────────────────────────────
bool WiFiScanner::isFullScanComplete()
{
    if (!fullScanActive)
        return false;

    int result = WiFi.scanComplete();

    if (superDebug)
    {
        static unsigned long lastPollLog = 0;
        unsigned long nowMs = millis();
        if (nowMs - lastPollLog > 1000UL)
        {
            char dbg[128];
            snprintf(dbg, sizeof(dbg),
                     "[WiFiScanner] poll scanComplete=%d elapsed=%lums fullScanActive=%d",
                     result, nowMs - sweepStartTime, fullScanActive);
            logger.debugPrintln(dbg);
            lastPollLog = nowMs;
        }
    }

    if (result == WIFI_SCAN_RUNNING || result == WIFI_SCAN_FAILED)
    {
        // Timeout: if stuck for > 10 seconds, force-reset
        if (millis() - sweepStartTime > 10000UL)
        {
            // Abort the IDF scan too; scanDelete() alone does not stop it.
            esp_err_t stopErr = esp_wifi_scan_stop();
            WiFi.scanDelete();
            fullScanActive = false;
            char buf[128];
            snprintf(buf, sizeof(buf),
                     "[WiFiScanner] Full scan hard timeout — resetting (stop err=%d)",
                     (int)stopErr);
            logger.debugPrintln(buf);
        }
        return false;
    }

    if (superDebug)
    {
        unsigned long elapsed = millis() - sweepStartTime;
        char buf[96];
        snprintf(buf, sizeof(buf),
                 "[WiFiScanner] Full scan complete: %d APs in %lums",
                 result, elapsed);
        logger.debugPrintln(buf);
    }

    collectChannelResults(result);

    fullScanActive = false;
    sweepInProgress = false;
    return true;
}

// ── Check if the full channel sweep is done ────────────────────────────────
// This drives the channel-hopping state machine: each call either advances
// to the next channel or signals that the entire sweep has finished.
bool WiFiScanner::isScanComplete()
{
    if (!sweepInProgress)
        return false;
    if (!channelScanActive)
        return false;

    int result = WiFi.scanComplete();

    if (result >= 0)
    {
        // Channel scan succeeded — harvest results
        collectChannelResults(result);
        channelScanActive = false;

        // Advance to next channel or finish sweep
        currentChannel++;
        if (currentChannel > maxChannel)
        {
            if (scanFailureCount > 0)
            {
                char buf[96];
                snprintf(buf, sizeof(buf),
                         "[WiFiScanner] Sweep complete: %d scan failures of %d channels",
                         scanFailureCount, maxChannel);
                logger.debugPrintln(buf);
            }
            sweepInProgress = false;
            return true; // Full sweep complete
        }
        startChannelScan(); // Immediately scan next channel
        return false;
    }
    else if (result == WIFI_SCAN_FAILED)
    {
        // Channel scan failed — clean up and skip to next channel
        WiFi.scanDelete();
        channelScanActive = false;
        scanFailureCount++;

        unsigned long sweepElapsed = millis() - sweepStartTime;
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "[WiFiScanner] Scan failed ch=%d failures_this_sweep=%d sweep_elapsed_ms=%lu",
                 currentChannel, scanFailureCount, sweepElapsed);
        logger.debugPrintln(buf);

        currentChannel++;
        if (currentChannel > maxChannel)
        {
            if (superDebug)
            {
                char summary[96];
                snprintf(summary, sizeof(summary),
                         "[WiFiScanner] Sweep complete: %d scan failures of %d channels",
                         scanFailureCount, maxChannel);
                logger.debugPrintln(summary);
            }

            sweepInProgress = false;
            return true;
        }
        startChannelScan();
        return false;
    }

    // result == -1  ->  scan still running on this channel
    return false;
}

// ── Collect results from the current channel into sweepResults ──────────────
void WiFiScanner::collectChannelResults(int knownCount)
{
    int n = knownCount;
    if (n <= 0)
    {
        WiFi.scanDelete();
        return;
    }

    for (int i = 0; i < n; i++)
    {
        if (sweepResults.size() >= MAX_SCAN_RESULTS_PER_SWEEP)
        {
            if (superDebug)
            {
                logger.debugPrintln("[WiFiScanner] Sweep result limit reached; skipping remaining APs");
            }
            break;
        }

        WiFiNetwork net;
        net.ssid = WiFi.SSID(i);

        // Format BSSID as lowercase colon-separated
        uint8_t *bssidRaw = WiFi.BSSID(i);
        char bssidStr[18];
        snprintf(bssidStr, sizeof(bssidStr), "%02x:%02x:%02x:%02x:%02x:%02x",
                 bssidRaw[0], bssidRaw[1], bssidRaw[2],
                 bssidRaw[3], bssidRaw[4], bssidRaw[5]);
        net.bssid = String(bssidStr);

        net.authMode = WiFi.encryptionType(i);
        net.channel = WiFi.channel(i);
        net.frequency = channelToFrequency(net.channel);
        net.rssi = WiFi.RSSI(i);

        sweepResults.push_back(net);
    }

    WiFi.scanDelete(); // Always free scan memory
}

// ── Return accumulated sweep results ────────────────────────────────────────
int WiFiScanner::getResults(std::vector<WiFiNetwork> &results)
{
    results = sweepResults;
    return results.size();
}

bool WiFiScanner::isNewBSSID(const String &bssid)
{
    uint64_t hash = murmurHash3_64(bssid.c_str(), bssid.length(), 0);

    if (std::find(uniqueBSSIDs.begin(), uniqueBSSIDs.end(), hash) == uniqueBSSIDs.end())
    {
        if (uniqueBSSIDs.size() >= MAX_UNIQUE_BSSIDS_TRACKED)
        {
            if (superDebug)
            {
                logger.debugPrintln("[WiFiScanner] Unique BSSID limit reached; treating as known");
            }
            return false;
        }
        uniqueBSSIDs.push_back(hash);
        return true;
    }
    return false;
}

int WiFiScanner::getUniqueCount()
{
    return uniqueBSSIDs.size();
}

size_t WiFiScanner::getUniqueBSSIDMemoryEstimate()
{
    return uniqueBSSIDs.size() * BSSID_SET_ENTRY_ESTIMATED_BYTES;
}

SecurityStats WiFiScanner::getSecurityStats(const std::vector<WiFiNetwork> &networks)
{
    SecurityStats stats = {0, 0, 0, 0};

    for (const auto &net : networks)
    {
        switch (net.authMode)
        {
        case WIFI_AUTH_OPEN:
            stats.openCount++;
            break;
        case WIFI_AUTH_WPA2_PSK:
        case WIFI_AUTH_WPA2_ENTERPRISE:
        case WIFI_AUTH_WPA_WPA2_PSK:
            stats.wpa2Count++;
            break;
        case WIFI_AUTH_WPA3_PSK:
        case WIFI_AUTH_WPA3_ENT_192:
        case WIFI_AUTH_WPA2_WPA3_PSK:
            stats.wpa3Count++;
            break;
        default:
            stats.otherCount++;
            break;
        }
    }

    return stats;
}

String WiFiScanner::authModeToCapabilities(wifi_auth_mode_t mode)
{
    switch (mode)
    {
    case WIFI_AUTH_OPEN:
        return "[OPEN][ESS]";
    case WIFI_AUTH_WEP:
        return "[WEP][ESS]";
    case WIFI_AUTH_WPA_PSK:
        return "[WPA-PSK-CCMP+TKIP][ESS]";
    case WIFI_AUTH_WPA2_PSK:
        return "[WPA2-PSK-CCMP][ESS]";
    case WIFI_AUTH_WPA_WPA2_PSK:
        return "[WPA-PSK-CCMP+TKIP][WPA2-PSK-CCMP+TKIP][ESS]";
    case WIFI_AUTH_WPA3_ENT_192:
        return "[WPA3-ENT-192][ESS]";
    case WIFI_AUTH_WPA2_ENTERPRISE:
        return "[WPA2-EAP-CCMP][ESS]";
    case WIFI_AUTH_WPA3_PSK:
        return "[WPA3-SAE-CCMP][ESS]";
    case WIFI_AUTH_WPA2_WPA3_PSK:
        return "[WPA2-PSK-CCMP][WPA3-SAE-CCMP][ESS]";
    case WIFI_AUTH_WAPI_PSK:
        return "[WAPI-PSK][ESS]";
    case WIFI_AUTH_MAX:
        return "[ESS]";
    default:
        return "[ESS]";
    }
}

int WiFiScanner::channelToFrequency(int channel)
{
    if (channel >= 1 && channel <= 13)
    {
        return 2412 + (channel - 1) * 5;
    }
    if (channel == 14)
    {
        return 2484;
    }
    // 5GHz channels (unlikely for ESP32 2.4GHz only, but handle gracefully)
    if (channel >= 36)
    {
        return 5000 + channel * 5;
    }
    return 0;
}
