![Cardputer Wardriver logo](assets/thumbnailsmall.png)

# Wardriver // stewmoss

Turn your [M5Stack Cardputer](https://shop.m5stack.com/products/m5stack-cardputer-kit-w-m5stamps3) into a portable WiFi mapping tool. This firmware scans for 2.4 GHz networks, tags each one with GPS coordinates, and saves [WiGLE-compatible](https://wigle.net) CSV files to an SD card — ready to upload and contribute to the global wireless database.

## Highlights

- **WiGLE auto-upload** — on-device HTTPS upload to WiGLE when your home network is detected at boot; gzip compression, thin-file quarantine, and archive or delete management built-in. Manual upload is also available from the shutdown screen. Output is strict **WiGLE CSV v1.6** format — no conversion needed - See docs: [WiGLE Upload](docs/wigle-upload.md)
- **Active & passive scanning** — per-channel hop or all-channel sweep with configurable dwell times; catch networks that basic scanners miss - See docs: [Scanning Modes](docs/scan-modes.md)
- **Six live dashboards** — summary, security snapshot, live AP feed, new AP discovery, channel activity map, and system info — all flicker-free
- **Channel congestion map** — colour-coded bar chart with three views (session, sweep, unique) so you can spot crowded channels at a glance
- **Three GPS logging modes** — strict fix-only, zero-GPS (no satellite required), or last-known (keep logging when the sky disappears)
- **GPS-sourced clock sync** — the ESP32 sets its system clock directly from GPS, so every timestamp is accurate with zero network dependency
- **Privacy controls** — SSID/BSSID exclusion lists, up to 10 GPS geofence zones, MAC randomisation with OUI spoofing — your home network and daily routes stay off the map
- **Built-in WiFi web config portal** — configure everything from your phone's browser; captive portal auto-redirects on Android, iOS, and Windows; HTTP Basic Auth with configurable credentials
- **Pause logging** — press `P` to keep scanning while temporarily halting CSV writes, perfect for indoor stops without losing session context
- **FLAG marker** — press the G0 button to drop a GPS-tagged marker in your CSV for points of interest
- **Configurable sound alerts** — beep on new AP, blocked SSID, or geofence entry — each independently toggleable
- **Smart power management** — run unattended for hours; display blanking, scan stop/start, and a configurable low-battery threshold trigger a graceful auto-shutdown with a full session summary
- **Structured diagnostics** — optional `/wardriver/logs/stats.csv` telemetry for heap, battery, scan counters, CPU, temperature, RTOS, and loop timing — dig in when you need it
- **WiFi TX power & country code** — tune radio output and channel availability for your region
- **On-device help** — press `H` anytime for a keyboard shortcut reference
- **Cardputer ADV support** — auto-detects and selects the correct GPS pinouts for both Cardputer v1.1 and ADV models; just plug in and go

## Quick Start

1. **Flash** the firmware via USB — `pio run --target upload`
2. **Wire a GPS module** to the active hardware profile's UART pins. On Cardputer v1.1, GPS TX goes to Cardputer RX GPIO 1 and GPS RX goes to Cardputer TX GPIO 2. On Cardputer ADV, GPS TX goes to Cardputer RX GPIO 15 and GPS RX goes to Cardputer TX GPIO 13.
3. **Insert** a FAT32 micro SD card
4. **Power on** — first boot enters Config Mode automatically
5. **Connect** your phone/laptop to the `M5-Wardriver` WiFi network
6. **Configure** at `http://192.168.4.1`, then save and reboot. (default username: `admin`, default password: `password`) **Change the default password on first setup** — see [Configuration](docs/configuration.md).
7. **Go outside** — scanning starts automatically after GPS lock
8. **WiGLE upload (optional)** — configure upload credentials in the web portal (Upload section). Enable **Auto-upload at boot** and the device will look for your home network on every power-on and upload any pending CSV files automatically, with gzip compression. Or skip auto-upload and press `U` on the shutdown screen for a manual on-demand upload. See [WiGLE Upload](docs/wigle-upload.md) for setup details.
9. **Safe Shutdown** — press `Q` to flush logs and unmount the SD card before powering off.

> **New here?** See the [Getting Started](docs/getting-started.md) guide for a full walkthrough with wiring diagrams and screenshots.

## What You Need

| Item | Notes |
|------|-------|
| M5Stack Cardputer v1.1 or Cardputer ADV | ESP32-S3 based |
| GPS module (UART) | e.g., BN-220, NEO-6M, M5Stack GPS Unit |
| Micro SD card | FAT32, Class 10 recommended |
| USB-C cable | For flashing and serial debug |



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
| [Scan Modes](docs/scan-modes.md) | Active vs passive scanning, dwell times, environment tuning |
| [Web Interface](docs/web-interface.md) | Config portal layout, fields, and workflow |
| [Operation](docs/operation.md) | Dashboard views, keyboard controls, LED/buzzer behaviour |
| [LED Indicator](docs/led-indicator.md) | Status LED colour reference |
| [WiGLE Upload](docs/wigle-upload.md) | Auto-upload and manual upload to WiGLE — setup, file management, and thin-file quarantine |
| [Troubleshooting](docs/troubleshooting.md) | Common problems and fixes |

## License

Released under the MIT License. See [LICENSE](LICENSE).
