#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <vector>
#include <WiFi.h>

// ── Firmware Version ---------------------------------------------
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif

// ── Application Constants ---------------------------------------------
#define APP_NAME "wardriver"
#define APP_DIR "/wardriver"
#define CONFIG_FILENAME "/wardriver/wardriverconfig.txt"
#define DEBUG_LOG_FILENAME "/wardriver/logs/debug.log"
#define DEBUG_LOG_DIR "/wardriver/logs"
#define CSV_DIR "/wardriver"
#define CSV_PREFIX "wardriving_"
#define AP_SSID "M5-Wardriver"

// ── Hardware Pin Constants (M5 Cardputer) ---------------------------------------------
#define SD_SPI_SCK_PIN 40
#define SD_SPI_MISO_PIN 39
#define SD_SPI_MOSI_PIN 14
#define SD_SPI_CS_PIN 12

#define BUZZER_PIN 2
#define LED_PIN 21
#define NUM_LEDS 1
#define LED_BRIGHTNESS 64

// -- Default Configuration Values ---------------------------------------------

#define DEFAULT_GPS_TX 2
#define DEFAULT_GPS_RX 1
#define DEFAULT_GPS_BAUD 115200 // 57600  or 115200
#define DEFAULT_ACCURACY_THRESHOLD 500.0f
#define DEFAULT_GMT_OFFSET_HOURS 0
#define DEFAULT_GPS_LOG_MODE "fix_only"
#define DEFAULT_SCAN_DELAY_MS 150
#define DEFAULT_ACTIVE_DWELL_TIME_MS 150
#define DEFAULT_PASSIVE_DWELL_TIME_MS 250
#define DEFAULT_SCAN_MODE "active_all"
#define DEFAULT_BUZZER_FREQ 2500
#define DEFAULT_BUZZER_DURATION_MS 50
#define DEFAULT_SCREEN_BRIGHTNESS 128
#define DEFAULT_LED_BRIGHTNESS 64
#define DEFAULT_BUZZER_ENABLED true
#define DEFAULT_LOW_BATTERY_THRESHOLD 5
#define DEFAULT_BEEP_ON_NEW_SSID false
#define DEFAULT_BEEP_ON_BLOCKED_SSID false
#define DEFAULT_BEEP_ON_GEOFENCE false
#define DEFAULT_KEYBOARD_SOUNDS true
#define DEFAULT_DEBUG_ENABLED false
#define DEFAULT_SUPER_DEBUG_ENABLED false
#define DEFAULT_SYSTEM_STATS_ENABLED true
#define DEFAULT_SYSTEM_STATS_INTERVAL_S 300 // 5 minutes
#define DEFAULT_ADMIN_USER "admin"
#define DEFAULT_ADMIN_PASS "password"
#define DEFAULT_MAC_RANDOMIZE_INTERVAL 1
#define DEFAULT_MAC_SPOOF_OUI false
#define DEFAULT_WIFI_COUNTRY_CODE "Default"
#define DEFAULT_WIFI_TX_POWER 84

// ── Timing Constants ---------------------------------------------
#define SYSTEM_STATS_LOG_INTERVAL_MS 300000UL // 5 minutes
#define SD_FLUSH_INTERVAL_MS 60000UL          // 1 minute
#define SYSTEM_STATS_LINE_BUFFER_SIZE 192
#define DISPLAY_UPDATE_INTERVAL_MS 500
#define CONFIG_PORTAL_TIMEOUT_MS 300000 // 5 minutes

// ── GPS Constants ---------------------------------------------
#define SATELLITES_FOR_FIX 4
#define HDOP_TO_METERS_MULTIPLIER 2.5f

// ── Buffer Size Constants ---------------------------------------------
#define MAX_SCAN_RESULTS_PER_SWEEP 1024
#define MAX_UNIQUE_BSSIDS_TRACKED 1024 * 10
#define BSSID_SET_ENTRY_ESTIMATED_BYTES 8 // pre-reserved vector: 8B per uint64_t, no per-entry heap overhead

