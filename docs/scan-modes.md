# Scan Modes — Passive vs Active, Hop vs All

A guide to how the four scan modes work, why the defaults were chosen, and how to tune them for different environments.

---

## The Two Dimensions

Every scan mode is a combination of two independent choices:

| Dimension | Option A | Option B |
|-----------|----------|----------|
| **How to find APs** | `active` — transmit probe requests | `passive` — listen for beacons |
| **How to cover channels** | `hop` — one channel at a time | `all` — all channels in one operation |

This gives four modes: `active_all`, `active_hop`, `passive_all`, `passive_hop`. The default is `active_all`.

---

## Active vs Passive Scanning

### Active Scanning

In active mode the Cardputer transmits a **probe request** frame on each channel. Access points that receive the probe request respond immediately with a **probe response**, regardless of whether they are currently broadcasting a beacon.

This means active scanning:

- Discovers APs faster because responses are solicited on demand
- Will detect APs with long beacon intervals, low TX power, or that are momentarily "quiet"
- Also finds **hidden SSIDs** (networks with beacon SSID broadcast disabled) — the firmware sets `show_hidden = true` for all scans, but the probe response from a hidden AP may still reveal it to other listeners
- Produces radio transmissions that can be detected by a wireless intrusion detection system (WIDS) or a sufficiently attentive human with a packet capture tool

The ESP-IDF active scan uses a **minimum dwell** of 20 ms (hardcoded — the radio considers the channel done early if probe responses stop arriving) and a **maximum dwell** of `active_dwell_ms` (default 150 ms). In practice, most channels complete at or near the minimum.

### Passive Scanning

In passive mode the Cardputer transmits nothing. It simply listens on each channel for **beacon frames** that APs broadcast periodically. The 802.11 standard specifies a default beacon interval of 100 ms (102.4 ms precisely), meaning a fully active AP sends roughly 10 beacons per second.

This means passive scanning:

- Is entirely silent — no probe requests are transmitted
- Requires enough dwell time to be confident of catching at least one beacon from every AP
- Will miss APs that have a non-standard beacon interval or are temporarily silent
- Is slower per channel than active scanning because you cannot solicit an immediate response

The passive dwell time is set by `passive_dwell_ms` (default 250 ms). At 250 ms, any AP following the standard 100 ms beacon interval will send ~2–3 beacons while the scanner is listening on that channel, giving a high probability of detection even if the first beacon is lost to interference.

### Active vs Passive: Side-by-Side

| Characteristic | Active | Passive |
|----------------|--------|---------|
| Transmits frames | Yes (probe requests) | No |
| Detectable by WIDS | Yes | No |
| Discovers APs | Via probe response | Via beacon frames |
| Minimum useful dwell | ~100 ms (default 150 ms) | ~200 ms (default 250 ms) |
| Relative speed | Faster | Slower |
| Finds hidden SSIDs | More likely | Less likely |

---

## Hop vs All Channel Modes

### Hop Mode (`active_hop` / `passive_hop`)

Hop mode iterates through every channel sequentially, one at a time. The scanner dwells on channel 1, harvests results, moves to channel 2, and so on up to the maximum channel for your country. After all channels are visited, the results are aggregated into one sweep.

Because the radio is only ever tuned to one channel at a time, hop mode gives each channel exactly the configured dwell time with no scheduling contention between channels.

### All-Channel Mode (`active_all` / `passive_all`)

All-channel mode hands a single scan request to the ESP-IDF WiFi driver with `channel = 0` (scan all). The driver's internal scheduler distributes time across all channels. From the application's perspective, the scan starts and completes in one operation rather than N sequential operations.

This tends to be slightly more efficient because the ESP-IDF scheduler can optimise channel transitions, and there is no per-channel round-trip overhead to the application layer. The dwell time setting still controls how long the radio spends on each channel within that single operation.

### Hop vs All: Side-by-Side

| Characteristic | Hop | All |
|----------------|-----|-----|
| Channel control | Application iterates channels | ESP-IDF schedules all channels |
| Overhead | Small per-channel round-trip | Single start/complete cycle |
| Dwell time control | Exact — full dwell on every channel | Per-channel dwell applied by driver |
| Suited for | Stationary surveys, close-range | Moving vehicles, general wardriving |

