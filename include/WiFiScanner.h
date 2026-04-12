#ifndef WIFI_SCANNER_H
#define WIFI_SCANNER_H

#include <Arduino.h>
#include <WiFi.h>
#include <vector>
#include "Config.h"
#include "MurmurHash3.h"

// Channel-hopping scan constants
#define WIFI_MIN_CHANNEL 1
#define WIFI_MAX_CHANNEL 13

class WiFiScanner
{
public:
    WiFiScanner();
    void begin(const String &countryCode = DEFAULT_WIFI_COUNTRY_CODE, int txPower = DEFAULT_WIFI_TX_POWER);
    void initValues();
    void configure(const String &mode, int activeDwellMs, int passiveDwellMs, int randomizeMACInterval, bool randomizeMACSpoofOUI, bool superDebugEnabled = false);

    // Channel-hopping scan API (active_hop / passive_hop)
    void startScan();      // Begin a new full channel sweep from channel 1
    bool isScanComplete(); // Returns true when the full sweep is done

    // All-channel scan API (active_all / passive_all)
    void startFullScan();      // Begin a single all-channel async scan
    bool isFullScanComplete(); // Returns true when the all-channel scan is done

    bool isAllChannelMode() const { return allChannelMode; }
    int getResults(std::vector<WiFiNetwork> &results);
    bool isNewBSSID(const String &bssid);
    int getUniqueCount();
    size_t getUniqueBSSIDMemoryEstimate();
    int getCurrentChannel() const { return currentChannel; }
    SecurityStats getSecurityStats(const std::vector<WiFiNetwork> &networks);

    static String authModeToCapabilities(wifi_auth_mode_t mode);
    static int channelToFrequency(int channel);
    bool isScanning();

    bool stopScanning();
    void shutdown();
private:
    std::vector<uint64_t> uniqueBSSIDs;
    std::vector<WiFiNetwork> sweepResults; // Accumulated results for the current sweep
    bool sweepInProgress;
    bool channelScanActive;
    int currentChannel;
    int scanFailureCount;
    unsigned long sweepStartTime;

    bool passiveMode;
    bool allChannelMode;
    bool fullScanActive;
    uint32_t activeDwellTimeMs;
    uint32_t passiveDwellTimeMs;

    int macRandomizeInterval;
    bool macSpoofOUI;
    uint32_t scansSinceLastMacChange;
    bool superDebug;
    int maxChannel;
    void randomizeMAC();
    void maybeRandomizeMAC();
    void setMacRandomizeConfig(int interval, bool spoofOUI);

    void startChannelScan();
    void collectChannelResults(int knownCount);
    int getMaxChannelForCountry(const String &cc);
    void applyCountryCode(const String &cc);
};

#endif // WIFI_SCANNER_H