// ── Sound Feedback Constants ---------------------------------------------
#define BEEP_FREQ 2000
#define BOOP_FREQ 1000
#define SHORT_TONE_MS 100
#define QUICK_TONE_MS 50
#define LONG_TONE_MS 100
#define LONG_CONFIG_TONE_MS 250
#define TONE_GAP_MS 60
#define MAX_TONE_SEQUENCE 8
#define GEOFENCE_BEEP_INTERVAL_MS 10000UL

// ── WiGLE CSV Constants ---------------------------------------------
#define WIGLE_PREHEADER_FMT                                                  \
    "WigleWifi-1.6,appRelease=%s,model=Cardputer,release=WarDriver,"         \
    "device=Cardputer,display=stewmoss,board=ESP32-S3,brand=M5Stack," \
    "star=Sol,body=3,subBody=0"

#define WIGLE_CSV_HEADER                                              \
    "MAC,SSID,AuthMode,FirstSeen,Channel,Frequency,RSSI,"             \
    "CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters," \
    "RCOIs,MfgrId,Type"

// ── Display Constants ---------------------------------------------
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 135
#define HEADER_HEIGHT 20
#define FOOTER_HEIGHT 14
#define MAX_RECENT_APS 10

// ── Data Structures ---------------------------------------------

struct GeofenceBox
{
    float topLat;
    float leftLon;
    float bottomLat;
    float rightLon;
};

struct WiFiNetwork
{
    String bssid;
    String ssid;
    wifi_auth_mode_t authMode;
    int channel;
    int frequency;
    int rssi;
};

struct GPSData
{
    double lat;
    double lng;
    float altitude;
    float accuracy; // HDOP value converted to meters (HDOP * 2.5)
    int satellites;
    bool isValid;
    bool has3DFix;
    String dateTime; // "YYYY-MM-DD hh:mm:ss" GMT/UTC timestamp
};

struct SecurityStats
{
    int openCount;
    int wpa2Count;
    int wpa3Count;
    int otherCount;
};

struct RecentEntry
{
    WiFiNetwork net;
    String seenTime; // "HH:MM:SS" extracted from GPS, empty if unavailable
};

struct SessionSummary
{
    uint32_t totalLogged;
    unsigned long sessionDurationMs;
    uint32_t averageScanMs;
    uint32_t maxStationsPerSweep;
    uint64_t totalStationsSeen;
    uint32_t uniqueStations;
    uint32_t totalSweeps;
};

struct ScanSession
{
    bool scanStopped;
    bool loggingPaused;
    std::vector<WiFiNetwork> lastResults;
    SecurityStats lastStats;
    int lastScanCount;
    uint32_t maxStationsPerSweep;
    uint64_t totalStationsSeen;
    uint32_t totalSweeps;
    std::vector<RecentEntry> recentAPs;
    std::vector<RecentEntry> recentUniqueAPs;
    GPSData gps;
    unsigned long bootTime;
    unsigned long startScanTime;
    unsigned long lastScanTime;
    unsigned long lastFlushTime;
    unsigned long lastDisplayUpdate;
    unsigned long lastDashCUpdate;
    unsigned long lastDashDUpdate;
    unsigned long lastDashEUpdate;
    unsigned long lastSystemStatsLog;
    unsigned long lastBatteryCheck;
    unsigned long lowBatteryWarnStart;
    unsigned long lastGeofenceBeep;
    int lastBatteryLevel;

    ScanSession()
    {
        reset();
    }

    void reset()
    {
        scanStopped = false;
        loggingPaused = false;
        lastResults.clear();
        lastStats = SecurityStats();
        lastScanCount = 0;
        maxStationsPerSweep = 0;
        totalStationsSeen = 0;
        totalSweeps = 0;
        recentAPs.clear();
        recentUniqueAPs.clear();
        gps = GPSData();
        bootTime = 0;
        startScanTime = 0;
        lastScanTime = 0;
        lastFlushTime = 0;
        lastDisplayUpdate = 0;
        lastDashCUpdate = 0;
        lastDashDUpdate = 0;
        lastDashEUpdate = 0;
        lastSystemStatsLog = 0;
        lastBatteryCheck = 0;
        lowBatteryWarnStart = 0;
        lastGeofenceBeep = 0;
        lastBatteryLevel = -1;
    }
};

