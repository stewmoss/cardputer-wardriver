# Getting Started

This guide walks you through everything from wiring to your first wardriving session. No prior experience with the M5 Cardputer is needed.

---

## What You'll Need

| Item | Details |
|------|---------|
| **M5Stack Cardputer** | Cardputer v1.1 or Cardputer ADV |
| **GPS module** | Any UART GPS at 115200 baud (e.g., BN-220, NEO-6M, M5Stack GPS Unit) |
| **Micro SD card** | FAT32 formatted, 2 GB – 32 GB |
| **USB-C cable** | For flashing and serial debug |
| **PlatformIO** | [VS Code extension](https://platformio.org/install/ide?install=vscode) or CLI |

---

## 1. Clone the Repository

```bash
git clone https://github.com/stewmoss/cardputer-wardriver.git
cd cardputer-wardriver
```

---

## 2. Install Dependencies

PlatformIO handles this automatically. Just open the project folder in VS Code with the PlatformIO extension installed — it will detect `platformio.ini` and download everything.

If you prefer the command line:

```bash
pio pkg install
```

---

## 3. Wire the GPS Module

Connect your GPS module to the Cardputer via the Grove port or direct wiring:

| GPS Pin | Cardputer Pin | Default GPIO |
|---------|---------------|-------------|
| TX (data out) | RX | GPIO 1 |
| RX (data in)  | TX | GPIO 2 |
| VCC | 3.3V or 5V | — |
| GND | GND | — |

Default Cardputer-side GPS pins depend on the active hardware profile:

| Hardware profile | Cardputer TX pin | Cardputer RX pin |
|------------------|------------------|------------------|
| Cardputer (v1.1) | GPIO 2 | GPIO 1 |
| Cardputer ADV | GPIO 13 | GPIO 15 |

> **Tip:** The TX/RX pins are configurable. If your GPS uses different pins, change them later in the web portal or config file. The Device model selector can also reseed the GPS pins to the selected hardware profile defaults.

---

## 4. Prepare the SD Card

1. Format a micro SD card as **FAT32**
2. Insert it into the Cardputer's card slot

That's it. The firmware creates its `/wardriver/` directory automatically on first boot.

**Optional — pre-load a config:** If you want to skip the web portal setup, copy `config.txt.example` from the project root to the SD card as `/wardriver/wardriverconfig.txt` and edit the values to match your setup.

---

## 5. Installation & Flashing

If you already use [M5Launcher](https://bmorcelli.github.io/Launcher/) to manage your Cardputer, you can install it with OTA

Or you can burn it directly from the [m5burner tool](https://docs.m5stack.com/en/uiflow/m5burner/intro), just search for 'Wardriver' on the 'cardputer' device category and click on burn.

---

## 6. First Boot

1. Power on the Cardputer with the SD card inserted
2. The splash screen shows **Wardriver**, the firmware version, and the detected hardware model
3. Since no config file exists yet, the device **automatically enters Config Mode**
4. The screen displays the WiFi AP name and IP address:
   - **SSID:** `M5-Wardriver`
   - **IP:** `192.168.4.1`

On later boots with a valid config, the firmware skips Config Mode. If WiGLE upload and boot-time auto-upload are enabled, it runs the upload check before GPS acquisition; otherwise it starts satellite search immediately.

---

## 7. Configure via the Web Portal

1. On your phone or laptop, connect to the **M5-Wardriver** WiFi network (no password)
2. A captive portal should open automatically. If not, go to `http://192.168.4.1`
3. Log in with the default credentials: **admin** / **password**
4. Fill in your settings:
   - **Hardware Model** — leave `Auto-detect` unless you need to force `Cardputer (v1.1)` or `Cardputer ADV`
   - **GPS TX/RX pins** — match your wiring (v1.1 default: TX=2, RX=1; ADV default: TX=13, RX=15)
   - **GMT Offset** — your timezone offset from UTC (e.g., `10` for AEST, `-5` for EST)
   - **Scan mode** — choose between active/passive and hop/all-channel
   - **Accuracy threshold** — how tight the GPS fix must be before logging starts
   - Optionally add SSID/BSSID exclusions or geofence boxes
5. Click **Save & Reboot**

The device saves everything to the SD card and restarts with your new settings.

> **Warning:** Change the default web portal password (`password`) on first setup. See [Configuration — Web Authentication](configuration.md#web-authentication-settings) for details.

---

## 8. Start Wardriving

After rebooting with a valid config:

1. **Satellite search** — the screen shows satellite count and fix status. The LED glows **yellow**. WiFi scanning runs in the background. In the default `fix_only` mode, nothing is logged yet — but if you've configured `zero_gps` or `last_known` mode, logging behaviour changes once scanning starts (see [GPS Logging Modes](configuration.md#gps-logging-modes)).

2. **GPS lock** — once you have a 3D fix with good accuracy, the LED turns **green**, a CSV file is created (e.g., `wardriving_001.csv`), and logging begins automatically.

3. **Go for a walk or drive** — the device handles everything. Networks are scanned, filtered, and logged continuously.

### Quick Controls

| Key | What it does |
|-----|-------------|
| `ENTER` | Cycle through the dashboard views |
| `SPACE` | Toggle dashboard sub-mode (e.g., channel view mode) |
| `H` | Show/hide the keyboard help overlay |
| `B` | Toggle the display on/off (saves battery) |
| `C` | Enter Config Mode |
| `S` | Mute/unmute the buzzer |
| `P` | Pause/resume CSV logging (scanning continues) |
| `X` | Stop/start scanning entirely (low power mode) |
| `Q` | Safe shutdown — flush logs, unmount SD card |
| `U` | Trigger manual WiGLE upload from the shutdown screen when upload is configured and files are pending |
| `G0` button | Drop a FLAG marker at your current GPS position |

See the full [Operation Guide](operation.md) for dashboard details and advanced features.

---

## 9. Retrieve or Upload Your Data

### On-Device WiGLE Upload

1. Enable `wigle_upload_enabled` in the Upload section of the web portal.
2. Enter your home WiFi SSID, WiFi password, WiGLE API name, and WiGLE API token.
3. Optional: enable `auto_upload` to upload pending CSV files at boot when the configured SSID is visible.
4. For a manual upload, press `Q` for safe shutdown. If upload is configured and pending files exist, press `U` on the shutdown screen.

### Manual SD Upload

1. Press `Q` for a safe shutdown.
2. Remove the SD card and insert it into a computer.
3. Navigate to the `/wardriver/` folder.
4. Upload any non-empty `wardriving_*.csv` file to [wigle.net/uploads](https://wigle.net/uploads).

The CSV files are in WiGLE v1.6 format — no conversion needed.

---

## 9. Serial Debug

If something isn't working, connect via USB and open a serial monitor at **115200 baud** to see real-time status messages:

```bash
pio device monitor
```

You'll see firmware version, config status, GPS updates, scan results, and any error messages.

---

## 10. Build and Flash

### VS Code (recommended)

1. Open the project folder in VS Code
2. Click the **checkmark** icon in the bottom toolbar to build
3. Connect the Cardputer via USB-C
4. Click the **arrow** icon to upload

### Command Line

```bash
pio run                # Build only
pio run -t upload      # Build and flash
pio device monitor     # Serial monitor at 115200 baud
```

After a successful build, the firmware binary is also saved to `output/wardriver-<version>.bin`.

---