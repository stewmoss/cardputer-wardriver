# Configuration

All settings live in a single JSON file on the SD card:

```
/wardriver/wardriverconfig.txt
```

You can edit this file three ways:
1. **Web portal** — press `C` during operation or hold G0 at boot (see [Web Interface](web-interface.md))
2. **Direct edit** — open the file on your computer with any text editor
3. **Pre-deploy** — copy and modify `config.txt.example` before first boot

If the config file is missing or contains invalid JSON, the device enters **Config Mode** automatically so you can set things up through the web portal.

---

## Quick Reference

Here's the full config with default values. Each section is explained in detail below.

```json
{
    "gps": {
        "tx_pin": 2,
        "rx_pin": 1,
        "accuracy_threshold_meters": 500,
        "gmt_offset_hours": 0,
        "gps_log_mode": "fix_only"
    },
    "scan": {
        "scan_delay_ms": 150,
        "active_dwell_ms": 150,
        "passive_dwell_ms": 250,
        "scan_mode": "active_all",
        "mac_randomize_interval": 1,
        "mac_spoof_oui": false,
        "wifi_country_code": "Default",
        "wifi_tx_power": 84
    },
    "filter": {
        "excluded_ssids": [],
        "excluded_bssids": [],
        "geofence_boxes": []
    },
    "hardware": {
        "buzzer_freq": 2500,
        "buzzer_duration_ms": 50,
        "screen_brightness": 128,
        "led_brightness": 64,
        "buzzer_enabled": true,
        "keyboard_sounds": true,
        "beep_on_new_ssid": false,
        "beep_on_blocked_ssid": false,
        "beep_on_geofence": false,
        "low_battery_threshold": 5
    },
    "debug": {
        "enabled": false,
        "super_debug_enabled": false,
        "system_stats_enabled": true,
        "system_stats_interval_s": 300
    },
    "web": {
        "admin_user": "admin",
        "admin_pass": "password"
    }
}
```

---

## GPS Settings

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `tx_pin` | int | `2` | GPIO pin connected to the GPS module's TX line |
| `rx_pin` | int | `1` | GPIO pin connected to the GPS module's RX line |
| `accuracy_threshold_meters` | float | `500` | Maximum HDOP value for logging. Scans are skipped when accuracy is worse than this. Lower = stricter. |
| `gmt_offset_hours` | int | `0` | Your timezone offset from UTC (e.g., `-5` for EST, `10` for AEST). Affects timestamps in debug logs and on SD card files. Range: -12 to +14. |
| `gps_log_mode` | string | `"fix_only"` | What to do when GPS fix is lost — see below |

### GPS Logging Modes

Three modes control what happens when the GPS fix is lost or accuracy drops below the threshold. All three modes behave identically while a live 3D fix is available — they only differ when the fix is lost.

#### Fix Only (default)

```json
"gps_log_mode": "fix_only"
```

CSV logging pauses when the fix is lost and resumes automatically when it returns. No data is written with inaccurate or missing coordinates.

**Benefits:** Cleanest data for WiGLE uploads. Every row has a verified GPS position. Best for accurate coverage mapping.

**Tradeoff:** Networks seen during GPS outages (tunnels, parking garages, dense urban canyons) are silently dropped.

#### Zero GPS

```json
"gps_log_mode": "zero_gps"
```

Logging continues with coordinates set to `0.000000, 0.000000` and altitude `0.0`. The accuracy field retains the last known value. Geofence filtering is bypassed (since coordinates are zeros), but SSID/BSSID exclusion lists still apply.

**Benefits:** Captures every network regardless of GPS status. Useful for indoor surveys, cataloguing SSIDs in buildings, or environments where GPS is unreliable. The zero-coordinate rows are easy to filter out in post-processing.

**Tradeoff:** WiGLE will ignore rows with 0,0 coordinates, so these entries won't appear on the map. You still get SSID, BSSID, encryption, signal strength, and channel data.

#### Last Known

```json
"gps_log_mode": "last_known"
```

When the fix drops, the device reuses the last valid GPS position from earlier in the session. Logging only starts once the first fix has been acquired — if the device has never had a fix this session, no data is written.

**Benefits:** Keeps data flowing during brief GPS outages while maintaining approximately correct coordinates. Good for urban wardriving where you pass through short tunnels or under bridges.

**Tradeoff:** Coordinates become stale the longer the outage lasts. Networks logged during a long GPS gap will all share the same position, which can cluster unrelated APs together on the map.

#### Quick Comparison

| Mode | Logs without fix? | Coordinates when no fix | WiGLE compatible? | Best for |
|------|-------------------|------------------------|-------------------|----------|
| `fix_only` | No | — | Yes | Accurate outdoor mapping |
| `zero_gps` | Yes | 0, 0 | Rows ignored | Indoor surveys, SSID cataloguing |
| `last_known` | Yes (after first fix) | Last valid position | Yes (approximately) | Brief GPS outages, urban driving |

---

