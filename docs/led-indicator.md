# LED Indicator

The single RGB LED on the front of the Cardputer tells you what the device is doing at a glance — no need to read the screen.

---

## Colour Reference

| Colour | Meaning |
|--------|---------|
| **Yellow** | Searching for GPS satellites |
| **Blue** | WiFi scan in progress |
| **Green** | GPS locked, ready between scans |
| **Dim Blue** | Scanning stopped (`X` key pressed) |
| **Purple** | Config Mode — web portal active |
| **Red** | Error (SD failure, etc.) or low battery warning |
| **Off** | Shutdown complete — safe to remove SD card |

During active wardriving, the LED alternates between **blue** (during a scan sweep) and **green** (between sweeps). While searching for satellites, it stays **yellow** the whole time.

---

## Brightness

LED brightness is separate from screen brightness. Configure it with `hardware.led_brightness`:

| Value | Good for |
|-------|----------|
| `0` | LED completely off |
| `1–63` | Dark environments |
| `64` | Default — visible but not distracting |
| `128` | Outdoor daytime use |
| `255` | Maximum brightness |

Change it in the [web portal](web-interface.md) (Hardware section) or directly in the [config file](configuration.md).

---

## Hardware Details

| Parameter | Value |
|-----------|-------|
| LED type | SK6812 (GRB) |
| GPIO pin | 21 |
| Driver | FastLED library |

---

## Troubleshooting

| Problem | Fix |
|---------|-----|
| LED stays off | Check `led_brightness` in config — it might be set to `0` |
| LED stuck on red | Read the error message on screen — usually an SD card problem |
| LED flickers blue rapidly | Normal — WiFi channel sweep in progress | This is expected during active scanning. |
| LED stays yellow for a long time | GPS cannot acquire 3D fix | Move outdoors with clear sky view. Check GPS wiring. |
| LED is purple and won't change | Device is in Config Mode | Connect to the web portal, save config, or press G0 to reboot. |
