#include "ConfigManager.h"
#include "Logger.h"
#include <ArduinoJson.h>
#include <SD.h>

namespace
{
bool isValidWifiTxPower(int txPower)
{
    switch (txPower)
    {
    case 8:
    case 20:
    case 28:
    case 34:
    case 44:
    case 52:
    case 56:
    case 60:
    case 66:
    case 72:
    case 76:
    case 78:
    case 80:
    case 84:
        return true;
    default:
        return false;
    }
}
}

ConfigManager::ConfigManager()
    : configValid(false)
{
    setDefaults();
}

void ConfigManager::setDefaults()
{
    config.gps.tx_pin = DEFAULT_GPS_TX;
    config.gps.rx_pin = DEFAULT_GPS_RX;
    config.gps.accuracy_threshold_meters = DEFAULT_ACCURACY_THRESHOLD;
    config.gps.gmt_offset_hours = DEFAULT_GMT_OFFSET_HOURS;
    config.gps.gps_log_mode = DEFAULT_GPS_LOG_MODE;

    config.scan.scan_delay_ms = DEFAULT_SCAN_DELAY_MS;
    config.scan.active_dwell_ms = DEFAULT_ACTIVE_DWELL_TIME_MS;
    config.scan.passive_dwell_ms = DEFAULT_PASSIVE_DWELL_TIME_MS;
    config.scan.scan_mode = DEFAULT_SCAN_MODE;
    config.scan.mac_randomize_interval = DEFAULT_MAC_RANDOMIZE_INTERVAL;
    config.scan.mac_spoof_oui = DEFAULT_MAC_SPOOF_OUI;
    config.scan.wifi_country_code = DEFAULT_WIFI_COUNTRY_CODE;
    config.scan.wifi_tx_power = DEFAULT_WIFI_TX_POWER;

    config.filter.excluded_ssids.clear();
    config.filter.excluded_bssids.clear();
    config.filter.geofence_boxes.clear();

    config.hardware.buzzer_freq = DEFAULT_BUZZER_FREQ;
    config.hardware.buzzer_duration_ms = DEFAULT_BUZZER_DURATION_MS;
    config.hardware.screen_brightness = DEFAULT_SCREEN_BRIGHTNESS;
    config.hardware.led_brightness = DEFAULT_LED_BRIGHTNESS;
    config.hardware.buzzer_enabled = DEFAULT_BUZZER_ENABLED;
    config.hardware.low_battery_threshold = DEFAULT_LOW_BATTERY_THRESHOLD;
    config.hardware.beep_on_new_ssid = DEFAULT_BEEP_ON_NEW_SSID;
    config.hardware.beep_on_blocked_ssid = DEFAULT_BEEP_ON_BLOCKED_SSID;
    config.hardware.beep_on_geofence = DEFAULT_BEEP_ON_GEOFENCE;
    config.hardware.keyboard_sounds = DEFAULT_KEYBOARD_SOUNDS;

    config.debug.enabled = DEFAULT_DEBUG_ENABLED;
    config.debug.super_debug_enabled = DEFAULT_SUPER_DEBUG_ENABLED;
    config.debug.system_stats_enabled = DEFAULT_SYSTEM_STATS_ENABLED;
    config.debug.system_stats_interval_s = DEFAULT_SYSTEM_STATS_INTERVAL_S;

    config.web.admin_user = DEFAULT_ADMIN_USER;
    config.web.admin_pass = DEFAULT_ADMIN_PASS;
}

