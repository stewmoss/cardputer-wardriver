# Cardputer Wardriver — WiGLE Upload

How to get your wardriving data onto WiGLE automatically (or with one keypress).

---

## What is WiGLE?

[WiGLE](https://wigle.net) (Wireless Geographic Logging Engine) is a community-maintained global database of WiFi and cellular networks. When you upload your CSV files, your scans are added to the map and your account statistics go up. It's the standard destination for wardriving data.

The Wardriver records everything in WiGLE v1.6 CSV format, so no conversion is needed — the files go straight up.

---

## Two Ways to Upload

The Wardriver gives you two upload paths. You can use one, both, or neither.

| Method | When it runs | What triggers it |
|--------|-------------|-----------------|
| **Auto-upload** | At every boot, before scanning starts | Your home WiFi is visible during the 3-second boot scan |
| **Manual upload** | On demand | You press `Q` to shut down, then `U` on the shutdown screen |

Both paths use the same upload pipeline and the same configuration.

---

## Getting Your WiGLE API Credentials

Before you configure anything on the device, grab your API credentials from wigle.net.

1. Log in to [wigle.net](https://wigle.net).
2. Go to **Account → API Access** (or visit `https://wigle.net/account`).
3. Copy your **API Name** and **API Token** — you'll need both.

> **Note:** The API Name is your WiGLE username, not your email address. The token is the long string shown under your account settings. Keep them safe — they authenticate uploads to your account.

---

## Setting Up in the Web Portal

Open the web portal (press `C` while running, or hold G0 at boot), then go to the **Upload** tab.

| Field | What to enter |
|-------|--------------|
| **Enable WiGLE upload** | Turn this on first — it's the master switch for all upload paths |
| **Auto-upload at boot** | Turn on to enable the automatic boot-time check |
| **Home WiFi SSID** | The exact name of your home/base WiFi network |
| **Home WiFi Password** | The password for that network (leave blank for open networks) |
| **WiGLE API Name** | Your WiGLE API name from the credentials step above |
| **WiGLE API Token** | Your WiGLE API token from the credentials step above |
| **Gzip before upload** | Leave this on — it compresses files before sending, saving time and data |
| **Delete after upload** | Off by default; uploaded files are moved to `/wardriver/uploaded/` instead |
| **Minimum sweeps per file** | Files with fewer than this many GPS-sweep entries are held back (see [Thin Files](#thin-files-minimum-sweeps)) |
| **Retry quarantined files** | Re-evaluates previously held-back files on the next upload run |

Hit **Save** and the device will reboot with your new settings applied.

---

## Auto-Upload at Boot

When both `wigle_upload_enabled` and `auto_upload` are on, every boot runs through a quick check before satellite search begins.

### What happens, step by step

1. **Scanning for home network** — The device scans for your configured SSID for up to 3 seconds.
2. **Network found** — A 10-second countdown appears on screen. Press `G0` during the countdown to skip and go straight to scanning.
3. **Battery check** — If the battery is below 15%, you'll get a prompt. The device skips upload automatically after 5 seconds if you don't respond.
4. **Uploading** — Each pending CSV is compressed and uploaded one at a time. The screen shows the filename, a progress bar, and which file out of how many you're on.
5. **Summary** — After all files are processed, a completion screen shows totals: files uploaded, any failures, skipped thin files, and record count. Press any key to continue to normal boot.

If the home network isn't visible during the 3-second scan, upload is skipped silently and wardriving starts as normal.

### Cancelling auto-upload

Press **G0** (the front button) at any point during the upload phases to cancel. The device will return to normal boot immediately.

---

## Manual Upload from the Shutdown Screen

If you're not at home but have access to WiFi — or you simply want to upload right now — you can trigger it manually.

1. Press **Q** to safely shut down. The device flushes everything to SD, disconnects WiFi, and shows the session summary.
2. If upload is configured and pending CSV files exist, the footer shows `U=Upload  G0=Reboot  B=Off`.
3. Press **U**. The device re-mounts the SD card, enables WiFi, and runs the same upload pipeline.
4. When complete, a summary screen appears. Press any key to return to the shutdown screen.
5. Press **G0** to reboot, or **B** to blank the screen.

> **Note:** Only one upload is allowed per shutdown session. After it finishes the `U` hint disappears.

### Cancelling manual upload

Press **ESC** at any phase during the upload to cancel. The device returns to the locked shutdown screen.

---

## Where Do the Files Go?

After a successful upload the device moves the file out of `/wardriver/` so it won't be picked up again next time.

| Outcome | Where the file ends up |
|---------|----------------------|
| Uploaded successfully | `/wardriver/uploaded/` (or deleted if `delete_after_upload` is on) |
| Failed to upload | Left in `/wardriver/` — will be retried next time |
| Too few sweeps (thin file) | `/wardriver/uploaded/thin/` — see below |

With gzip enabled (the default), the `.csv.gz` file that was actually sent to WiGLE is what gets moved to `/wardriver/uploaded/`. The original `.csv` stays in `/wardriver/` until the file is also removed based on your settings.

---

## Thin Files — Minimum Sweeps

A "sweep" is one GPS-timestamped pass by the scanner. A file with only a handful of sweeps — say, a 30-second test in your driveway — probably isn't worth uploading to WiGLE.

The `min_sweeps_threshold` setting (default: 20) controls how many distinct sweeps a file needs before the upload will include it. Files that don't pass are moved to `/wardriver/uploaded/thin/` instead of being uploaded or deleted.

The completion summary shows a **Skipped** count for thin files so you always know what was held back.

### Getting thin files uploaded later

If you want those files to have another chance, enable **Retry quarantined files** (`retry_thin_files`) in the web portal. The next upload run will pick them up from `/wardriver/uploaded/thin/` and re-evaluate them against the current threshold.

To upload everything regardless of sweep count, set `min_sweeps_threshold` to `0`.

---

## How Files Are Compressed

Before uploading, each CSV is wrapped in a `.csv.gz` gzip file. This typically reduces file size by 70–80%, which makes a noticeable difference when uploading large sessions over a slower home connection.

WiGLE accepts `.csv.gz` natively, so there's nothing special to do on the receiving end.

To send raw CSV files instead, turn off **Gzip before upload** in the web portal. This is slower and uses more data, but can be useful for troubleshooting.

---

## Troubleshooting

See the [WiGLE Auto-Upload section in Troubleshooting](troubleshooting.md#wigle-auto-upload) for a full list of upload-specific problems and fixes.

---

## See Also

- [Configuration](configuration.md) — full table of all upload settings with defaults
- [Web Interface](web-interface.md) — the Upload tab in detail
- [Operation](operation.md) — keyboard controls and shutdown screen behaviour
- [Troubleshooting](troubleshooting.md) — upload-specific problems
