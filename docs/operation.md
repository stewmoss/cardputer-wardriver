# Operation Guide

Everything you need to know about using the wardriver day-to-day — what's on screen, what the keys do, and what the LED colours mean.

---

## Keyboard Quick Reference

| Key | Action |
|-----|--------|
| **ENTER** | Cycle dashboard views (A → B → C → D → E) |
| **H** | Show/hide keyboard help overlay |
| **B** | Toggle display on/off (works on shutdown screen too) |
| **C** | Enter Config Mode |
| **S** | Mute/unmute buzzer |
| **Q** | Safe shutdown |
| **X** | Stop/start scanning (low-power toggle) |
| **P** | Pause/resume CSV logging |
| **G0 button** | Boot: force Config Mode · Running: drop FLAG marker · Shutdown/Config: reboot |

> **Tip:** Press **H** at any time to see this list on-screen. All other keys are blocked while the help overlay is showing.

---

## Boot Sequence

| Step | What happens | Duration |
|------|-------------|----------|
| 1 | Splash screen (project name + firmware version) | ~1 s |
| 2 | SD card mounts, config loads | ~1 s |
| 3 | Satellite search begins | Until GPS locks |
| 4 | System clock syncs from GPS | Automatic |
| 5 | Active scanning — logging to CSV | Continuous |

If no config file is found at step 2, the device jumps straight into [Config Mode](web-interface.md).

---

## Dashboard Views

Press **ENTER** to cycle through five views. Each shows different information about your session.

### A — Summary (Main View)

Your primary view while wardriving. Shows the big picture at a glance.

```
┌──────────────────────────────────┐
│ SAT:8  FIX:3D  wardriving_001   │
├──────────────────────────────────┤
│           247                    │
│        Unique APs                │
│  Total Logged: 1,832             │
│  Uptime: 01:23:45               │
│  51.5074°N  -0.1278°W           │
│  Alt: 45m                        │
├──────────────────────────────────┤
│ v0.0.1               01:23:45   │
└──────────────────────────────────┘
```

- **Unique APs** — distinct networks seen this session (large number)
- **Total Logged** — total CSV rows written (includes repeated sightings)
- **SAT / FIX** — satellite count and fix quality
- **Coordinates** — current GPS position and altitude

Status overlays appear on this view when relevant:
- Red **NO FIX** — GPS fix lost or accuracy too low
- Red **STOPPED** — scanning stopped via `X`
- Orange **LOG PAUSED** — logging paused via `P`
- Yellow **GPS:0** or **GPS:LAST** — non-default GPS logging mode active

### B — Security Snapshot

Breaks down the networks you've found by encryption type.

```
┌──────────────────────────────────┐
│ SNAPSHOT                         │
├──────────────────────────────────┤
│ Open: 12  WPA2: 198  WPA3: 37   │
│ Top Signals:       Visible: 24  │
│ -42dBm HomeWiFi                  │
│ -55dBm CoffeeShop               │
│ -61dBm FreePublic               │
├──────────────────────────────────┤
│ ENT=next H=Help         B=72%   │
└──────────────────────────────────┘
```

- **Open / WPA2 / WPA3** — encryption breakdown
- **Top Signals** — 3 strongest networks from the last scan (colour-coded by signal strength)
- **Visible** — total APs currently in range

Signal strength colours: **Green** = excellent (≥ -50 dBm), **Yellow** = good (-50 to -70), **Red** = weak (< -70)

### C — Live Feed

A scrolling list of the **last 10 access points** seen across all scan cycles, updated every 2 seconds. Useful for confirming the scanner is working while you're moving.

Each row shows the time, SSID (or raw MAC if hidden), and signal strength.

### D — New AP Discovery Feed

Like the live feed, but only shows **brand-new networks** — the last 10 BSSIDs seen for the first time this session. Each network appears here only once.

### E — System Settings

Shows battery level, RAM usage, and sound status. This data is read once when you switch to this view (press ENTER to leave and come back to refresh it).

- **Battery %** — colour-coded: green (>50%), yellow (20–50%), red (<20%)
- **Total / Free RAM** — heap + PSRAM in KB
- **Sound** — ON (green) or MUTED (red)

---

## LED Indicator

The front LED changes colour based on device state:

| Colour | Meaning |
|--------|---------|
| **Yellow** | Searching for GPS satellites |
| **Blue** | WiFi scan in progress |
| **Green** | Ready — GPS locked, between scan cycles |
| **Dim Blue** | Scanning stopped (X key) |
| **Purple** | Config Mode active |
| **Red** | Error or low battery warning |
| **Off** | Shutdown complete |

See the full [LED Indicator](led-indicator.md) guide for brightness settings and detailed behaviour.

---

## Battery Monitoring

Battery level is checked every 60 seconds and shown in the footer of all dashboard views as **B=XX%** (or **B=---** during the first minute after boot).