---

## Default Values and Why They Were Chosen

| Setting | Default | Reasoning |
|---------|---------|-----------|
| `scan_mode` | `active_all` | Fastest overall coverage — best for wardriving from a moving vehicle where time on any given location is limited. Active mode finds APs quickly; all-channel mode minimises overhead. |
| `active_dwell_ms` | `150` | Allows the radio up to 150 ms to collect probe responses. Most APs respond within 20–50 ms, so 150 ms is conservative enough to catch slow or overloaded APs without wasting time. |
| `passive_dwell_ms` | `250` | With a standard 100 ms beacon interval, 250 ms gives the radio 2–3 full beacon intervals per channel. This provides high confidence of catching at least one beacon even if one is lost to interference or channel congestion. |
| `scan_delay_ms` | `150` | A short pause between full sweeps. This gives the ESP32 CPU time to process and log results, flush the data buffer, and update the display without stalling, before the next sweep begins. |

### Total Sweep Time

Total sweep time depends on the mode, dwell time, channel count, and scan delay:

$$T_{\text{sweep}} = (\text{channels} \times \text{dwell\_ms}) + \text{scan\_delay\_ms}$$

With default settings (13 channels, most regions):

| Mode | Channels | Dwell | Sweep time | + Delay | Cycles/min |
|------|----------|-------|-----------|---------|------------|
| `active_all` | 13 | 150 ms | ~1,950 ms | +150 ms | ~29 |
| `active_hop` | 13 | 150 ms | ~1,950 ms | +150 ms | ~29 |
| `passive_all` | 13 | 250 ms | ~3,250 ms | +150 ms | ~17 |
| `passive_hop` | 13 | 250 ms | ~3,250 ms | +150 ms | ~17 |

For US/Canada (11 channels), multiply channel count by 11 instead of 13.

> **Note:** Active scans may complete faster than the `active_dwell_ms` ceiling if probe responses arrive early. The ESP-IDF uses the minimum dwell (20 ms hardcoded) as an early-exit threshold. Passive scans always use the full `passive_dwell_ms` — there is no early exit.

---

## Configuration Examples for Different Environments

The examples below show the `scan` block of `config.json`. All other settings stay at their defaults unless noted.

### 1 — Wardriving from a Vehicle (default)

Optimised for maximum coverage rate when moving at road speed. You pass each AP quickly, so short dwell and fast cycles matter more than thoroughness.

```json
"scan": {
    "scan_mode": "active_all",
    "active_dwell_ms": 150,
    "passive_dwell_ms": 250,
    "scan_delay_ms": 150
}
```

Approx. sweep time: **2.1 seconds** | Cycles/min: **~29**

---

### 2 — Dense Urban Area (on foot, high AP density)

In cities with hundreds of APs per block, even a short dwell captures plenty of networks. Reducing dwell and delay slightly increases throughput without meaningfully reducing discovery — at high AP density, APs respond quickly and reliably.

```json
"scan": {
    "scan_mode": "active_all",
    "active_dwell_ms": 100,
    "passive_dwell_ms": 200,
    "scan_delay_ms": 100
}
```

Approx. sweep time: **1.4 seconds** | Cycles/min: **~43**

---

### 3 — Suburban or Rural Area (on foot, low AP density)

In low-density areas APs may be at the edge of range. A longer dwell gives the radio more time to collect weak probe responses or catch sparse beacons. Increasing scan delay slightly reduces power consumption without meaningfully affecting results.

```json
"scan": {
    "scan_mode": "active_all",
    "active_dwell_ms": 250,
    "passive_dwell_ms": 400,
    "scan_delay_ms": 250
}
```

Approx. sweep time: **3.5 seconds** | Cycles/min: **~17**

---

### 4 — Stationary Site Survey

When you are stationary and want a thorough, accurate picture of all nearby networks on all channels, increase dwell times significantly. Hop mode is preferred here because it gives you explicit per-channel control and makes it easier to correlate channel load with discovery rate.