## Scan Settings

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `scan_delay_ms` | int | `150` | Delay between WiFi scan cycles in milliseconds. Lower = more frequent scans but higher power usage. |
| `active_dwell_ms` | int | `150` | Time spent on each channel during active scans (sends probe requests). Range: 10–2000. |
| `passive_dwell_ms` | int | `250` | Time spent on each channel during passive scans (just listens for beacons). Higher values catch more APs. Range: 10–2000. |
| `scan_mode` | string | `"active_all"` | Scanning strategy — see below. Requires reboot to take effect. |
| `mac_randomize_interval` | int | `1` | How often to randomise the device MAC before scanning. `0` = disabled, `1` = every scan, `N` = every Nth scan. Range: 0–100. |
| `mac_spoof_oui` | bool | `false` | When `false` (default), randomised MACs are flagged as locally-administered (safe). When `true`, ~50% of MACs will look like real manufacturer addresses — harder to fingerprint as randomised. |
| `wifi_country_code` | string | `"Default"` | ISO 3166-1 alpha-2 country code for channel and power compliance (e.g. `"US"`, `"GB"`, `"AU"`). `"Default"` uses the ESP32 built-in settings. |
| `wifi_tx_power` | int | `84` | WiFi transmit power in quarter-dBm units (8–84). `84` = 21 dBm (maximum). Lower values reduce range and power consumption. |

### Scan Modes

| Mode | How it works |
|------|-------------|
| `"active_all"` | Sends probe requests, all channels in one sweep |
| `"passive_all"` | Listens for beacons only, all channels in one sweep |
| `"active_hop"` | Sends probe requests, one channel at a time |
| `"passive_hop"` | Listens for beacons only, one channel at a time |

> **Tip:** Active modes find more networks faster but are detectable by wireless IDS. Passive modes are stealthier. The `_all` variants are faster per cycle but may miss networks on busy channels.

---

## Filter Settings

Filters let you exclude specific networks or locations from your log files.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `excluded_ssids` | string[] | `[]` | SSIDs to exclude (exact match, case-sensitive) |
| `excluded_bssids` | string[] | `[]` | MAC addresses to exclude (case-insensitive) |
| `geofence_boxes` | object[] | `[]` | GPS bounding boxes — logging is suppressed inside these areas (max 10) |

### Example: Exclude Your Home Network

```json
"filter": {
    "excluded_ssids": ["MyHomeWiFi", "MyHomeWiFi-5G"],
    "excluded_bssids": ["aa:bb:cc:dd:ee:ff"],
    "geofence_boxes": []
}
```

### Example: Suppress Logging Near Your House

Define a GPS bounding box around your home. Any networks detected while you're inside this area won't be logged:

```json
"geofence_boxes": [
    {
        "top_lat": 37.7850,
        "left_lon": -122.4200,
        "bottom_lat": 37.7800,
        "right_lon": -122.4150
    }
]
```

Each box has four values: `top_lat` (north), `bottom_lat` (south), `left_lon` (west), `right_lon` (east) — all in decimal degrees.

> **Note:** Filtered networks still count toward your Unique APs total on screen, but they won't appear in the CSV file.

---

## Hardware Settings

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `buzzer_freq` | int | `2500` | Buzzer tone frequency in Hz. Range: 100–8000. |
| `buzzer_duration_ms` | int | `50` | How long each beep lasts in milliseconds. Range: 10–500. |
| `screen_brightness` | int | `128` | Display backlight brightness. Range: 10–255. |
| `led_brightness` | int | `64` | Status LED brightness. `0` = off, `255` = maximum. |
| `buzzer_enabled` | bool | `true` | Master switch for all buzzer sounds. When `false`, all sound options below are overridden. |
| `keyboard_sounds` | bool | `true` | Sound feedback for key presses (view cycle, start/stop, help, etc.). |
| `beep_on_new_ssid` | bool | `false` | Beep when a new unique access point is discovered. |
| `beep_on_blocked_ssid` | bool | `false` | Beep when a filtered/excluded access point is scanned. |
| `beep_on_geofence` | bool | `false` | Periodic beep every 10 seconds while GPS is inside a geofence zone. |
| `low_battery_threshold` | int | `5` | Auto-shutdown when battery drops to this percentage. `0` = disabled. |

---

## Debug Settings

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `enabled` | bool | `false` | Write debug output to `/wardriver/logs/debug.log` on the SD card. Serial output is always active regardless. |
| `super_debug_enabled` | bool | `false` | Verbose scan-level logging (MAC changes, per-scan timing, channel results). Only works when `enabled` is also `true`. |
| `system_stats_enabled` | bool | `true` | Periodically log system stats (heap, battery, etc.) |
| `system_stats_interval_s` | int | `300` | How often to log system stats, in seconds |

---

## Web Authentication Settings

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `admin_user` | string | `"admin"` | Username for the web config portal |
| `admin_pass` | string | `"password"` | Password for the web config portal |

The portal uses HTTP Basic Authentication — your browser will prompt for credentials before showing the settings page.

> **Warning:** Change the default password on first setup. Credentials are stored in plain text on the SD card. If both username and password are set to empty strings, authentication is disabled entirely.

---

## Files on the SD Card

| Path | What it is |
|------|-----------|
| `/wardriver/wardriverconfig.txt` | Your config file (JSON) |
| `/wardriver/logs/debug.log` | Debug log (when `debug.enabled` is `true`) |
| `/wardriver/wardriving_001.csv` | First session's WiGLE CSV output |
| `/wardriver/wardriving_002.csv` | Second session (auto-incrementing) |
