# M5 Cardputer Wardriver

Turn your [M5Stack Cardputer](https://shop.m5stack.com/products/m5stack-cardputer-kit-w-m5stamps3) into a portable WiFi mapping tool. This firmware scans for 2.4 GHz networks, tags each one with GPS coordinates, and saves [WiGLE-compatible](https://wigle.net) CSV files to an SD card — ready to upload and contribute to the global wireless database.

## Quick Start

1. **Flash** the firmware via USB — `pio run --target upload`
2. **Wire a GPS module** to GPIO 1 (TX) and GPIO 2 (RX)
3. **Insert** a FAT32 micro SD card
4. **Power on** — first boot enters Config Mode automatically
5. **Connect** your phone/laptop to the `M5-Wardriver` WiFi network
6. **Configure** at `http://192.168.4.1`, then save and reboot
7. **Go outside** — scanning starts automatically after GPS lock

> **New here?** See the [Getting Started](docs/getting-started.md) guide for a full walkthrough with wiring diagrams and screenshots.

## What You Need

| Item | Notes |
|------|-------|
| M5Stack Cardputer v1.1 | ESP32-S3 based |
| GPS module (UART) | e.g., BN-220, NEO-6M, M5Stack GPS Unit |
| Micro SD card | FAT32, Class 10 recommended |
| USB-C cable | For flashing and serial debug |

## Highlights

- **WiGLE CSV v1.6** output — upload directly to wigle.net, no conversion needed
- **Active & passive scanning** — per-channel hop or all-channel sweep with configurable dwell times
- **Three GPS logging modes** — strict fix-only, zero-GPS (log without coordinates), or last-known (reuse stale position)
- **Five live dashboards** — summary, security snapshot, live AP feed, new AP discovery, system info — all flicker-free
- **Web config portal** — configure everything from your phone's browser over the built-in WiFi hotspot
- **Privacy controls** — SSID/BSSID exclusion lists, up to 10 GPS geofence zones, MAC randomisation with OUI spoofing
- **Configurable sound alerts** — beep on new AP, blocked SSID, or geofence entry — each independently toggleable
- **Smart power management** — display blanking, scan stop/start, low-battery auto-shutdown with session summary
- **FLAG bookmarks** — press the G0 button to drop a GPS-tagged marker in your CSV for points of interest
- **WiFi TX power & country code** — tune radio output and channel availability for your region
- **On-device help** — press `H` anytime for a keyboard shortcut reference

## Build

Requires [PlatformIO](https://platformio.org/).

```bash
pio run                    # Build
pio run --target upload    # Build and flash
pio device monitor         # Serial monitor (115200 baud)
```

## Documentation

| Guide | What's inside |
|-------|---------------|
| [Overview & Architecture](docs/index.md) | How the firmware works, feature list, system design |
| [Getting Started](docs/getting-started.md) | Hardware setup, flashing, first boot walkthrough |
| [Configuration](docs/configuration.md) | Every setting explained with defaults and examples |
| [Web Interface](docs/web-interface.md) | Config portal layout, fields, and workflow |
| [Operation](docs/operation.md) | Dashboard views, keyboard controls, LED/buzzer behaviour |
| [LED Indicator](docs/led-indicator.md) | Status LED colour reference |
| [Troubleshooting](docs/troubleshooting.md) | Common problems and fixes |

## License

See LICENSE file for details.
