#include "WiFiManager.h"
#include "Logger.h"
#include "esp_wifi.h"

namespace
{
    void applyCountryCode(const String &countryCode)
    {
        if (countryCode.length() != 2)
        {
            return;
        }

        String upper = countryCode;
        upper.toUpperCase();
        esp_err_t err = esp_wifi_set_country_code(upper.c_str(), true);
        if (err == ESP_OK)
        {
            logger.debugPrintln(String("[WiFiManager] Country code set: ") + upper);
        }
        else
        {
            logger.debugPrintln(String("[WiFiManager] Country code failed: ") + upper + " err=" + String((int)err));
        }
    }
}

WiFiManager::WiFiManager(bool superDebug)
    : apActive(false), staActive(false), superDebug(superDebug)
{
}

void WiFiManager::startAP(const String &ssid)
{
    apSSID = ssid;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid.c_str());

    // Small delay for AP to settle
    delay(100);

    apActive = true;
    if (superDebug)
    {
        logger.debugPrintln(String("[WiFiManager] AP started: ") + ssid +
                            " IP: " + WiFi.softAPIP().toString());
    }
    else
    {
        logger.debugPrintln(String("[WiFiManager] AP started: ") + ssid);
    }
}

void WiFiManager::stopAP()
{
    if (!apActive)
        return;

    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    apActive = false;
    if (superDebug)
    {
        logger.debugPrintln("[WiFiManager] AP stopped");
    }
}

void WiFiManager::startSTA()
{
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    staActive = true;
    if (superDebug)
    {
        logger.debugPrintln("[WiFiManager] STA mode enabled");
    }
}

bool WiFiManager::connectSTA(const String &ssid,
                             const String &password,
                             uint32_t timeoutMs,
                             const String &countryCode)
{
    if (ssid.length() == 0)
    {
        logger.debugPrintln("[WiFiManager] Cannot connect STA: empty SSID");
        return false;
    }

    startSTA();
    applyCountryCode(countryCode);

    logger.debugPrintln(String("[WiFiManager] Connecting STA to SSID: ") + ssid);
    if (password.length() > 0)
    {
        WiFi.begin(ssid.c_str(), password.c_str());
    }
    else
    {
        WiFi.begin(ssid.c_str());
    }

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs)
    {
        delay(250);
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        staActive = true;
        logger.debugPrintln(String("[WiFiManager] STA connected IP: ") + WiFi.localIP().toString() +
                            " RSSI: " + String(WiFi.RSSI()));
        return true;
    }

    logger.debugPrintln("[WiFiManager] STA connect timed out");
    disconnectSTA();
    return false;
}

void WiFiManager::disconnectSTA()
{
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    staActive = false;
    if (superDebug)
    {
        logger.debugPrintln("[WiFiManager] STA disconnected");
    }
}

bool WiFiManager::isSTAConnected()
{
    return staActive && WiFi.status() == WL_CONNECTED;
}

bool WiFiManager::isSsidVisible(const String &targetSsid, uint32_t timeoutMs)
{
    if (targetSsid.length() == 0)
    {
        return false;
    }

    startSTA();
    uint32_t start = millis();
    bool found = false;

    while (!found && (millis() - start) < timeoutMs)
    {
        WiFi.scanDelete();
        int count = WiFi.scanNetworks(false, false, false, 100);
        if (count > 0)
        {
            for (int i = 0; i < count; ++i)
            {
                if (WiFi.SSID(i) == targetSsid)
                {
                    found = true;
                    break;
                }
            }
        }
        WiFi.scanDelete();

        if (!found)
        {
            delay(100);
        }
    }

    if (superDebug)
    {
        logger.debugPrintln(String("[WiFiManager] SSID visible '") + targetSsid + "': " + (found ? "yes" : "no"));
    }
    return found;
}

IPAddress WiFiManager::getAPIP()
{
    return WiFi.softAPIP();
}

bool WiFiManager::isAPActive()
{
    return apActive;
}
