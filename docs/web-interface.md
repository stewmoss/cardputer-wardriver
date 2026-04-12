# Web Interface

The wardriver has a built-in web portal for configuring all settings from your phone or laptop browser. The device creates its own WiFi hotspot — no internet connection or separate router needed.

---

## How to Open the Config Portal

| Method | When to use | How |
|--------|------------|-----|
| **First boot** | No config file on SD card | Happens automatically |
| **G0 button** | Want to reconfigure at boot | Hold the front G0 button while powering on |
| **`C` key** | Want to reconfigure while running | Press `C` on the keyboard |

### Step by Step

1. The device creates a WiFi network called **M5-Wardriver** (no password)
2. Connect to it from your phone or laptop
3. A login prompt appears — enter the credentials:
   - Default username: **admin**
   - Default password: **password**
4. The config page should open automatically (captive portal). If it doesn't, go to **http://192.168.4.1**

The Cardputer's screen shows the AP name and IP address while Config Mode is active.

> **Tip:** The portal times out after **5 minutes** of inactivity and reboots the device. If this happens, just re-enter Config Mode.

---

## Portal Sections

The config page is a single dark-themed page with all your settings. Here's what each section contains:

### GPS Settings

| Field | What to enter | Default |
|-------|--------------|---------|
| TX Pin | GPIO pin wired to your GPS module's TX | `2` |
| RX Pin | GPIO pin wired to your GPS module's RX | `1` |
| Accuracy Threshold (meters) | How accurate GPS must be before logging starts (lower = stricter) | `500` |
| GMT Offset (hours) | Your timezone offset from UTC (e.g., `10` for AEST, `-5` for EST) | `0` |
| GPS Logging Mode | What to do when GPS fix is lost — **Fix Only** (default), **Zero GPS**, or **Last Known** | Fix Only |

### Scanning

| Field | What to enter | Default |
|-------|--------------|---------|
| Scan Delay (ms) | Pause between WiFi scan cycles | `150` |
| Active Dwell (ms) | Time per channel during active scans | `150` |
| Passive Dwell (ms) | Time per channel during passive scans | `250` |
| Scan Mode | Choose from active/passive + hop/all combinations | `active_all` |
| WiFi Country Code | ISO 3166-1 country code for channel/power compliance | `"Default"` |
| WiFi TX Power | Transmit power in quarter-dBm units (8–84) | `84` |
| MAC Randomise Interval | How often to rotate the device MAC (`0` = never, `1` = every scan) | `1` |
| MAC Spoof OUI | Make randomised MACs look like real manufacturers | Off |

### Filters

| Field | How to format | Example |
|-------|--------------|---------|
| Excluded SSIDs | Comma-separated network names | `MyHomeWiFi, OfficeNetwork` |
| Excluded BSSIDs | Comma-separated MAC addresses | `aa:bb:cc:dd:ee:ff` |
| Geofence Boxes | Semicolon-separated boxes: `top_lat,left_lon,bottom_lat,right_lon` | `37.785,-122.42,37.78,-122.415` |

### Hardware

| Field | Range | Default |
|-------|-------|---------|
| Buzzer Enabled | Checkbox (master switch) | On |
| Keyboard Sounds | Checkbox | On |
| Beep on New SSID | Checkbox | Off |
| Beep on Blocked SSID | Checkbox | Off |
| Sound in Geofenced Area | Checkbox | Off |
| Buzzer Frequency | 100–8000 Hz | `2500` |
| Buzzer Duration | 10–500 ms | `50` |
| Screen Brightness | Slider: 10–255 | `128` |
| LED Brightness | Slider: 0–255 (0 = off) | `64` |
| Low Battery Threshold | 0–100% (0 = disabled) | `5` |

### Debug

| Field | Description |
|-------|-------------|
| Enable Debug Logging | Write detailed logs to the SD card |
| Enable Super Debug | Extra verbose scan-level logging (requires debug enabled) |
| Enable System Stats | Periodically log heap/battery stats |
| View Debug Log | Link to view the log file in your browser (appears when debug is enabled) |

### Web Authentication

| Field | Description |
|-------|-------------|
| Username | Login name for the portal |
| Password | Login password (leave blank to keep the current one) |

> **Warning:** Change the default password from `password` on first setup.

---

## Saving Settings

Click **Save & Reboot** at the bottom of the page. The device will:
1. Write all settings to `/wardriver/wardriverconfig.txt` on the SD card
2. Show a confirmation message
3. Automatically reboot after 3 seconds with the new settings

---

## Debug Log Viewer

When debug logging is enabled, a **View Debug Log** link appears in the Debug section. This opens a separate page at `/debuglog` that shows the full debug log with a **Clear Log** button.

The log viewer requires the same login credentials as the main portal.

---

## API Endpoints

For programmatic access, the portal exposes these endpoints (all require authentication):

| Method | Path | What it returns |
|--------|------|----------------|
| `GET /` | Main page | HTML configuration form |
| `POST /save` | Save | Writes config and reboots |
| `GET /status` | Status | JSON with firmware version, uptime, free heap, SD status |
| `GET /debuglog` | Debug log | Log viewer page |
| `POST /cleardebuglog` | Clear log | Wipes the debug log file |

### Example `/status` Response

```json
{
    "firmware": "0.0.1",
    "uptime": 1234,
    "free_heap": 245760,
    "sd_mounted": true
}
```

---

## Captive Portal Detection

The portal automatically redirects captive portal detection requests from Android, iOS, and Windows so the config page opens as soon as you connect to the WiFi.

---

## Exiting Config Mode

| Method | What happens |
|--------|-------------|
| **Save & Reboot** | Settings saved, device restarts in normal mode |
| **G0 button** | Press to exit and reboot without saving |
| **Timeout** | After 5 minutes idle, device reboots automatically |