bool ConfigManager::loadConfig(const char *filename)
{
    setDefaults();

    if (!SD.exists(filename))
    {
        logger.debugPrintln("[Config] Config file not found, using defaults");
        configValid = false;
        return false;
    }

    File file = SD.open(filename, FILE_READ);
    if (!file)
    {
        logger.debugPrintln("[Config] Failed to open config file");
        configValid = false;
        return false;
    }

    DynamicJsonDocument doc(3072);
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error)
    {
        logger.debugPrintln(String("[Config] JSON parse error: ") + error.c_str());
        configValid = false;
        return false;
    }

    // GPS config
    JsonObject gps = doc["gps"];
    if (gps)
    {
        config.gps.tx_pin = gps["tx_pin"] | DEFAULT_GPS_TX;
        config.gps.rx_pin = gps["rx_pin"] | DEFAULT_GPS_RX;
        config.gps.accuracy_threshold_meters = gps["accuracy_threshold_meters"] | DEFAULT_ACCURACY_THRESHOLD;
        config.gps.gmt_offset_hours = gps["gmt_offset_hours"] | DEFAULT_GMT_OFFSET_HOURS;
        if (gps["gps_log_mode"].is<const char *>())
        {
            String mode = gps["gps_log_mode"].as<String>();
            if (mode == "fix_only" || mode == "zero_gps" || mode == "last_known")
            {
                config.gps.gps_log_mode = mode;
            }
        }
    }

    // Scan config
    JsonObject scan = doc["scan"];
    if (scan)
    {
        config.scan.scan_delay_ms = scan["scan_delay_ms"] | DEFAULT_SCAN_DELAY_MS;
        config.scan.active_dwell_ms = scan["active_dwell_ms"] | DEFAULT_ACTIVE_DWELL_TIME_MS;
        config.scan.passive_dwell_ms = scan["passive_dwell_ms"] | DEFAULT_PASSIVE_DWELL_TIME_MS;
        const char *mode = scan["scan_mode"] | DEFAULT_SCAN_MODE;
        String modeStr = String(mode);
        if (modeStr == "active_hop" || modeStr == "active_all" ||
            modeStr == "passive_hop" || modeStr == "passive_all")
        {
            config.scan.scan_mode = modeStr;
        }
        else
        {
            config.scan.scan_mode = DEFAULT_SCAN_MODE;
            logger.debugPrintln("[Config] Unknown scan_mode '" + modeStr + "', using default");
        }
        int macInterval = scan["mac_randomize_interval"] | DEFAULT_MAC_RANDOMIZE_INTERVAL;
        config.scan.mac_randomize_interval = (macInterval >= 0) ? macInterval : 0;
        config.scan.mac_spoof_oui = scan["mac_spoof_oui"] | DEFAULT_MAC_SPOOF_OUI;
        if (scan["wifi_country_code"].is<const char *>())
        {
            config.scan.wifi_country_code = scan["wifi_country_code"].as<String>();
        }
        int wifiTxPower = scan["wifi_tx_power"] | DEFAULT_WIFI_TX_POWER;
        if (isValidWifiTxPower(wifiTxPower))
        {
            config.scan.wifi_tx_power = wifiTxPower;
        }
        else
        {
            config.scan.wifi_tx_power = DEFAULT_WIFI_TX_POWER;
            logger.debugPrintln("[Config] Invalid wifi_tx_power '" + String(wifiTxPower) + "', using default");
        }
    }

    // Filter config
    JsonObject filter = doc["filter"];
    if (filter)
    {
        JsonArray ssids = filter["excluded_ssids"];
        if (ssids)
        {
            config.filter.excluded_ssids.clear();
            for (const char *s : ssids)
            {
                if (s && strlen(s) > 0)
                {
                    config.filter.excluded_ssids.push_back(String(s));
                }
            }
        }

        JsonArray bssids = filter["excluded_bssids"];
        if (bssids)
        {
            config.filter.excluded_bssids.clear();
            for (const char *b : bssids)
            {
                if (b && strlen(b) > 0)
                {
                    config.filter.excluded_bssids.push_back(String(b));
                }
            }
        }

        JsonArray boxes = filter["geofence_boxes"];
        if (boxes)
        {
            config.filter.geofence_boxes.clear();
            for (JsonObject box : boxes)
            {
                GeofenceBox gb;
                gb.topLat = box["top_lat"] | 0.0f;
                gb.leftLon = box["left_lon"] | 0.0f;
                gb.bottomLat = box["bottom_lat"] | 0.0f;
                gb.rightLon = box["right_lon"] | 0.0f;
                config.filter.geofence_boxes.push_back(gb);
            }
        }
    }

    // Hardware config
    JsonObject hw = doc["hardware"];
    if (hw)
    {
        config.hardware.buzzer_freq = hw["buzzer_freq"] | DEFAULT_BUZZER_FREQ;
        config.hardware.buzzer_duration_ms = hw["buzzer_duration_ms"] | DEFAULT_BUZZER_DURATION_MS;
        config.hardware.screen_brightness = hw["screen_brightness"] | DEFAULT_SCREEN_BRIGHTNESS;
        config.hardware.led_brightness = hw["led_brightness"] | DEFAULT_LED_BRIGHTNESS;
        config.hardware.buzzer_enabled = hw["buzzer_enabled"] | DEFAULT_BUZZER_ENABLED;
        config.hardware.low_battery_threshold = hw["low_battery_threshold"] | DEFAULT_LOW_BATTERY_THRESHOLD;
        config.hardware.beep_on_new_ssid = hw["beep_on_new_ssid"] | DEFAULT_BEEP_ON_NEW_SSID;
        config.hardware.beep_on_blocked_ssid = hw["beep_on_blocked_ssid"] | DEFAULT_BEEP_ON_BLOCKED_SSID;
        config.hardware.beep_on_geofence = hw["beep_on_geofence"] | DEFAULT_BEEP_ON_GEOFENCE;
        config.hardware.keyboard_sounds = hw["keyboard_sounds"] | DEFAULT_KEYBOARD_SOUNDS;
    }

    // Debug config
    JsonObject dbg = doc["debug"];
    if (dbg)
    {
        config.debug.enabled = dbg["enabled"] | DEFAULT_DEBUG_ENABLED;
        config.debug.super_debug_enabled = dbg["super_debug_enabled"] | DEFAULT_SUPER_DEBUG_ENABLED;
        config.debug.system_stats_enabled = dbg["system_stats_enabled"] | DEFAULT_SYSTEM_STATS_ENABLED;
        config.debug.system_stats_interval_s = dbg["system_stats_interval_s"] | DEFAULT_SYSTEM_STATS_INTERVAL_S;
    }

    // Web config
    JsonObject web = doc["web"];
    if (web)
    {
        config.web.admin_user = web["admin_user"] | DEFAULT_ADMIN_USER;
        config.web.admin_pass = web["admin_pass"] | DEFAULT_ADMIN_PASS;
    }

    configValid = true;
    logger.debugPrintln("[Config] Config loaded successfully");
    return true;
}

