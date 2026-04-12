# Troubleshooting

Quick fixes for common problems. If you're stuck, connect via USB and open a serial monitor at **115200 baud** (`pio device monitor`) to see detailed error messages.

---

## Most Common Issues

| Problem | Quick fix |
|---------|-----------|
| Red screen on boot | SD card missing or not FAT32 — reformat and re-insert |
| Enters Config Mode every boot | Config file missing or has bad JSON — save settings via the web portal |
| GPS never gets a fix | Check wiring (TX/RX pins match config), move outdoors with clear sky |
| No networks found | Normal if area is sparse — the ESP32-S3 only scans 2.4 GHz, not 5 GHz |
| WiGLE rejects upload | Don't edit the CSV in Excel — it corrupts the encoding |

---

## Boot & Startup

| Symptom | Cause | Fix |
|---------|-------|-----|
| Red screen: **"SD CARD ERROR!"** | SD card not inserted, not FAT32, or bad contact | Re-seat the card. Reformat as FAT32 if needed. |
| Red screen: **"Cant create /wardriver"** | SD card is write-protected or corrupted | Check the write-protect switch on the card. Try reformatting. |
| Config Mode on every boot | Config file missing or invalid JSON | Save settings via web portal, or manually place a valid `wardriverconfig.txt` on the SD card |
| Stuck on splash screen | Very slow SD card or firmware crash | Try a different SD card (Class 10+). Re-flash the firmware. |
| No display at all | Hardware issue or wrong firmware | Make sure you flashed the M5 Cardputer build. Check battery charge. |

---

## GPS

| Symptom | Cause | Fix |
|---------|-------|-----|
| "Searching..." forever | GPS not connected or wrong pins | Verify wiring matches config (default: TX=2, RX=1). |
| Satellite count stays 0 | Antenna blocked or indoors | Go outside with a clear sky view. Cold start takes 1–5 minutes. |
| Satellites found but no 3D fix | Fewer than 4 satellites tracked | Move to a more open area. Urban canyons slow acquisition. |
| Position jumps around | Poor satellite geometry (high HDOP) | The accuracy guard prevents logging bad fixes. Wait it out. |
| Fix keeps dropping | Loose wiring or intermittent power | Check connections and solder joints. Ensure stable power to GPS module. |
| Wrong time after GPS fix | GMT offset not set | Set `gmt_offset_hours` in config (e.g., `10` for AEST, `-5` for EST) |

---

## WiFi Scanning

| Symptom | Cause | Fix |
|---------|-------|-----|
| Unique APs stays at 0 | WiFi radio issue or no networks nearby | Check serial monitor for errors. Try in a populated area. |
| Very few networks | Scan interval too long or sparse area | Lower `scan_delay_ms`. Drive slower through dense areas. |
| Same networks logged repeatedly | Expected behaviour | The device logs every AP every scan cycle. Unique AP count shows distinct BSSIDs. |
| Phone sees networks the device doesn't | 5 GHz only networks | ESP32-S3 scans 2.4 GHz only. Dual-band networks should still appear on their 2.4 GHz radio. |
| Scanning won't start | Stopped with `X` key | Press `X` again to resume. |
| Data not being logged | Paused or no GPS fix | Press `P` to resume logging. Check for the red "NO FIX" overlay on Dashboard A. |

---

## Config Mode & Web Portal

| Symptom | Cause | Fix |
|---------|-------|-----|
| Can't find the WiFi network | Config Mode not active | Hold G0 during boot, or press `C` during operation |
| Connected but page won't load | Captive portal redirect failed | Go to `http://192.168.4.1` manually |
| Login credentials don't work | Password was changed and forgotten | Remove SD card, delete `/wardriver/wardriverconfig.txt`, reboot to reset to defaults (admin / password) |
| Page is blank | Browser blocking HTTP content | Try a different browser or disable HTTPS-only mode |
| Settings not saving | SD card write error | Check SD card. Look at serial output for errors. |
| Portal times out | 5-minute idle timeout | Reload the page to reset the timer |
| G0 doesn't trigger Config Mode | Not held early enough | Hold G0 **before** powering on and keep holding through the splash screen |

---

## SD Card & Logging

| Symptom | Cause | Fix |
|---------|-------|-----|
| CSV file is empty | No networks passed filters, or no GPS fix | Check filter settings. Make sure GPS has a 3D fix. If you want to log without GPS, set `gps_log_mode` to `"zero_gps"` or `"last_known"`. |
| Entries with 0,0 coordinates | `gps_log_mode` set to `"zero_gps"` | Expected — networks are logged without coordinates when GPS fix is lost. These rows won't appear on the WiGLE map but still record SSID/BSSID/signal data. |
| Stale/repeated coordinates in CSV | `gps_log_mode` set to `"last_known"` | Expected — the last valid GPS position is reused during outages. Coordinates become less accurate the longer the outage lasts. |
| File counter keeps going up | New file each boot | Expected — each session gets its own `wardriving_NNN.csv` |
| "SD write failed" in debug log | SD card full or failing | Check available space. Try a new card. |
| Debug log missing | Debug logging disabled | Enable `debug.enabled` in config |
| WiGLE rejects upload | Header or encoding corrupted | Don't edit the CSV in Excel. Upload the raw file as-is. |

---

## LED & Buzzer

| Symptom | Cause | Fix |
|---------|-------|-----|
| LED stays off | Brightness set to 0 | Increase `led_brightness` in config (try 64) |
| LED stuck on red | Error state | Read the on-screen error message |
| No buzzer sound | Buzzer disabled or muted | Set `buzzer_enabled: true` in config, press `S` to unmute |
| Buzzer too quiet | Frequency outside speaker range | Try `buzzer_freq` between 2000–4000 Hz |

---

## Shutdown

| Symptom | Cause | Fix |
|---------|-------|-----|
| `Q` does nothing | Device is in Config Mode | Exit Config Mode first (G0 button), then press `Q` |
| Data lost after removing SD | Removed without shutting down | Always press `Q` first — the write buffer needs to flush |

---

## Memory & Stability

The ESP32-S3 has limited RAM. These are the firmware's safeguards against memory pressure:

- **Write buffer** — fails safely if allocation fails at startup
- **Scan results** — capped per sweep (`MAX_SCAN_RESULTS_PER_SWEEP`)
- **Unique BSSID tracking** — capped (`MAX_UNIQUE_BSSIDS_TRACKED`)
- **Config Mode re-entry** — old portal instance is cleaned up before creating a new one

If you see unexpected reboots, enable debug logging and capture serial output to identify the last message before the crash.

---

## Factory Reset

To reset everything to defaults:

1. Power off
2. Remove the SD card
3. Delete `/wardriver/wardriverconfig.txt` from the card on a computer
4. Re-insert and power on — the device will enter Config Mode with factory defaults