// ── Configuration Structs ───────────────────────────────────────────────────

struct GPSConfig
{
    int tx_pin;
    int rx_pin;
    float accuracy_threshold_meters;
    int gmt_offset_hours;
    String gps_log_mode; // "fix_only", "zero_gps", "last_known"
};

struct ScanConfig
{
    int scan_delay_ms;
    int active_dwell_ms;        // Per-channel dwell time for active scanning in ms
    int passive_dwell_ms;       // Per-channel dwell time for passive scanning in ms
    String scan_mode;           // "active_hop", "active_all", "passive_hop", "passive_all"
    int mac_randomize_interval; // 0 = disabled, 1 = every scan, N = every Nth scan
    bool mac_spoof_oui;         // false = locally-administered MAC, true = can mimic manufacturer OUIs
    String wifi_country_code;   // "Default" = no override, or ISO 3166-1 alpha-2 code (e.g. "US", "GB")
    int wifi_tx_power;          // ESP32 TX power in quarter-dBm units (8-84), 84 = max
};

struct FilterConfig
{
    std::vector<String> excluded_ssids;
    std::vector<String> excluded_bssids;
    std::vector<GeofenceBox> geofence_boxes;
};

struct HardwareConfig
{
    int buzzer_freq;
    int buzzer_duration_ms;
    int screen_brightness;
    int led_brightness;
    bool buzzer_enabled;
    int low_battery_threshold;
    bool beep_on_new_ssid;
    bool beep_on_blocked_ssid;
    bool beep_on_geofence;
    bool keyboard_sounds;
};

struct DebugConfig
{
    bool enabled;
    bool super_debug_enabled;
    bool system_stats_enabled;
    int system_stats_interval_s;
};

struct WebConfig
{
    String admin_user;
    String admin_pass;
};

struct AppConfig
{
    GPSConfig gps;
    ScanConfig scan;
    FilterConfig filter;
    HardwareConfig hardware;
    DebugConfig debug;
    WebConfig web;
};

// ── Application State ───────────────────────────────────────────────────────

enum AppState
{
    STATE_INIT,
    STATE_CONFIG_MODE,
    STATE_SEARCHING_SATS,
    STATE_ACTIVE_SCAN,
    STATE_LOW_BATTERY_WARNING,
    STATE_SHUTDOWN,
    STATE_ERROR
};

enum KeyAction
{
    KEY_ACTION_NONE,
    KEY_ACTION_NEXT_VIEW,
    KEY_ACTION_TOGGLE_BLANK,
    KEY_ACTION_ENTER_CONFIG,
    KEY_ACTION_SHUTDOWN,
    KEY_ACTION_TOGGLE_MUTE,
    KEY_ACTION_SHOW_HELP,
    KEY_ACTION_DISMISS_HELP,
    KEY_ACTION_TOGGLE_SCAN,
    KEY_ACTION_TOGGLE_PAUSE
};

enum DisplayView
{
    VIEW_SEARCHING_SATS, // Pre-fix: satellite search screen
    VIEW_DASHBOARD_A,    // Summary: sats, fix, filename, unique APs, total, uptime, lat/lon
    VIEW_DASHBOARD_B,    // Snapshot: security breakdown, top 3 strongest APs
    VIEW_DASHBOARD_C,    // Live feed: last 10 APs seen (SSID/MAC, time, RSSI)
    VIEW_DASHBOARD_D,    // Discovery: last 10 unique APs first seen (SSID/MAC, time, RSSI)
    VIEW_DASHBOARD_E     // Settings: RAM, battery, sound status
};

#endif // CONFIG_H