bool ConfigManager::saveConfig(const char *filename)
{
    DynamicJsonDocument doc(3072);

    // GPS
    JsonObject gps = doc.createNestedObject("gps");
    gps["tx_pin"] = config.gps.tx_pin;
    gps["rx_pin"] = config.gps.rx_pin;
    gps["accuracy_threshold_meters"] = config.gps.accuracy_threshold_meters;
    gps["gmt_offset_hours"] = config.gps.gmt_offset_hours;
    gps["gps_log_mode"] = config.gps.gps_log_mode;

    // Scan
    JsonObject scan = doc.createNestedObject("scan");
    scan["scan_delay_ms"] = config.scan.scan_delay_ms;
    scan["active_dwell_ms"] = config.scan.active_dwell_ms;
    scan["passive_dwell_ms"] = config.scan.passive_dwell_ms;
    scan["scan_mode"] = config.scan.scan_mode;
    scan["mac_randomize_interval"] = config.scan.mac_randomize_interval;
    scan["mac_spoof_oui"] = config.scan.mac_spoof_oui;
    scan["wifi_country_code"] = config.scan.wifi_country_code;
    scan["wifi_tx_power"] = config.scan.wifi_tx_power;

    // Filter
    JsonObject filter = doc.createNestedObject("filter");
    JsonArray ssids = filter.createNestedArray("excluded_ssids");
    for (const String &s : config.filter.excluded_ssids)
    {
        ssids.add(s);
    }
    JsonArray bssids = filter.createNestedArray("excluded_bssids");
    for (const String &b : config.filter.excluded_bssids)
    {
        bssids.add(b);
    }
    JsonArray boxes = filter.createNestedArray("geofence_boxes");
    for (const GeofenceBox &gb : config.filter.geofence_boxes)
    {
        JsonObject box = boxes.createNestedObject();
        box["top_lat"] = gb.topLat;
        box["left_lon"] = gb.leftLon;
        box["bottom_lat"] = gb.bottomLat;
        box["right_lon"] = gb.rightLon;
    }

    // Hardware
    JsonObject hw = doc.createNestedObject("hardware");
    hw["buzzer_freq"] = config.hardware.buzzer_freq;
    hw["buzzer_duration_ms"] = config.hardware.buzzer_duration_ms;
    hw["screen_brightness"] = config.hardware.screen_brightness;
    hw["led_brightness"] = config.hardware.led_brightness;
    hw["buzzer_enabled"] = config.hardware.buzzer_enabled;
    hw["low_battery_threshold"] = config.hardware.low_battery_threshold;
    hw["beep_on_new_ssid"] = config.hardware.beep_on_new_ssid;
    hw["beep_on_blocked_ssid"] = config.hardware.beep_on_blocked_ssid;
    hw["beep_on_geofence"] = config.hardware.beep_on_geofence;
    hw["keyboard_sounds"] = config.hardware.keyboard_sounds;

    // Debug
    JsonObject dbg = doc.createNestedObject("debug");
    dbg["enabled"] = config.debug.enabled;
    dbg["super_debug_enabled"] = config.debug.super_debug_enabled;
    dbg["system_stats_enabled"] = config.debug.system_stats_enabled;
    dbg["system_stats_interval_s"] = config.debug.system_stats_interval_s;

    // Web
    JsonObject web = doc.createNestedObject("web");
    web["admin_user"] = config.web.admin_user;
    web["admin_pass"] = config.web.admin_pass;

    File file = SD.open(filename, FILE_WRITE);
    if (!file)
    {
        logger.debugPrintln("[Config] Failed to open config file for writing");
        return false;
    }

    serializeJsonPretty(doc, file);
    file.close();

    logger.debugPrintln("[Config] Config saved successfully");
    return true;
}
