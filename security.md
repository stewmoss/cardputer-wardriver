# Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| 0.5.x   | Yes       |
| < 0.5.0 | No        |

Only the latest release receives security fixes. Upgrade to the current version before reporting an issue.

---

## Reporting a Vulnerability

**Do not open a public issue for security vulnerabilities.**

Instead, please report them privately:

1. **Github:** Send details via the contact information on their GitHub profile ([@stewmoss](https://github.com/stewmoss)).
2. **X - Twitter:** Send details via X /Twitter ([@stewartmoss](https://x.com/stewartmoss)).
3. **GitHub Private Vulnerability Reporting:** If enabled on the repository, use the **Security → Report a vulnerability** tab on GitHub.

Include the following in your report:

- Firmware version and build configuration
- Description of the vulnerability and its potential impact
- Steps to reproduce (or a proof-of-concept if possible)
- Any suggested mitigation or fix

You can expect an initial acknowledgement within **7 days**. Confirmed issues will be patched in the next release and credited in the release notes (unless you prefer to remain anonymous).

---

## Security Model

The Cardputer Wardriver is an **offline-capable, single-user embedded device**. It is not designed to be internet-facing or to operate on untrusted networks. When WiGLE upload is enabled, it can connect to a user-configured WiFi network and submit CSV files to WiGLE over HTTPS. The threat model reflects this — security controls focus on protecting local data, limiting exposure during the WiFi configuration portal, and avoiding accidental disclosure of upload credentials.

### Attack Surface

| Surface | Exposure | Notes |
|---------|----------|-------|
| Web config portal (HTTP) | Local WiFi only | Active only in Config Mode; AP or STA, port 80 |
| WiFi AP (`M5-Wardriver`) | Short-range RF | Open during Config Mode; password-less by default |
| WiGLE upload | Configured WiFi only | Optional STA connection to the configured SSID, then HTTPS upload to WiGLE |
| SD card | Physical access | Config file and CSV logs stored unencrypted |
| USB-C serial | Physical access | Debug serial output at 115200 baud |

### Built-in Protections

| Feature | Description |
|---------|-------------|
| **HTTP Basic Auth** | The web portal requires a username and password (configurable via `admin_user` / `admin_pass`). All routes except captive-portal redirects are protected. |
| **Portal idle timeout** | The web portal auto-closes after 5 minutes of inactivity, reducing the window of exposure. |
| **MAC randomisation** | The device rotates its WiFi MAC address during active scans (configurable interval). This limits tracking by nearby access points. |
| **OUI spoofing option** | When enabled, randomised MACs mimic real manufacturer prefixes, making the device less distinguishable. When disabled, the locally-administered bit is set (safer, but identifiable as randomised). |
| **SSID / BSSID exclusion** | Exclude your own networks from being logged — useful for keeping home/office APs out of uploaded data. |
| **GPS geofencing** | Define bounding-box zones where logging is automatically suppressed (e.g., your home area). |
| **Safe shutdown** | Press `Q` to flush all buffered data and unmount the SD card cleanly before power-off. |

---

## Known Limitations & Hardening Recommendations

### Change the default credentials immediately

The web portal ships with default credentials (`admin` / `password`). **Change these on first boot** via the web interface or by editing `wardriverconfig.txt` on the SD card before inserting it.

### Web portal uses HTTP, not HTTPS

The ESP32 web server uses plain HTTP. Credentials are sent as Base64-encoded Basic Auth headers, which can be intercepted on the local WiFi segment. Mitigation:

- Only use Config Mode in a trusted physical environment.
- Keep portal sessions short — the 5-minute idle timeout helps.
- Change your password after each portal session if operating in a shared space.

### Config file stores credentials in plaintext

`/wardriver/wardriverconfig.txt` on the SD card contains `admin_user`, `admin_pass`, WiFi upload credentials, and WiGLE API credentials in cleartext JSON. Mitigation:

- Keep physical control of your SD card.
- Do not share SD card images or config files without redacting the `web` and `upload` sections.

### SD card data is unencrypted

CSV log files and debug logs are stored in plaintext. Anyone with physical access to the SD card can read them. Mitigation:

- Remove the SD card when not in use.
- Encrypt sensitive files on your PC after retrieval if the data is sensitive to you.

### WiFi AP is open by default

When the device enters Config Mode, it creates an open (unencrypted) WiFi access point named `M5-Wardriver`. Any nearby device can connect. Mitigation:

- Only enter Config Mode when you intend to change settings.
- Complete configuration quickly and reboot into scanning mode.
- Be aware of your physical surroundings.

### Debug log may contain sensitive information

When `debug.enabled` is `true`, the debug log at `/wardriver/logs/debug.log` records detailed operational data including GPS coordinates, scan results, and system state. Mitigation:

- Disable debug logging in production use (`debug.enabled = false`).
- Clear the debug log via the web portal (`/cleardebuglog`) before sharing the SD card.

---

## Responsible Use

During scanning, this tool receives WiFi beacon frames that access points broadcast publicly. It does **not** connect to, probe, or interact with discovered networks. If WiGLE upload is enabled, it connects only to the configured upload SSID and the WiGLE HTTPS API to submit your CSV files. Regardless, **always comply with local laws and regulations** regarding wireless monitoring and data collection in your jurisdiction.

---

## Dependencies

The firmware relies on the following third-party libraries. Keep them updated to benefit from upstream security fixes:

| Library | Purpose |
|---------|---------|
| [M5Cardputer](https://github.com/m5stack/M5Cardputer) | Hardware abstraction |
| [ArduinoJson](https://github.com/bblanchon/ArduinoJson) v6 | Config parsing |
| [FastLED](https://github.com/FastLED/FastLED) | RGB LED control |
| [TinyGPSPlus](https://github.com/mikalhart/TinyGPSPlus) | GPS NMEA parsing |
| [ESP32-targz](https://github.com/tobozo/ESP32-targz) | Gzip artifact generation for WiGLE upload |
| [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32) | WiFi, WiFiClientSecure, SPI, WebServer |

---

## Changelog

Security-relevant changes are noted in the [release notes](docs/ReleaseNotes/).