### Low Battery Auto-Shutdown

When battery drops to or below the `low_battery_threshold` (default: 5%), a 10-second countdown appears. The LED turns red and the keyboard is locked. After the countdown, logs are flushed, the SD card is unmounted, and the shutdown summary is displayed.

Set `low_battery_threshold` to `0` in config to disable this feature.

---

## Buzzer

A short beep plays each time a **new unique AP** (previously unseen BSSID) is discovered. The tone frequency and duration come from your config settings.

- Press **S** to mute/unmute at any time
- Set `buzzer_enabled` to `false` in config to disable permanently

---

## Stop/Start Scanning (X Key)

Press **X** to completely stop the WiFi radio and GPS. The device enters a low-power idle state:

- LED turns dim blue
- Dashboard A shows a red **STOPPED** overlay
- Current CSV file is flushed and closed

Press **X** again to resume — a **new CSV file** is created with the next sequence number. Useful for taking breaks without powering off.

---

## Pause/Resume Logging (P Key)

Press **P** to pause CSV writing while scanning continues. Unlike stop (X), everything keeps running — dashboards update, GPS tracks — but nothing is written to the SD card.

- Dashboard A shows an orange **LOG PAUSED** overlay
- Press **P** again to resume writing to the **same CSV file**

This is useful for passing through areas you don't want to log (e.g., private property, parking garages).

> **Note:** Pause isn't available when scanning is stopped. Start scanning first with `X`, then use `P`.

---

## FLAG Markers (G0 Button)

During active scanning with a GPS fix, press the physical **G0 button** on the front of the device to drop a bookmark in the CSV file. This creates a special entry you can find later:

| CSV Field | Value |
|-----------|-------|
| MAC | `00:00:00:00:00:00` |
| SSID | `FLAG` |
| AuthMode | `[FLAG]` |

The marker records your exact GPS coordinates and timestamp. A confirmation beep plays when the flag is dropped.

---

## GPS Fix Lost Warning

If GPS accuracy degrades or the fix is lost entirely during scanning, Dashboard A shows a prominent red **NO FIX** warning. CSV logging pauses automatically (there are no valid coordinates to record), but WiFi scanning continues normally.

The warning clears and logging resumes as soon as the GPS reacquires an acceptable fix.

---

## MAC Address Randomisation

When `mac_randomize_interval` is set (default: `1`), the device rotates its WiFi MAC address before scans. This prevents access points and wireless IDS from tracking your device.

- **Default mode** — generated MACs use the locally-administered bit (safe, no conflicts)
- **OUI spoof mode** — ~50% of MACs look like real manufacturer addresses (harder to fingerprint)

The original hardware MAC is automatically restored on reboot.

---

## Data Logging

### CSV Output

- Saved to `/wardriver/wardriving_NNN.csv` (new file each boot or after stop/start)
- Format: WiGLE CSV v1.6 — upload directly to [wigle.net/uploads](https://wigle.net/uploads)
- Every detected network is logged every scan cycle
- A 10 KB write buffer batches SD writes (auto-flushes every 5 s or at 80% full)

### Uploading to WiGLE

1. Press `Q` for safe shutdown (or power off)
2. Remove the SD card and plug it into your computer
3. Go to `/wardriver/` and upload any `wardriving_*.csv` to [wigle.net/uploads](https://wigle.net/uploads)

> **Warning:** Don't open CSV files in Excel before uploading — it can alter the encoding and cause WiGLE to reject the file.

---

## Filtering

Three types of filters can exclude networks from your log files:

| Filter | How it works |
|--------|-------------|
| **SSID** | Exact name match (case-sensitive) — keeps your home WiFi out of logs |
| **BSSID** | MAC address match (case-insensitive) — excludes specific access points |
| **Geofence** | GPS bounding box — suppresses all logging inside the area |

There's also an **accuracy guard** that automatically skips logging when GPS accuracy (HDOP) is too low.

> **Note:** Filtered networks still count toward the Unique APs number on screen — they just don't appear in the CSV file.

---

## Shutdown

Press **Q** for a safe shutdown. The device:
1. Stops WiFi scanning
2. Flushes the write buffer to SD
3. Closes the CSV file
4. Unmounts the SD card
5. Shows a session summary

The summary includes total logged rows, unique APs, session duration, number of scan cycles, peak APs per sweep, and average scan time.

From the shutdown screen:
- Press **G0** to reboot
- Press **B** to turn off the display (for faster charging)

---

## Typical Session

1. Insert SD card, power on
2. First boot? Set up via the web portal. Otherwise, it goes straight to satellite search.
3. Wait for GPS lock (yellow LED → green)
4. Walk or drive — logging is automatic
5. Press `ENTER` to check different dashboards, `H` for help
6. Use `P` to pause through areas you want to skip
7. Use `X` to take a break without powering off
8. Press `Q` when done — remove SD card and upload to WiGLE
