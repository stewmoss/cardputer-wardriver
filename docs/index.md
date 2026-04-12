# M5 Cardputer Wardriver

The M5 Cardputer Wardriver turns a pocket-sized M5Stack Cardputer (ESP32-S3) into a dedicated WiFi mapping device. Pair it with a cheap GPS module, pop in an SD card, and go for a walk or drive — every 2.4 GHz network you pass is automatically logged with its location, signal strength, and encryption type.

The output is a standard **WiGLE CSV v1.6** file you can upload directly to [wigle.net](https://wigle.net) to contribute to the global wireless map.

---

## Key Features

### Scanning & Logging
- **Configurable scan modes** — active or passive, per-channel hop or all-channel sweep, with adjustable dwell times
- **Three GPS logging modes** — strict fix-only (default), zero-GPS (log without coordinates), or last-known (reuse last valid position when fix drops)
- **WiGLE CSV v1.6** — strict format compliance for direct upload
- **10 KB write buffer** — batches SD writes to improve reliability and card longevity
- **Auto-incrementing files** — each session creates a new `wardriving_NNN.csv`

### Privacy & Filtering
- **SSID/BSSID exclusion lists** — hide your home or office network from logs
- **GPS geofencing** — define up to 10 bounding boxes where logging is suppressed
- **MAC randomisation** — rotate the device's MAC address during active scans to avoid being tracked
- **Accuracy guard** — skip logging when GPS accuracy drops below a threshold

### User Interface
- **Five dashboard views** — cycle through summary, security snapshot, live feed, new AP discoveries, and system settings
- **Keyboard help overlay** — press `H` for an instant shortcut reference
- **Colour-coded status LED** — see device state at a glance without reading the screen
- **Buzzer alerts** — audio beep when a brand-new access point is discovered

### Power & Control
- **Display blanking** (`B`) — turn the screen off to save battery
- **Stop/start scanning** (`X`) — pause the WiFi radio entirely for low-power idle
- **Pause/resume logging** (`P`) — keep scanning but temporarily stop writing to the SD card
- **FLAG markers** (G0 button) — drop a GPS-tagged bookmark in the CSV
- **Safe shutdown** (`Q`) — flush logs and unmount the SD card before powering off
- **Low-battery auto-shutdown** — configurable threshold triggers a graceful shutdown

---

## How It Works

1. **Power on** — splash screen appears, SD card mounts
2. **Config check** — settings load from `/wardriver/wardriverconfig.txt`. If the file is missing or invalid, the device enters Config Mode automatically so you can set things up via your phone
3. **Satellite search** — GPS begins acquiring a fix. WiFi scanning runs in the background and you can see networks on screen, but nothing is saved yet
4. **GPS lock** — once the GPS reports a 3D fix with good accuracy, a CSV file opens and logging begins
5. **Active scanning** — every scan cycle, detected networks are filtered and logged. The LED flashes blue during sweeps and returns to green between them
6. **You're done** — press `Q` to safely shut down, remove the SD card, and upload your CSV files

> **Tip:** Hold the **G0 button** during boot to force Config Mode, even if your config is valid. Handy for changing settings on the go.

---

## Architecture

```
┌─────────────────────────────────────────────────────┐
│  main.cpp — State Machine                           │
│  INIT → SEARCHING_SATS → ACTIVE_SCAN → SHUTDOWN    │
│                ↕                                    │
│           CONFIG_MODE                               │
├─────────────────────────────────────────────────────┤
│  Modules                                            │
│  ┌────────────┐ ┌────────────┐ ┌────────────────┐  │
│  │ GPSManager │ │WiFiScanner │ │GeofenceFilter  │  │
│  │ (TinyGPS++)│ │(async scan)│ │(SSID/BSSID/geo)│  │
│  └────────────┘ └────────────┘ └────────────────┘  │
│  ┌────────────┐ ┌────────────┐ ┌────────────────┐  │
│  │ DataLogger │ │  Display   │ │ AlertManager   │  │
│  │ (ring buf) │ │(dashboards)│ │ (LED + buzzer) │  │
│  └────────────┘ └────────────┘ └────────────────┘  │
│  ┌────────────┐ ┌────────────┐ ┌────────────────┐  │
│  │ WebPortal  │ │WiFiManager │ │    Logger      │  │
│  │ (captive)  │ │  (AP/STA)  │ │ (Serial + SD)  │  │
│  └────────────┘ └────────────┘ └────────────────┘  │
├─────────────────────────────────────────────────────┤
│  Config Layer                                       │
│  Config.h (structs & defaults) + ConfigManager      │
└─────────────────────────────────────────────────────┘
```

---

## Documentation Guide

| Guide | What you'll find |
|-------|-----------------|
| **[Getting Started](getting-started.md)** | Hardware setup, wiring, flashing, and your first boot |
| **[Configuration](configuration.md)** | Every setting explained — type, default, what it does |
| **[Web Interface](web-interface.md)** | The browser-based config portal: layout, fields, workflow |
| **[Operation](operation.md)** | Dashboard views, keyboard controls, LED/buzzer meanings |
| **[LED Indicator](led-indicator.md)** | Quick colour reference for the status LED |
| **[Troubleshooting](troubleshooting.md)** | Common problems with step-by-step fixes |

---

## Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| M5Cardputer | ^1.0.0 | Display, keyboard, speaker hardware access |
| ArduinoJson | ^6.21.3 | JSON config parsing |
| FastLED | ^3.6.0 | SK6812 status LED |
| TinyGPSPlus | ^1.0.3 | GPS NMEA sentence parsing |

Plus built-in ESP32 Arduino libraries: WiFi, SD, SPI, FS, DNSServer, WebServer.
