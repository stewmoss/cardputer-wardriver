#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>

class WiFiManager
{
public:
    WiFiManager(bool superDebug = false);
    void startAP(const String &ssid);
    void stopAP();
    void startSTA();
    bool connectSTA(const String &ssid,
                    const String &password,
                    uint32_t timeoutMs,
                    const String &countryCode);
    void disconnectSTA();
    bool isSTAConnected();
    bool isSsidVisible(const String &targetSsid, uint32_t timeoutMs);
    IPAddress getAPIP();
    bool isAPActive();

private:
    bool apActive;
    bool staActive;
    String apSSID;
    bool superDebug;
};

#endif // WIFI_MANAGER_H
