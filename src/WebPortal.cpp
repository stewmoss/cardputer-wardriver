#include "WebPortal.h"
#include "Logger.h"
#include "HardwareProfile.h"
#include <WiFi.h>
#include <SD.h>

static String htmlEscape(const String &s)
{
    String out;
    out.reserve(s.length() * 6);
    for (size_t i = 0; i < s.length(); i++)
    {
        char c = s[i];
        if      (c == '&')  out += F("&amp;");
        else if (c == '<')  out += F("&lt;");
        else if (c == '>')  out += F("&gt;");
        else if (c == '"')  out += F("&quot;");
        else if (c == '\'') out += F("&#39;");
        else                out += c;
    }
    return out;
}

WebPortal::WebPortal(ConfigManager *configMgr)
    : configManager(configMgr), server(80), active(false), apMode(true), lastActivity(0)
{
}

void WebPortal::begin(bool isAPMode)
{
    apMode = isAPMode;

    if (apMode)
    {
        wifiMgr.startAP(AP_SSID);
        // Start DNS server — redirect all queries to AP IP for captive portal
        dnsServer.start(53, "*", WiFi.softAPIP());
        logger.debugPrintln("[WebPortal] Captive portal DNS started");
    }

    // Register routes
    server.on("/", HTTP_GET, [this]()
              { handleRoot(); });
    server.on("/save", HTTP_POST, [this]()
              { handleSave(); });
    server.on("/status", HTTP_GET, [this]()
              { handleStatus(); });

    // Captive portal detection endpoints
    server.on("/generate_204", HTTP_GET, [this]()
              {
        server.sendHeader("Location", "/", true);
        server.send(302, "text/plain", ""); });
    server.on("/hotspot-detect.html", HTTP_GET, [this]()
              {
        server.sendHeader("Location", "/", true);
        server.send(302, "text/plain", ""); });
    server.on("/connecttest.txt", HTTP_GET, [this]()
              {
        server.sendHeader("Location", "/", true);
        server.send(302, "text/plain", ""); });
    server.on("/fwlink", HTTP_GET, [this]()
              {
        server.sendHeader("Location", "/", true);
        server.send(302, "text/plain", ""); });

    server.on("/debuglog", HTTP_GET, [this]()
              { handleDebugLog(); });
    server.on("/cleardebuglog", HTTP_POST, [this]()
              { handleClearDebugLog(); });

    server.onNotFound([this]()
                      { handleNotFound(); });

    server.begin();
    active = true;
    lastActivity = millis();

    logger.debugPrintln(String("[WebPortal] Web server started on ") + getIPAddress());
}

void WebPortal::handle()
{
    if (!active)
        return;

    if (apMode)
    {
        dnsServer.processNextRequest();
    }
    server.handleClient();
}

void WebPortal::stop()
{
    if (!active)
        return;

    server.stop();
    if (apMode)
    {
        dnsServer.stop();
        wifiMgr.stopAP();
    }
    active = false;
    logger.debugPrintln("[WebPortal] Stopped");
}

void WebPortal::resetIdleTimer()
{
    lastActivity = millis();
}

bool WebPortal::hasTimedOut()
{
    return (millis() - lastActivity) >= CONFIG_PORTAL_TIMEOUT_MS;
}

String WebPortal::getIPAddress()
{
    if (apMode)
    {
        return WiFi.softAPIP().toString();
    }
    return WiFi.localIP().toString();
}

void WebPortal::handleRoot()
{
    resetIdleTimer();
    if (!authenticate())
        return;
    sendHTMLChunked();
}

