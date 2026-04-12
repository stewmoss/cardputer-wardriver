#include "WiFiManager.h"
#include "Logger.h"

WiFiManager::WiFiManager()
    : apActive(false)
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
    logger.debugPrintln(String("[WiFiManager] AP started: ") + ssid +
                        " IP: " + WiFi.softAPIP().toString());
}

void WiFiManager::stopAP()
{
    if (!apActive)
        return;

    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    apActive = false;

    logger.debugPrintln("[WiFiManager] AP stopped");
}

void WiFiManager::startSTA()
{
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    logger.debugPrintln("[WiFiManager] STA mode enabled");
}

IPAddress WiFiManager::getAPIP()
{
    return WiFi.softAPIP();
}

bool WiFiManager::isAPActive()
{
    return apActive;
}