```json
"scan": {
    "scan_mode": "passive_hop",
    "active_dwell_ms": 150,
    "passive_dwell_ms": 800,
    "scan_delay_ms": 500
}
```

Approx. sweep time: **~11 seconds per sweep** | Cycles/min: **~5**

With 800 ms per channel, any AP following a standard 100 ms beacon interval will send ~8 beacons while the scanner is listening, providing very high confidence of detection even for APs with occasional beacon loss.

---

### 5 — Stealth / Passive Only

If you need to operate without transmitting any frames — for example, when monitoring a network you are authorised to test without revealing the scanner's presence to WIDS — use a passive mode. Passive all-channel is the simplest choice.

```json
"scan": {
    "scan_mode": "passive_all",
    "passive_dwell_ms": 350,
    "scan_delay_ms": 200
}
```

Approx. sweep time: **~4.7 seconds** | Cycles/min: **~13**

Increasing `passive_dwell_ms` above 300 ms is recommended here to compensate for the fact that you cannot solicit responses and must wait for scheduled beacons.

---

### 6 — Extended Battery Life

If you need to run for many hours on a single charge, reducing the scan duty cycle saves meaningful power. Using `active_all` (fastest per sweep) combined with a longer `scan_delay_ms` keeps the radio off more of the time.

```json
"scan": {
    "scan_mode": "active_all",
    "active_dwell_ms": 120,
    "scan_delay_ms": 800
}
```

Approx. sweep time: **~2.4 seconds active + 0.8 s delay** | Cycles/min: **~19**

The radio is idle for ~800 ms of every ~3.2 s cycle, reducing average current draw from the WiFi transceiver. Note that actual battery life is dominated more by the display brightness (`screen_brightness`) than the scan duty cycle — consider lowering brightness or pressing `B` to blank the display.

---

## Tuning Tips

### If you are missing nearby networks

- Increase `active_dwell_ms` to 200–300 ms. Some APs (especially older or heavily loaded devices) are slow to respond to probe requests.
- If using a passive mode, increase `passive_dwell_ms` to at least 300 ms. A value of 250 ms catches most APs but a particularly overloaded channel may lose one or two beacons to collisions.
- Ensure your `wifi_country_code` matches your actual country. Using the wrong code restricts the channel list and TX power, which reduces range.

### If your session CSV has fewer networks than expected

- Check Dashboard A for the sweep count (shown as `S:` or sweeps). A low sweep count after a long drive suggests dwell times are too high and you are spending too long on each channel.
- Switch from `passive` to `active` mode. Active scanning is significantly more reliable for coverage under time pressure.

### If you want to reduce missed APs on a specific channel

- Switch from `active_all` / `passive_all` to `active_hop` / `passive_hop`. In `_all` mode the ESP-IDF internal scheduler manages time across channels, and a busy channel may occasionally receive slightly less dwell time. In `_hop` mode the application guarantees the exact configured dwell on every channel.

### If the device feels sluggish or the display lags

- The display and data logger run between sweeps during `scan_delay_ms`. If `scan_delay_ms` is below 100 ms, updates may be deferred. Increase `scan_delay_ms` to 150–250 ms.

### Active dwell minimum and maximum

The valid range for both `active_dwell_ms` and `passive_dwell_ms` is **10–2000 ms**. Values outside this range are clamped on save. For active scanning, values below 80 ms may miss some APs on congested channels. For passive scanning, values below 150 ms are not recommended — you risk missing APs whose beacons fall slightly outside the listen window.

### Country code and channel count

The number of channels scanned — and therefore total sweep time — depends on your `wifi_country_code`:

| Region | Channels | Example codes |
|--------|----------|---------------|
| North America | 1–11 | `US`, `CA`, `MX` |
| Europe, Australia, most others | 1–13 | `GB`, `DE`, `AU`, `FR` |
| Japan | 1–14 | `JP` |
| Default (no override) | 1–13 | `Default` |

Setting the correct country code is also important for regulatory compliance — it controls maximum TX power in addition to channel range.

---

## See Also

- [Configuration](configuration.md) — full reference for all scan settings
- [Web Interface](web-interface.md) — editing scan settings via the browser portal
- [Operation](operation.md) — Dashboard F shows the active scan mode and model