void WebPortal::handleSave()
{
    resetIdleTimer();
    if (!authenticate())
        return;

    AppConfig &config = configManager->getConfig();

    // Device model selection
    if (server.hasArg("device_model"))
    {
        String m = server.arg("device_model");
        if (m == "auto" || m == "cardputer" || m == "cardputer_adv")
        {
            config.device.model = m;
        }
    }

    // GPS settings
    if (server.hasArg("gps_tx"))
        config.gps.tx_pin = server.arg("gps_tx").toInt();
    if (server.hasArg("gps_rx"))
        config.gps.rx_pin = server.arg("gps_rx").toInt();
    if (server.hasArg("accuracy_threshold"))
        config.gps.accuracy_threshold_meters = server.arg("accuracy_threshold").toFloat();
    if (server.hasArg("gmt_offset"))
        config.gps.gmt_offset_hours = server.arg("gmt_offset").toInt();
    if (server.hasArg("gps_log_mode"))
    {
        String mode = server.arg("gps_log_mode");
        if (mode == "fix_only" || mode == "zero_gps" || mode == "last_known")
        {
            config.gps.gps_log_mode = mode;
        }
    }

    // Scan settings
    if (server.hasArg("scan_delay"))
        config.scan.scan_delay_ms = server.arg("scan_delay").toInt();
    if (server.hasArg("active_dwell_ms"))
    {
        int v = server.arg("active_dwell_ms").toInt();
        config.scan.active_dwell_ms = constrain(v, 10, 2000);
    }
    if (server.hasArg("passive_dwell_ms"))
    {
        int v = server.arg("passive_dwell_ms").toInt();
        config.scan.passive_dwell_ms = constrain(v, 10, 2000);
    }
    if (server.hasArg("scan_mode"))
    {
        String mode = server.arg("scan_mode");
        if (mode == "active_hop" || mode == "active_all" ||
            mode == "passive_hop" || mode == "passive_all")
        {
            config.scan.scan_mode = mode;
        }
    }
    if (server.hasArg("mac_randomize_interval"))
    {
        int v = server.arg("mac_randomize_interval").toInt();
        config.scan.mac_randomize_interval = constrain(v, 0, 100);
    }
    config.scan.mac_spoof_oui = server.hasArg("mac_spoof_oui");

    // WiFi country code
    if (server.hasArg("wifi_country_code"))
    {
        String countryCode = server.arg("wifi_country_code");
        countryCode.trim();

        if (countryCode == "Default")
        {
            config.scan.wifi_country_code = countryCode;
        }
        else
        {
            countryCode.toUpperCase();
            bool validCountryCode = (countryCode.length() == 2);

            for (size_t i = 0; validCountryCode && i < 2; ++i)
            {
                char c = countryCode.charAt(i);
                if (c < 'A' || c > 'Z')
                {
                    validCountryCode = false;
                }
            }

            config.scan.wifi_country_code = validCountryCode ? countryCode : DEFAULT_WIFI_COUNTRY_CODE;
        }
    }

    // WiFi TX power
    if (server.hasArg("wifi_tx_power"))
    {
        int v = server.arg("wifi_tx_power").toInt();
        // Only allow valid ESP32 power levels
        const int validPowers[] = {84, 78, 72, 66, 60, 52, 44, 34, 28, 20, 8};
        bool valid = false;
        for (int p : validPowers)
        {
            if (v == p) { valid = true; break; }
        }
        config.scan.wifi_tx_power = valid ? v : DEFAULT_WIFI_TX_POWER;
    }

    // Filter settings — excluded SSIDs (comma-separated)
    if (server.hasArg("excluded_ssids"))
    {
        config.filter.excluded_ssids.clear();
        String raw = server.arg("excluded_ssids");
        raw.trim();
        if (raw.length() > 0)
        {
            int start = 0;
            int idx;
            while ((idx = raw.indexOf(',', start)) >= 0)
            {
                String item = raw.substring(start, idx);
                item.trim();
                if (item.length() > 0)
                    config.filter.excluded_ssids.push_back(item);
                start = idx + 1;
            }
            String last = raw.substring(start);
            last.trim();
            if (last.length() > 0)
                config.filter.excluded_ssids.push_back(last);
        }
    }

    // Excluded BSSIDs (comma-separated)
    if (server.hasArg("excluded_bssids"))
    {
        config.filter.excluded_bssids.clear();
        String raw = server.arg("excluded_bssids");
        raw.trim();
        if (raw.length() > 0)
        {
            int start = 0;
            int idx;
            while ((idx = raw.indexOf(',', start)) >= 0)
            {
                String item = raw.substring(start, idx);
                item.trim();
                if (item.length() > 0)
                    config.filter.excluded_bssids.push_back(item);
                start = idx + 1;
            }
            String last = raw.substring(start);
            last.trim();
            if (last.length() > 0)
                config.filter.excluded_bssids.push_back(last);
        }
    }

    // Geofence boxes (semicolon-separated, each: top_lat,left_lon,bottom_lat,right_lon)
    if (server.hasArg("geofence_boxes"))
    {
        config.filter.geofence_boxes.clear();
        String raw = server.arg("geofence_boxes");
        raw.trim();
        if (raw.length() > 0)
        {
            int bStart = 0;
            int bIdx;
            while ((bIdx = raw.indexOf(';', bStart)) >= 0)
            {
                String boxStr = raw.substring(bStart, bIdx);
                boxStr.trim();
                // Parse 4 comma-separated values
                float vals[4];
                int vIdx = 0;
                int vStart = 0;
                int cIdx;
                while ((cIdx = boxStr.indexOf(',', vStart)) >= 0 && vIdx < 4)
                {
                    vals[vIdx++] = boxStr.substring(vStart, cIdx).toFloat();
                    vStart = cIdx + 1;
                }
                if (vIdx < 4)
                    vals[vIdx++] = boxStr.substring(vStart).toFloat();
                if (vIdx == 4)
                {
                    GeofenceBox gb;
                    gb.topLat = vals[0];
                    gb.leftLon = vals[1];
                    gb.bottomLat = vals[2];
                    gb.rightLon = vals[3];
                    config.filter.geofence_boxes.push_back(gb);
                }
                bStart = bIdx + 1;
            }
            // Last box (no trailing semicolon)
            String boxStr = raw.substring(bStart);
            boxStr.trim();
            if (boxStr.length() > 0)
            {
                float vals[4];
                int vIdx = 0;
                int vStart2 = 0;
                int cIdx2;
                while ((cIdx2 = boxStr.indexOf(',', vStart2)) >= 0 && vIdx < 4)
                {
                    vals[vIdx++] = boxStr.substring(vStart2, cIdx2).toFloat();
                    vStart2 = cIdx2 + 1;
                }
                if (vIdx < 4)
                    vals[vIdx++] = boxStr.substring(vStart2).toFloat();
                if (vIdx == 4)
                {
                    GeofenceBox gb;
                    gb.topLat = vals[0];
                    gb.leftLon = vals[1];
                    gb.bottomLat = vals[2];
                    gb.rightLon = vals[3];
                    config.filter.geofence_boxes.push_back(gb);
                }
            }
        }
    }

    // Hardware settings
    if (server.hasArg("buzzer_freq"))
        config.hardware.buzzer_freq = server.arg("buzzer_freq").toInt();
    if (server.hasArg("buzzer_duration"))
        config.hardware.buzzer_duration_ms = server.arg("buzzer_duration").toInt();
    if (server.hasArg("screen_brightness"))
        config.hardware.screen_brightness = server.arg("screen_brightness").toInt();
    if (server.hasArg("led_brightness"))
        config.hardware.led_brightness = server.arg("led_brightness").toInt();
    config.hardware.buzzer_enabled = server.hasArg("buzzer_enabled");
    config.hardware.keyboard_sounds = server.hasArg("keyboard_sounds");
    config.hardware.beep_on_new_ssid = server.hasArg("beep_on_new_ssid");
    config.hardware.beep_on_blocked_ssid = server.hasArg("beep_on_blocked_ssid");
    config.hardware.beep_on_geofence = server.hasArg("beep_on_geofence");
    if (server.hasArg("low_battery_threshold"))
    {
        int v = server.arg("low_battery_threshold").toInt();
        config.hardware.low_battery_threshold = constrain(v, 0, 100);
    }

    // Debug settings
    config.debug.enabled = server.hasArg("debug_enabled");
    config.debug.super_debug_enabled = server.hasArg("super_debug_enabled");
    config.debug.system_stats_enabled = server.hasArg("sys_stats_en");
    if (server.hasArg("sys_stats_interval"))
    {
        int v = server.arg("sys_stats_interval").toInt();
        config.debug.system_stats_interval_s = constrain(v, 30, 3600);
    }

    // Upload settings
    config.upload.wigle_upload_enabled = server.hasArg("wigle_upload_enabled");
    config.upload.auto_upload = server.hasArg("auto_upload");
    if (server.hasArg("upload_ssid"))
    {
        String value = server.arg("upload_ssid");
        value.trim();
        config.upload.upload_ssid = value.substring(0, 32);
    }
    if (server.hasArg("upload_password_clear") && server.arg("upload_password_clear") == "1")
    {
        config.upload.upload_password = "";
    }
    else if (server.hasArg("upload_password"))
    {
        String value = server.arg("upload_password");
        if (value.length() > 0)
        {
            config.upload.upload_password = value.substring(0, 63);
        }
    }
    if (server.hasArg("wigle_api_name"))
    {
        String value = server.arg("wigle_api_name");
        value.trim();
        config.upload.wigle_api_name = value.substring(0, 64);
    }
    if (server.hasArg("wigle_api_token_clear") && server.arg("wigle_api_token_clear") == "1")
    {
        config.upload.wigle_api_token = "";
    }
    else if (server.hasArg("wigle_api_token"))
    {
        String value = server.arg("wigle_api_token");
        value.trim();
        if (value.length() > 0)
        {
            config.upload.wigle_api_token = value.substring(0, 128);
        }
    }
    config.upload.compress_before_upload = server.hasArg("compress_before_upload");
    config.upload.delete_after_upload = server.hasArg("delete_after_upload");
    if (server.hasArg("min_sweeps_threshold"))
    {
        long v = server.arg("min_sweeps_threshold").toInt();
        if (v < 0) v = 0;
        if (v > 65535) v = 65535;
        config.upload.min_sweeps_threshold = (uint16_t)v;
    }
    config.upload.retry_thin_files = server.hasArg("retry_thin_files");

    // Web authentication settings
    if (server.hasArg("admin_user"))
        config.web.admin_user = server.arg("admin_user");

    if (server.hasArg("admin_pass"))
    {
        String newPass = server.arg("admin_pass");
        if (newPass.length() > 0)
        {
            config.web.admin_pass = newPass;
        }
    }

     // Reseed profile-driven pin defaults only when the submitted form includes
     // a device model update. This avoids overwriting manual GPS pin overrides
     // when the user saves unrelated settings, especially for older configs
     // where detected_model may be empty or missing.
     if (server.hasArg("device_model"))
     {
        CardputerModel detected = detectCardputerModel();
        CardputerModel effective = configManager->effectiveModel(detected);
        const HardwareProfile &profile = getProfile(effective);
        if (configManager->reseedFromProfile(profile, effective))
        {
            logger.debugPrintln("[WebPortal] Hardware profile changed — reseeded pin defaults");
        }
    }

    // Save to SD
    bool saved = configManager->saveConfig();

    String html = "<!DOCTYPE html><html><head>"
                  "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                  "<style>body{font-family:sans-serif;background:#1a1a2e;color:#e0e0e0;"
                  "text-align:center;padding:40px;}"
                  "h2{color:#00d4ff;}</style></head><body>"
                  "<h2>";
    html += saved ? "Settings Saved!" : "Save Failed!";
    html += "</h2><p>Device will reboot in 3 seconds...</p>"
            "</body></html>";

    server.send(200, "text/html", html);

    if (saved)
    {
        delay(3000);
        ESP.restart();
    }
}

void WebPortal::handleStatus()
{
    resetIdleTimer();
    if (!authenticate())
        return;

    AppConfig &cfg = configManager->getConfig();
    bool uploadConfigured = cfg.upload.upload_ssid.length() > 0 &&
                            cfg.upload.wigle_api_name.length() > 0 &&
                            cfg.upload.wigle_api_token.length() > 0;

    String json = "{";
    json += "\"firmware\":\"" FIRMWARE_VERSION "\",";
    json += "\"uptime\":" + String(millis() / 1000) + ",";
    json += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"sd_mounted\":true,";
    json += "\"upload\":{";
    json += "\"enabled\":" + String(cfg.upload.wigle_upload_enabled ? "true" : "false") + ",";
    json += "\"auto_upload\":" + String(cfg.upload.auto_upload ? "true" : "false") + ",";
    json += "\"configured\":" + String(uploadConfigured ? "true" : "false") + ",";
    json += "\"min_sweeps_threshold\":" + String(cfg.upload.min_sweeps_threshold) + ",";
    json += "\"retry_thin_files\":" + String(cfg.upload.retry_thin_files ? "true" : "false") + ",";
    json += "\"last_upload_iso\":null";
    json += "}";
    json += "}";

    server.send(200, "application/json", json);
}

void WebPortal::handleNotFound()
{
    // Redirect everything to root for captive portal
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
}

bool WebPortal::authenticate()
{
    AppConfig &cfg = configManager->getConfig();
    if (cfg.web.admin_user.length() > 0 && cfg.web.admin_pass.length() > 0)
    {
        if (!server.authenticate(cfg.web.admin_user.c_str(), cfg.web.admin_pass.c_str()))
        {
            server.requestAuthentication();
            return false;
        }
    }
    return true;
}

void WebPortal::sendHTMLChunked()
{
    AppConfig &cfg = configManager->getConfig();

    // Build filter strings
    String ssidsStr;
    for (size_t i = 0; i < cfg.filter.excluded_ssids.size(); i++)
    {
        if (i > 0)
            ssidsStr += ", ";
        ssidsStr += cfg.filter.excluded_ssids[i];
    }
    String bssidsStr;
    for (size_t i = 0; i < cfg.filter.excluded_bssids.size(); i++)
    {
        if (i > 0)
            bssidsStr += ", ";
        bssidsStr += cfg.filter.excluded_bssids[i];
    }
    String geoStr;
    for (size_t i = 0; i < cfg.filter.geofence_boxes.size(); i++)
    {
        if (i > 0)
            geoStr += "; ";
        const GeofenceBox &gb = cfg.filter.geofence_boxes[i];
        geoStr += String(gb.topLat, 6) + "," + String(gb.leftLon, 6) + "," +
                  String(gb.bottomLat, 6) + "," + String(gb.rightLon, 6);
    }

    // Build entire HTML into a single String to avoid chunked transfer
    // encoding issues over ESP32 soft AP. PSRAM provides ample heap.
    String html;
    html.reserve(5120);

    // --- Head & CSS ---
    html += F("<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n"
              "<meta charset=\"UTF-8\">\n"
              "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
              "<title>Cardputer Wardriver Config</title>\n<style>\n"
              "*{box-sizing:border-box;margin:0;padding:0}\n"
              "body{font-family:'Segoe UI',sans-serif;background:#0f0f23;color:#e0e0e0;padding:16px}\n"
              "h1{color:#00d4ff;text-align:center;margin-bottom:8px;font-size:1.4em}\n"
              ".ver{text-align:center;color:#666;font-size:0.85em;margin-bottom:20px}\n"
              ".section{background:#1a1a2e;border:1px solid #333;border-radius:8px;padding:16px;margin-bottom:16px}\n"
              ".section h2{color:#00d4ff;font-size:1.1em;margin-bottom:12px;border-bottom:1px solid #333;padding-bottom:6px}\n"
              "label{display:block;margin-bottom:4px;color:#aaa;font-size:0.85em}\n"
              "input[type=text],input[type=password],input[type=number]{width:100%;padding:8px;background:#0f0f23;border:1px solid #444;\n"
              "border-radius:4px;color:#e0e0e0;font-size:0.95em;margin-bottom:12px}\n"
              "input[type=number]{width:120px}\n"
              "textarea{width:100%;padding:8px;background:#0f0f23;border:1px solid #444;\n"
              "border-radius:4px;color:#e0e0e0;font-size:0.85em;margin-bottom:12px;min-height:60px;resize:vertical}\n"
              ".cb{display:flex;align-items:center;gap:8px;margin-bottom:12px}\n"
              ".cb input{width:18px;height:18px;accent-color:#00d4ff}\n"
              ".cb label{margin:0;color:#e0e0e0;font-size:0.95em}\n"
              ".row{display:flex;gap:12px}\n"
              ".row>div{flex:1}\n"
              "input[type=range]{width:100%;accent-color:#00d4ff;margin-bottom:4px}\n"
              ".range-val{text-align:center;color:#00d4ff;font-size:0.85em;margin-bottom:12px}\n"
              ".hint{color:#666;font-size:0.78em;margin-top:-8px;margin-bottom:12px}\n"
              "button{display:block;width:100%;padding:14px;background:#00d4ff;color:#0f0f23;\n"
              "border:none;border-radius:8px;font-size:1.1em;font-weight:bold;cursor:pointer;margin-top:8px}\n"
              "button:hover{background:#00b8d4}\n"
              ".secret-row{display:flex;gap:8px;align-items:flex-start}\n"
              ".secret-row input{flex:1;min-width:0;margin-bottom:8px}\n"
              "button.secret-clear{display:inline-block;width:auto;min-width:68px;padding:8px 10px;"
              "margin-top:0;border-radius:4px;font-size:0.9em}\n"
              ".secret-note{color:#8a8;font-size:0.78em;margin-top:-8px;margin-bottom:12px}\n"
              "</style>\n</head>\n<body>\n");

    // --- Title & version ---
    html += F("<h1>Cardputer Wardriver</h1>\n<div class=\"ver\">Firmware v");
    html += FIRMWARE_VERSION;
    html += F("</div>\n<form method=\"POST\" action=\"/save\">\n");

    // --- Device section (first) ---
    {
        CardputerModel detected = detectCardputerModel();
        CardputerModel effective = configManager->effectiveModel(detected);
        const char *detectedName = (detected == MODEL_UNKNOWN) ? "Unknown"
                                                               : getProfile(detected).model_name;
        const char *effectiveName = getProfile(effective).model_name;

        html += F("<div class=\"section\">\n<h2>Device</h2>\n"
                  "<label for=\"device_model\">Hardware Model</label>\n"
                  "<select id=\"device_model\" name=\"device_model\" "
                  "style=\"width:100%;padding:8px;background:#0f0f23;border:1px solid #444;"
                  "border-radius:4px;color:#e0e0e0;font-size:0.95em;margin-bottom:12px\">");
        const char *modelKeys[]   = {"auto",                    "cardputer",          "cardputer_adv"};
        const char *modelLabels[] = {"Auto-detect (recommended)", "Cardputer (v1.1)", "Cardputer ADV"};
        for (int i = 0; i < 3; i++)
        {
            html += F("<option value=\"");
            html += modelKeys[i];
            html += F("\"");
            if (cfg.device.model == modelKeys[i])
                html += F(" selected");
            html += F(">");
            html += modelLabels[i];
            html += F("</option>");
        }
        html += F("</select>\n"
                  "<div class=\"hint\">Detected: <b>");
        html += detectedName;
        html += F("</b> &middot; Active: <b>");
        html += effectiveName;
        html += F("</b></div>\n");

        // Show each profile's default GPS pins and wire up a small script
        // that auto-resets the GPS TX/RX inputs when the model changes.
        const HardwareProfile &pCard = getProfile(MODEL_CARDPUTER);
        const HardwareProfile &pAdv  = getProfile(MODEL_CARDPUTER_ADV);
        html += F("<div class=\"hint\">Default GPS pins &mdash; "
                  "Cardputer (v1.1): TX=<b>");
        html += String(pCard.gps_tx_pin);
        html += F("</b> RX=<b>");
        html += String(pCard.gps_rx_pin);
        html += F("</b> &middot; Cardputer ADV: TX=<b>");
        html += String(pAdv.gps_tx_pin);
        html += F("</b> RX=<b>");
        html += String(pAdv.gps_rx_pin);
        html += F("</b></div>\n"
                  "<div class=\"hint\" style=\"color:#e0b040\">&#9888; Changing this "
                  "will reset the GPS TX/RX pin fields below to the selected "
                  "profile's defaults.</div>\n"
                  "<script>\n(function(){\n"
                  "var defaults={"
                  "'cardputer':{tx:");
        html += String(pCard.gps_tx_pin);
        html += F(",rx:");
        html += String(pCard.gps_rx_pin);
        html += F("},'cardputer_adv':{tx:");
        html += String(pAdv.gps_tx_pin);
        html += F(",rx:");
        html += String(pAdv.gps_rx_pin);
        html += F("}};\n"
                  "var autoKey='");
        html += modelKey(effective);
        html += F("';\n"
                  "var sel=document.getElementById('device_model');\n"
                  "var tx=document.getElementById('gps_tx');\n"
                  "var rx=document.getElementById('gps_rx');\n"
                  "if(!sel||!tx||!rx)return;\n"
                  "sel.addEventListener('change',function(){\n"
                  "var k=sel.value==='auto'?autoKey:sel.value;\n"
                  "var d=defaults[k];if(!d)return;\n"
                  "tx.value=d.tx;rx.value=d.rx;\n"
                  "});\n"
                  "})();\n</script>\n"
                  "</div>\n");
    }

    // --- GPS section ---
    html += F("<div class=\"section\">\n<h2>GPS Settings</h2>\n<div class=\"row\">\n<div>\n"
              "<label for=\"gps_tx\">TX Pin</label>\n"
              "<input type=\"number\" id=\"gps_tx\" name=\"gps_tx\" value=\"");
    html += String(cfg.gps.tx_pin);
    html += F("\" min=\"0\" max=\"48\">\n</div>\n<div>\n"
              "<label for=\"gps_rx\">RX Pin</label>\n"
              "<input type=\"number\" id=\"gps_rx\" name=\"gps_rx\" value=\"");
    html += String(cfg.gps.rx_pin);
    html += F("\" min=\"0\" max=\"48\">\n</div>\n</div>\n"
              "<label for=\"accuracy_threshold\">Accuracy Threshold (meters)</label>\n"
              "<input type=\"number\" id=\"accuracy_threshold\" name=\"accuracy_threshold\" value=\"");
    html += String(cfg.gps.accuracy_threshold_meters, 0);
    html += F("\" min=\"1\" max=\"1500\" step=\"1\">\n"
              "<div class=\"hint\">Logging suppressed when Accuracy exceeds this value</div>\n"
              "<label for=\"gmt_offset\">GMT Offset (hours)</label>\n"
              "<input type=\"number\" id=\"gmt_offset\" name=\"gmt_offset\" value=\"");
    html += String(cfg.gps.gmt_offset_hours);
    html += F("\" min=\"-12\" max=\"14\" step=\"1\">\n"
              "<div class=\"hint\">Timezone offset from UTC for system clock (e.g. -5 for EST, 10 for AEST)</div>\n"
              "<label for=\"gps_log_mode\">GPS Logging Mode</label>\n"
              "<select id=\"gps_log_mode\" name=\"gps_log_mode\" "
              "style=\"width:100%;padding:8px;background:#0f0f23;border:1px solid #444;"
              "border-radius:4px;color:#e0e0e0;font-size:0.95em;margin-bottom:12px\">");
    {
        const char *gpsLogModes[] = {"fix_only", "zero_gps", "last_known"};
        const char *gpsLogLabels[] = {"Fix Only (require GPS)", "Zero GPS (log without coordinates)", "Last Known (use stale coordinates)"};
        for (int i = 0; i < 3; i++)
        {
            html += F("<option value=\"");
            html += gpsLogModes[i];
            html += F("\"");
            if (cfg.gps.gps_log_mode == gpsLogModes[i])
                html += F(" selected");
            html += F(">");
            html += gpsLogLabels[i];
            html += F("</option>");
        }
    }
    html += F("</select>\n"
              "<div class=\"hint\">Fix Only: log only with GPS fix. Zero GPS: log with 0,0 coordinates when no fix. "
              "Last Known: reuse last valid GPS position when fix is lost.</div>\n</div>\n");

    // --- Scan section ---
    html += F("<div class=\"section\">\n<h2>Scanning</h2>\n"
              "<label for=\"scan_mode\">Scan Mode</label>\n"
              "<select id=\"scan_mode\" name=\"scan_mode\" "
              "style=\"width:100%;padding:8px;background:#0f0f23;border:1px solid #444;"
              "border-radius:4px;color:#e0e0e0;font-size:0.95em;margin-bottom:12px\">");
    const char *modes[] = {"active_hop", "active_all", "passive_hop", "passive_all"};
    const char *labels[] = {"Active Hop (per-channel)", "Active All (single scan)",
                            "Passive Hop (per-channel)", "Passive All (single scan)"};
    for (int i = 0; i < 4; i++)
    {
        html += F("<option value=\"");
        html += modes[i];
        html += F("\"");
        if (cfg.scan.scan_mode == modes[i])
            html += F(" selected");
        html += F(">");
        html += labels[i];
        html += F("</option>");
    }
    html += F("</select>\n"
              "<div class=\"hint\">Active sends probe requests; Passive listens for beacons (slower). "
              "Hop scans one channel at a time; All scans every channel in one operation.</div>\n"
              "<div class=\"row\">\n<div>\n"
              "<label for=\"active_dwell_ms\">Active Dwell per Channel (ms) A good number is about 150ms</label>\n"
              "<input type=\"number\" id=\"active_dwell_ms\" name=\"active_dwell_ms\" value=\"");
    html += String(cfg.scan.active_dwell_ms);
    html += F("\" min=\"10\" max=\"2000\" step=\"10\">\n"
              "<div class=\"hint\">Dwell for active (probe-request) scans. 100\u2013200ms typical.</div>\n"
              "</div>\n<div>\n"
              "<label for=\"passive_dwell_ms\">Passive Dwell per Channel (ms) A good number is about 250ms</label>\n"
              "<input type=\"number\" id=\"passive_dwell_ms\" name=\"passive_dwell_ms\" value=\"");
    html += String(cfg.scan.passive_dwell_ms);
    html += F("\" min=\"10\" max=\"2000\" step=\"10\">\n"
              "<div class=\"hint\">Dwell for passive (beacon-listen) scans. 220\u2013250ms recommended.</div>\n"
              "</div>\n</div>\n"
              "<label for=\"scan_delay\">Delay Between Scans (ms)</label>\n"
              "<input type=\"number\" id=\"scan_delay\" name=\"scan_delay\" value=\"");
    html += String(cfg.scan.scan_delay_ms);
    html += F("\" min=\"10\" max=\"5000\" step=\"10\">\n"
              "<label for=\"mac_randomize_interval\">MAC Randomize Interval</label>\n"
              "<input type=\"number\" id=\"mac_randomize_interval\" name=\"mac_randomize_interval\" value=\"");
    html += String(cfg.scan.mac_randomize_interval);
    html += F("\" min=\"0\" max=\"100\" step=\"1\">\n"
              "<div class=\"hint\">Randomize device MAC before scanning. 0 = disabled, 1 = every scan, N = every Nth scan.</div>\n"
              "<div class=\"cb\">\n"
              "<input type=\"checkbox\" id=\"mac_spoof_oui\" name=\"mac_spoof_oui\"");
    if (cfg.scan.mac_spoof_oui)
        html += F(" checked");
    html += F(">\n<label for=\"mac_spoof_oui\">Spoof Manufacturer OUI</label>\n</div>\n"
              "<div class=\"hint\">When enabled, randomized MACs can mimic real manufacturer addresses. "
              "When disabled, MACs are flagged as locally-administered (safer, but detectable).</div>\n"

              "<label for=\"wifi_country_code\">WiFi Country Code</label>\n"
              "<select id=\"wifi_country_code\" name=\"wifi_country_code\" "
              "style=\"width:100%;padding:8px;background:#0f0f23;border:1px solid #444;"
              "border-radius:4px;color:#e0e0e0;font-size:0.95em;margin-bottom:12px\">");
    {
        const char *countryCodes[] = {
            "Default", "AT", "AU", "BE", "BG", "BR", "CA", "CH", "CN", "CO",
            "CY", "CZ", "DE", "DK", "DO", "EE", "ES", "FI", "FR", "GB",
            "GR", "GT", "HK", "HR", "HU", "IE", "IN", "IS", "IT", "JP",
            "KR", "LI", "LT", "LU", "LV", "MT", "MX", "NL", "NO", "NZ",
            "PA", "PL", "PR", "PT", "RO", "SE", "SG", "SI", "SK", "TW",
            "US", "UZ", "ZA"
        };
        const char *countryLabels[] = {
            "Default (no override)", "AT - Austria", "AU - Australia", "BE - Belgium",
            "BG - Bulgaria", "BR - Brazil", "CA - Canada", "CH - Switzerland",
            "CN - China", "CO - Colombia", "CY - Cyprus", "CZ - Czech Republic",
            "DE - Germany", "DK - Denmark", "DO - Dominican Republic", "EE - Estonia",
            "ES - Spain", "FI - Finland", "FR - France", "GB - United Kingdom",
            "GR - Greece", "GT - Guatemala", "HK - Hong Kong", "HR - Croatia",
            "HU - Hungary", "IE - Ireland", "IN - India", "IS - Iceland",
            "IT - Italy", "JP - Japan", "KR - South Korea", "LI - Liechtenstein",
            "LT - Lithuania", "LU - Luxembourg", "LV - Latvia", "MT - Malta",
            "MX - Mexico", "NL - Netherlands", "NO - Norway", "NZ - New Zealand",
            "PA - Panama", "PL - Poland", "PR - Puerto Rico", "PT - Portugal",
            "RO - Romania", "SE - Sweden", "SG - Singapore", "SI - Slovenia",
            "SK - Slovakia", "TW - Taiwan", "US - United States", "UZ - Uzbekistan",
            "ZA - South Africa"
        };
        const size_t countryCount = sizeof(countryCodes) / sizeof(countryCodes[0]);
        static_assert(
            (sizeof(countryCodes) / sizeof(countryCodes[0])) ==
                (sizeof(countryLabels) / sizeof(countryLabels[0])),
            "countryCodes and countryLabels must have the same number of entries");
        for (size_t i = 0; i < countryCount; ++i)
        {
            html += F("<option value=\"");
            html += countryCodes[i];
            html += F("\"");
            if (cfg.scan.wifi_country_code == countryCodes[i])
                html += F(" selected");
            html += F(">");
            html += countryLabels[i];
            html += F("</option>");
        }
    }
    html += F("</select>\n"
              "<div class=\"hint\">Set WiFi country code for channel/power compliance. "
              "US/CA/TW = channels 1-11, JP = 1-14, most others = 1-13. "
              "Default uses ESP32 built-in settings.</div>\n"

              "<label for=\"wifi_tx_power\">WiFi TX Power</label>\n"
              "<select id=\"wifi_tx_power\" name=\"wifi_tx_power\" "
              "style=\"width:100%;padding:8px;background:#0f0f23;border:1px solid #444;"
              "border-radius:4px;color:#e0e0e0;font-size:0.95em;margin-bottom:12px\">");
    {
        const int txValues[] =   {84,   78,    72,   66,    60,   52,   44,   34,    28,  20,  8};
        const char *txLabels[] = {"84 (21 dBm - Max)", "78 (19.5 dBm)", "72 (18 dBm)",
                                  "66 (16.5 dBm)", "60 (15 dBm)", "52 (13 dBm)",
                                  "44 (11 dBm)", "34 (8.5 dBm)", "28 (7 dBm)",
                                  "20 (5 dBm)", "8 (2 dBm)"};
        for (int i = 0; i < 11; i++)
        {
            html += F("<option value=\"");
            html += String(txValues[i]);
            html += F("\"");
            if (cfg.scan.wifi_tx_power == txValues[i])
                html += F(" selected");
            html += F(">");
            html += txLabels[i];
            html += F("</option>");
        }
    }
    html += F("</select>\n"
              "<div class=\"hint\">Transmit power in quarter-dBm units. Higher = more range but more power consumption.</div>\n"
              "</div>\n");

    // --- Filter section ---
    html += F("<div class=\"section\">\n<h2>Filters</h2>\n"
              "<label for=\"excluded_ssids\">Excluded SSIDs</label>\n"
              "<textarea id=\"excluded_ssids\" name=\"excluded_ssids\" "
              "placeholder=\"SSID1, SSID2, SSID3\">");
    html += ssidsStr;
    html += F("</textarea>\n"
              "<div class=\"hint\">Comma-separated exact-match SSID names to exclude from logging</div>\n"
              "<label for=\"excluded_bssids\">Excluded BSSIDs</label>\n"
              "<textarea id=\"excluded_bssids\" name=\"excluded_bssids\" "
              "placeholder=\"aa:bb:cc:dd:ee:ff, 11:22:33:44:55:66\">");
    html += bssidsStr;
    html += F("</textarea>\n"
              "<div class=\"hint\">Comma-separated MAC addresses to exclude from logging</div>\n"
              "<label for=\"geofence_boxes\">Geofence Boxes</label>\n"
              "<textarea id=\"geofence_boxes\" name=\"geofence_boxes\" "
              "placeholder=\"top_lat,left_lon,bottom_lat,right_lon; ...\">");
    html += geoStr;
    html += F("</textarea>\n"
              "<div class=\"hint\">Semicolon-separated boxes: top_lat,left_lon,bottom_lat,right_lon. "
              "Logging suppressed inside these areas.</div>\n</div>\n");

    // --- Hardware section ---
    const bool buzzerOptionsEnabled = cfg.hardware.buzzer_enabled;
    html += F("<div class=\"section\">\n<h2>Hardware</h2>\n<div class=\"cb\">\n"
              "<input type=\"checkbox\" id=\"buzzer_enabled\" name=\"buzzer_enabled\"");
    if (cfg.hardware.buzzer_enabled)
        html += F(" checked");
    html += F(">\n<label for=\"buzzer_enabled\">Buzzer Enabled (Master Switch)</label>\n</div>\n"
              "<div class=\"hint\">When disabled, the sound options below are overridden and unavailable.</div>\n");
    html += buzzerOptionsEnabled
                ? F("<div id=\"buzzer_options_group\">\n")
                : F("<div id=\"buzzer_options_group\" style=\"opacity:0.6;\">\n");
    html += F("<div class=\"cb\">\n"
              "<input type=\"checkbox\" id=\"keyboard_sounds\" name=\"keyboard_sounds\"");
    if (cfg.hardware.keyboard_sounds)
        html += F(" checked");
    if (!buzzerOptionsEnabled)
        html += F(" disabled");
    html += F(">\n<label for=\"keyboard_sounds\">Keyboard Sounds</label>\n</div>\n"
              "<div class=\"hint\">Sound feedback for key presses (view cycle, start/stop, help, etc.)</div>\n"

              "<div class=\"cb\">\n"
              "<input type=\"checkbox\" id=\"beep_on_new_ssid\" name=\"beep_on_new_ssid\"");
    if (cfg.hardware.beep_on_new_ssid)
        html += F(" checked");
    if (!buzzerOptionsEnabled)
        html += F(" disabled");
    html += F(">\n<label for=\"beep_on_new_ssid\">Beep on New SSID</label>\n</div>\n"
              "<div class=\"hint\">Beep when a new unique access point is discovered</div>\n"

              "<div class=\"cb\">\n"
              "<input type=\"checkbox\" id=\"beep_on_blocked_ssid\" name=\"beep_on_blocked_ssid\"");
    if (cfg.hardware.beep_on_blocked_ssid)
        html += F(" checked");
    if (!buzzerOptionsEnabled)
        html += F(" disabled");
    html += F(">\n<label for=\"beep_on_blocked_ssid\">Beep on Blocked SSID</label>\n</div>\n"
              "<div class=\"hint\">Beep when a filtered/excluded access point is scanned</div>\n"

              "<div class=\"cb\">\n"
              "<input type=\"checkbox\" id=\"beep_on_geofence\" name=\"beep_on_geofence\"");
    if (cfg.hardware.beep_on_geofence)
        html += F(" checked");
    if (!buzzerOptionsEnabled)
        html += F(" disabled");
    html += F(">\n<label for=\"beep_on_geofence\">Sound in Geofenced Area</label>\n</div>\n");
    html += F("</div>\n");
    html += F("<div class=\"hint\">Periodic beep every 10 seconds while GPS is inside a geofence zone</div>\n"

              "<div class=\"row\">\n<div>\n"
              "<label for=\"buzzer_freq\">Buzzer Frequency (Hz)</label>\n"
              "<input type=\"number\" id=\"buzzer_freq\" name=\"buzzer_freq\" value=\"");
    html += String(cfg.hardware.buzzer_freq);
    html += F("\" min=\"100\" max=\"8000\">\n</div>\n<div>\n"
              "<label for=\"buzzer_duration\">Buzzer Duration (ms)</label>\n"
              "<input type=\"number\" id=\"buzzer_duration\" name=\"buzzer_duration\" value=\"");
    html += String(cfg.hardware.buzzer_duration_ms);
    html += F("\" min=\"10\" max=\"500\">\n</div>\n</div>\n"
              "<label for=\"screen_brightness\">Screen Brightness</label>\n"
              "<input type=\"range\" id=\"screen_brightness\" name=\"screen_brightness\" "
              "min=\"10\" max=\"255\" value=\"");
    html += String(cfg.hardware.screen_brightness);
    html += F("\" oninput=\"document.getElementById('bval').textContent=this.value\">\n"
              "<div class=\"range-val\" id=\"bval\">");
    html += String(cfg.hardware.screen_brightness);
    html += F("</div>\n"
              "<label for=\"led_brightness\">LED Brightness</label>\n"
              "<input type=\"range\" id=\"led_brightness\" name=\"led_brightness\" "
              "min=\"0\" max=\"255\" value=\"");
    html += String(cfg.hardware.led_brightness);
    html += F("\" oninput=\"document.getElementById('lval').textContent=this.value\">\n"
              "<div class=\"range-val\" id=\"lval\">");
    html += String(cfg.hardware.led_brightness);
    html += F("</div>\n"
              "<label for=\"low_battery_threshold\">Low Battery Shutdown Threshold (%)</label>\n"
              "<input type=\"number\" id=\"low_battery_threshold\" name=\"low_battery_threshold\" "
              "min=\"0\" max=\"100\" value=\"");
    html += String(cfg.hardware.low_battery_threshold);
    html += F("\">\n"
              "<div class=\"hint\">Battery % at which auto-shutdown triggers. Set to 0 to disable.</div>\n"
              "</div>\n");

    // --- Debug section ---
    html += F("<div class=\"section\">\n<h2>Debug</h2>\n");
    if (cfg.debug.enabled)
    {
        html += F("<div style=\"margin-bottom:14px\">\n"
                  "<a href=\"/debuglog\" style=\"display:inline-block;padding:10px 20px;"
                  "background:#4fc3f7;color:#0f0f23;border-radius:6px;font-weight:bold;"
                  "text-decoration:none;font-size:0.95em\">View Debug Log &rarr;</a>\n"
                  "</div>\n");
    }
    html += F("<div class=\"cb\">\n"
              "<input type=\"checkbox\" id=\"debug_enabled\" name=\"debug_enabled\"");
    if (cfg.debug.enabled)
        html += F(" checked");
    html += F(">\n<label for=\"debug_enabled\">Enable Debug Logging to SD</label>\n"
              "</div>\n"
              "<div id=\"stats_fields\">\n"
              "<div class=\"cb\">\n"
              "<input type=\"checkbox\" id=\"super_debug_enabled\" name=\"super_debug_enabled\"");
    if (cfg.debug.super_debug_enabled)
        html += F(" checked");
    html += F(">\n<label for=\"super_debug_enabled\">Enable Super Debug Logging</label>\n"
              "</div>\n"
              "<div class=\"hint\">Verbose scan-level instrumentation (channels, timing, MAC changes). Requires debug enabled</div>\n"
              "<div class=\"cb\">\n"
              "<input type=\"checkbox\" id=\"sys_stats_en\" name=\"sys_stats_en\"");
    if (cfg.debug.system_stats_enabled)
        html += F(" checked");
    html += F(">\n<label for=\"sys_stats_en\">Enable System Stats Logging</label>\n"
              "</div>\n"
              "<div class=\"hint\">Periodic memory, station, and battery stats (requires debug enabled)</div>\n"
              "<label for=\"sys_stats_interval\">System Stats Interval (seconds)</label>\n"
              "<input type=\"number\" id=\"sys_stats_interval\" name=\"sys_stats_interval\" value=\"");
    html += String(cfg.debug.system_stats_interval_s);
    html += F("\" min=\"30\" max=\"3600\" step=\"1\">\n"
              "<div class=\"hint\">How often system stats are logged (30–3600 seconds)</div>\n"
              "</div>\n"
              "<script>\n"
              "(function(){\n"
              "var d=document.getElementById('debug_enabled'),"
              "s=document.getElementById('stats_fields');\n"
              "function t(){s.style.display=d.checked?'block':'none';}\n"
              "d.addEventListener('change',t);t();\n"
              "})();\n"
              "</script>\n"
              "</div>\n\n");

    // --- Upload section ---
    html += F("<div class=\"section\">\n<h2>Upload</h2>\n"
              "<div class=\"cb\">\n"
              "<input type=\"checkbox\" id=\"wigle_upload_enabled\" name=\"wigle_upload_enabled\"");
    if (cfg.upload.wigle_upload_enabled)
        html += F(" checked");
    html += F(">\n<label for=\"wigle_upload_enabled\">Enable WiGLE upload (manual + auto)</label>\n</div>\n"
              "<div class=\"hint\">Master switch. When off, the manual upload option on the shutdown screen is also disabled.</div>\n"
              "<div class=\"cb\">\n"
              "<input type=\"checkbox\" id=\"auto_upload\" name=\"auto_upload\"");
    if (cfg.upload.auto_upload)
        html += F(" checked");
    html += F(">\n<label for=\"auto_upload\">Auto-upload WiGLE CSVs at boot</label>\n</div>\n"
              "<div class=\"hint\">Runs before GPS and WiFi scanning, only when the configured SSID is visible. Requires WiGLE upload to be enabled.</div>\n"
              "<label for=\"upload_ssid\">Home WiFi SSID</label>\n"              "<input type=\"text\" id=\"upload_ssid\" name=\"upload_ssid\" maxlength=\"32\" value=\"");
    html += htmlEscape(cfg.upload.upload_ssid);
    html += F("\">\n"
              "<label for=\"upload_password\">Home WiFi Password</label>\n"
              "<input type=\"hidden\" id=\"upload_password_clear\" name=\"upload_password_clear\" value=\"0\">\n"
              "<div class=\"secret-row\">\n"
              "<input type=\"password\" id=\"upload_password\" name=\"upload_password\" maxlength=\"63\" "
              "value=\"\" placeholder=\"Leave blank to keep saved password\">\n"
              "<button type=\"button\" class=\"secret-clear\" id=\"upload_password_clear_button\" "
              "data-field=\"upload_password\" data-clear=\"upload_password_clear\" "
              "data-note=\"upload_password_note\">Clear</button>\n"
              "</div>\n"
              "<div class=\"secret-note\" id=\"upload_password_note\">Leave blank to keep the saved password.</div>\n"
              "<label for=\"wigle_api_name\">WiGLE API Name</label>\n"
              "<input type=\"text\" id=\"wigle_api_name\" name=\"wigle_api_name\" maxlength=\"64\" value=\"");
    html += htmlEscape(cfg.upload.wigle_api_name);
    html += F("\">\n"
              "<label for=\"wigle_api_token\">WiGLE API Token</label>\n"
              "<input type=\"hidden\" id=\"wigle_api_token_clear\" name=\"wigle_api_token_clear\" value=\"0\">\n"
              "<div class=\"secret-row\">\n"
              "<input type=\"password\" id=\"wigle_api_token\" name=\"wigle_api_token\" maxlength=\"128\" "
              "value=\"\" placeholder=\"Leave blank to keep saved token\">\n"
              "<button type=\"button\" class=\"secret-clear\" id=\"wigle_api_token_clear_button\" "
              "data-field=\"wigle_api_token\" data-clear=\"wigle_api_token_clear\" "
              "data-note=\"wigle_api_token_note\">Clear</button>\n"
              "</div>\n"
              "<div class=\"secret-note\" id=\"wigle_api_token_note\">Leave blank to keep the saved token.</div>\n"
              "<div class=\"cb\">\n"
              "<input type=\"checkbox\" id=\"compress_before_upload\" name=\"compress_before_upload\"");
    if (cfg.upload.compress_before_upload)
        html += F(" checked");
    html += F(">\n<label for=\"compress_before_upload\">Gzip before upload</label>\n</div>\n"
              "<div class=\"cb\">\n"
              "<input type=\"checkbox\" id=\"delete_after_upload\" name=\"delete_after_upload\"");
    if (cfg.upload.delete_after_upload)
        html += F(" checked");
    html += F(">\n<label for=\"delete_after_upload\">Delete after successful upload</label>\n</div>\n"
              "<div class=\"hint\">When disabled, uploaded artifacts are moved to /wardriver/uploaded/.</div>\n"
              "<label for=\"min_sweeps_threshold\">Minimum sweeps to upload</label>\n"
              "<input type=\"number\" id=\"min_sweeps_threshold\" name=\"min_sweeps_threshold\" min=\"0\" max=\"65535\" step=\"1\" value=\"");
    html += String(cfg.upload.min_sweeps_threshold);
    html += F("\">\n"
              "<div class=\"hint\">Files with fewer distinct sweep timestamps are quarantined to /wardriver/uploaded/thin instead of being uploaded. Set 0 to disable.</div>\n"
              "<div class=\"cb\">\n"
              "<input type=\"checkbox\" id=\"retry_thin_files\" name=\"retry_thin_files\"");
    if (cfg.upload.retry_thin_files)
        html += F(" checked");
    html += F(">\n<label for=\"retry_thin_files\">Retry quarantined thin files on next upload</label>\n</div>\n"
              "<div class=\"hint\" id=\"thinDisabledHint\" style=\"display:none;color:#f0a\">"
              "Threshold is disabled (0) \u2014 retry-thin has no effect.</div>\n"
              "<script>(function(){var t=document.getElementById('min_sweeps_threshold');"
              "var r=document.getElementById('retry_thin_files');"
              "var h=document.getElementById('thinDisabledHint');"
              "function upd(){var d=parseInt(t.value||'0',10)===0;r.disabled=d;h.style.display=d?'block':'none';}"
              "t.addEventListener('input',upd);upd();})();</script>\n"
              "<script>(function(){function bind(btnId){var b=document.getElementById(btnId);if(!b)return;"
              "var f=document.getElementById(b.getAttribute('data-field'));"
              "var c=document.getElementById(b.getAttribute('data-clear'));"
              "var n=document.getElementById(b.getAttribute('data-note'));"
              "if(!f||!c)return;b.addEventListener('click',function(){c.value='1';f.value='';"
              "if(n)n.textContent='Will be cleared on save.';});"
              "f.addEventListener('input',function(){if(f.value.length>0){c.value='0';"
              "if(n)n.textContent='New value will replace the saved secret.';}});}"
              "bind('upload_password_clear_button');bind('wigle_api_token_clear_button');})();</script>\n"
              "<script>(function(){var m=document.getElementById('wigle_upload_enabled');"
              "var ids=['auto_upload','upload_ssid','upload_password','upload_password_clear_button',"
              "'wigle_api_name','wigle_api_token','wigle_api_token_clear_button',"
              "'compress_before_upload','delete_after_upload','min_sweeps_threshold','retry_thin_files'];"
              "function upd(){var off=!m.checked;ids.forEach(function(id){var e=document.getElementById(id);"
              "if(e)e.disabled=off;});}m.addEventListener('change',upd);upd();})();</script>\n"
              "</div>\n\n");

    // --- Web Authentication section ---
    html += F("<div class=\"section\">\n<h2>Web Authentication</h2>\n"
              "<label for=\"admin_user\">Username</label>\n"
              "<input type=\"text\" id=\"admin_user\" name=\"admin_user\" value=\"");
    html += htmlEscape(cfg.web.admin_user);
    html += F("\">\n"
              "<label for=\"admin_pass\">Password</label>\n"
              "<input type=\"text\" id=\"admin_pass\" name=\"admin_pass\" placeholder=\"(leave blank to keep current)\">\n"
              "<div class=\"hint\">Leave password blank to keep the current password unchanged</div>\n"
              "</div>\n\n"
              "<button type=\"submit\">Save &amp; Reboot</button>\n"
              "</form>\n</body>\n</html>");

    // Send as a single response with known content-length (avoids
    // chunked transfer encoding which is unreliable over ESP32 soft AP)
    server.send(200, "text/html", html);
}

// ─── Debug Log Viewer ─────────────────────────────────────────────────────────────
void WebPortal::handleDebugLog()
{
    if (!authenticate())
        return;
    resetIdleTimer();

    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");

    server.sendContent(F(
        "<!DOCTYPE html><html><head>"
        "<meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>Debug Log - Cardputer Wardriver</title>"
        "<style>"
        "body{font-family:sans-serif;background:#1a1a2e;color:#eee;padding:16px;margin:0;}"
        "h1{color:#4fc3f7;margin-top:0;}"
        "pre{background:#0d0d1a;padding:12px;border-radius:6px;overflow-x:auto;"
        "white-space:pre-wrap;word-break:break-all;font-size:11px;"
        "max-height:65vh;overflow-y:auto;border:1px solid #333;}"
        ".bar{display:flex;gap:12px;align-items:center;margin-bottom:14px;flex-wrap:wrap;}"
        "a{color:#4fc3f7;text-decoration:none;}"
        "a:hover{text-decoration:underline;}"
        "button{background:#c62828;color:#fff;border:none;padding:8px 18px;"
        "border-radius:4px;cursor:pointer;font-size:14px;}"
        "button:hover{background:#e53935;}"
        "</style></head><body>"
        "<h1>Debug Log</h1>"
        "<div class=\"bar\">"
        "<a href=\"/\">&larr; Back to Config</a>"
        "<form action=\"/cleardebuglog\" method=\"POST\" style=\"margin:0\">"
        "<button type=\"submit\" "
        "onclick=\"return confirm('Clear the entire debug log?')\">Clear Log</button>"
        "</form>"
        "</div><pre>"));

    File f = SD.open(logger.getDebugLogFile());
    if (f)
    {
        const size_t BUF = 512;
        uint8_t buf[BUF];
        while (f.available())
        {
            size_t n = f.read(buf, BUF);
            if (n > 0)
                server.sendContent(reinterpret_cast<const char *>(buf), n);
        }
        f.close();
    }
    else
    {
        server.sendContent("No log file found.");
    }

    server.sendContent(F("</pre></body></html>"));
    server.sendContent("");
}

void WebPortal::handleClearDebugLog()
{
    if (!authenticate())
        return;
    resetIdleTimer();
    logger.clearDebugLog();
    server.sendHeader("Location", "/debuglog", true);
    server.send(302, "text/plain", "");
}
