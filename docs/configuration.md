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
    "device": {
        "model": "auto",
        "detected_model": ""
    },
    "gps": {
        "tx_pin": 2,
        "rx_pin": 1,
        "baud": 115200,
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
    },
    "upload": {
        "wigle_upload_enabled": false,
        "auto_upload": false,
        "upload_ssid": "",
        "upload_password": "",
        "wigle_api_name": "",
        "wigle_api_token": "",
        "compress_before_upload": true,
        "delete_after_upload": false,
        "min_sweeps_threshold": 20,
        "retry_thin_files": false
    }
}
```

---

## Device Settings

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `model` | string | `"auto"` | Hardware profile selection. Valid values: `"auto"`, `"cardputer"`, `"cardputer_adv"`. |
| `detected_model` | string | `""` | Runtime-populated record of the detected or effective hardware model. Normally read-only from the user's perspective. |

When `model` changes, the firmware can reseed profile-driven defaults such as GPS TX/RX pins to match the selected hardware profile.

---

## GPS Settings

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `tx_pin` | int | `2` | Cardputer/ESP32 TX GPIO connected to the GPS module's RX line |
| `rx_pin` | int | `1` | Cardputer/ESP32 RX GPIO connected to the GPS module's TX line |
| `baud` | int | `115200` | GPS UART baud rate. The firmware default is `115200`; `57600` is also commonly used for compatible modules. |
| `accuracy_threshold_meters` | float | `500` | Maximum GPS accuracy estimate in meters for logging. Scans are skipped when accuracy is worse than this. Lower = stricter. |
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
| `system_stats_enabled` | bool | `true` | When `enabled` is also `true`, append structured telemetry to `/wardriver/logs/stats.csv`. Rows include `init`, periodic `run`, `pre_upload`, and `exit` events. |
| `system_stats_interval_s` | int | `300` | How often to append periodic `run` rows to `stats.csv`, in seconds. Lifecycle rows are written immediately. |

`stats.csv` is separate from the WiGLE `wardriving_*.csv` files. It is for diagnostics and captures timestamp/epoch when available, uptime, heap/PSRAM totals, scan and station counters, battery percentage and millivolts, charging state, internal temperature, CPU frequency, RTOS task data, and maximum loop time since the previous stats row.

---

## Upload Settings

Boot-time WiGLE upload runs before GPS acquisition and WiFi scanning. If the required fields are blank, `wigle_upload_enabled` is `false`, or `auto_upload` is `false`, the firmware skips this state and starts normal wardriving. Manual upload (shutdown screen, `U` key) requires `wigle_upload_enabled` to be `true` regardless of the `auto_upload` setting.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `wigle_upload_enabled` | bool | `false` | Master switch for the WiGLE upload subsystem. When `false`, both boot-time auto-upload and the manual upload option on the shutdown screen are disabled |
| `auto_upload` | bool | `false` | Enable boot-time upload of pending `/wardriver/wardriving_*.csv` files. Requires `wigle_upload_enabled` to also be `true` |
| `upload_ssid` | string | `""` | Home WiFi SSID to look for at boot |
| `upload_password` | string | `""` | Home WiFi password. Leave empty only for an open network |
| `wigle_api_name` | string | `""` | WiGLE API name for Basic Auth |
| `wigle_api_token` | string | `""` | WiGLE API token for Basic Auth |
| `compress_before_upload` | bool | `true` | Wrap CSV files as `.csv.gz` before upload |
| `delete_after_upload` | bool | `false` | Delete uploaded artifacts instead of moving them to `/wardriver/uploaded/` |
| `min_sweeps_threshold` | int | `20` | Minimum distinct GPS-timestamp sweeps a CSV must contain to be uploaded. `0` disables the check. Files below the threshold are quarantined to `/wardriver/uploaded/thin/` |
| `retry_thin_files` | bool | `false` | If `true`, files in `/wardriver/uploaded/thin/` are re-evaluated on the next upload run. Ignored when `min_sweeps_threshold = 0` |

Successfully uploaded files are either moved to `/wardriver/uploaded/` or deleted, depending on `delete_after_upload`. When `compress_before_upload` is enabled, the archived file is the uploaded `.csv.gz`; when it is disabled, the archived file is the original `.csv`. The highest uploaded CSV index may leave a 0-byte placeholder behind so future session filenames keep increasing. Older zero-byte placeholders are cleaned automatically; if the highest root CSV is non-empty, no placeholder is retained.

### Minimum Sweeps Quarantine

A "sweep" is one full pass through the WiFi channel list, identified by a unique `FirstSeen` timestamp in the CSV. Files containing fewer than `min_sweeps_threshold` distinct sweeps (typically very short sessions or sessions without a GPS fix) are skipped and moved to `/wardriver/uploaded/thin/` rather than being uploaded. This keeps WiGLE submissions clean of low-value data.

Set `min_sweeps_threshold` to `0` to disable the check entirely. Set `retry_thin_files` to `true` to give quarantined files another chance on the next upload run (useful after raising or lowering the threshold).

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
| `/wardriver/logs/stats.csv` | Structured system stats CSV (when `debug.enabled` and `debug.system_stats_enabled` are both `true`) |
| `/wardriver/wardriving_001.csv` | First session's WiGLE CSV output |
| `/wardriver/wardriving_002.csv` | Second session (auto-incrementing) |
