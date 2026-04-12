#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>

class WiFiManager
{
public:
    WiFiManager();
    void startAP(const String &ssid);
    void stopAP();
    void startSTA();
    IPAddress getAPIP();
    bool isAPActive();

private:
    bool apActive;
    String apSSID;
};

#endif // WIFI_MANAGER_H
