#ifndef WEB_PORTAL_H
#define WEB_PORTAL_H

#include <Arduino.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "ConfigManager.h"
#include "WiFiManager.h"

class WebPortal
{
public:
    WebPortal(ConfigManager *configMgr);
    void begin(bool apMode = true);
    void handle();
    void stop();
    bool isActive() { return active; }
    void resetIdleTimer();
    bool hasTimedOut();

private:
    ConfigManager *configManager;
    WiFiManager wifiMgr;
    WebServer server;
    DNSServer dnsServer;
    bool active;
    bool apMode;
    unsigned long lastActivity;

    void handleRoot();
    void handleSave();
    void handleStatus();
    void handleDebugLog();
    void handleClearDebugLog();
    void handleNotFound();
    bool authenticate();
    void sendHTMLChunked();
    String getIPAddress();
};

#endif // WEB_PORTAL_H
