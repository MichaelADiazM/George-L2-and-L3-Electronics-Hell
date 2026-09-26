# Development Log — Georges Rocket Sensor

## How to use this document

This is a **working baseline document**, written at the start of a refactor, not the final word. Its job is to pin down what actually exists in the codebase right now — including the parts that are half-finished or buggy — so the refactor has a known starting point instead of relying on memory. Once the refactor is done, this gets replaced by a clean, final version (and `TELEMETRY.md` / `README.md` get reconciled with whatever the refactor lands on).

Each chapter below follows the same pattern: a short **Concept** section explaining *why* the piece exists and how it fits the whole, followed by a **Reference** section with the concrete details (files, pins, structs, functions).

---

## Chapter 1 — System Overview

### Concept

The project's one non-negotiable constraint shapes everything downstream: **the rocket launches from a remote site in Texas with no reliable internet access.** That rules out any design that depends on a live cloud connection during flight, and it's why the architecture is built as a chain of local, self-contained links rather than a single connected system.

The pipeline, as currently designed:

```
┌──────────────────┐   RFM69 radio    ┌───────────────────┐   USB serial    ┌──────────────┐
│  Flight Computer   │ ───────────────> │   Ground Station    │ ───────────────> │  Relay Script  │
│  (main/, ESP32-C61) │   915 MHz, no   │ (ground-station/,  │   NDJSON lines  │  (relay/, on   │
│                    │   ack/retry      │  plain ESP32)      │                 │   a laptop)    │
└──────────────────┘                  └───────────────────┘                 └──────────────┘
        │                                       │
        │ SD card (pending)                     │ Local WiFi AP + HTTP
        ▼                                       ▼
   local CSV log                        phone/laptop browser
   (resilience backstop)                (gs_server.ino, in progress)
```

Two independent "views" exist on purpose:
1. **Local, at the ground station** — anyone standing nearby can connect to the ground station's own WiFi AP and watch live numbers in a browser. No laptop or internet required. This is the primary way the club will watch a flight in Texas.
2. **Logged, on a laptop** — if a laptop is present, `relay.py` reads the ground station's serial output and writes an unconditional local log, so there's a full-resolution record independent of what anyone saw live.

A third piece, a cloud dashboard, was built but has since been **removed** (see [Chapter 5](#chapter-5--cloud-dashboard--removed)) because it solves a problem (remote viewing) that doesn't apply at this launch site.

### Reference — current state summary

| Subsystem | Directory | Status |
|---|---|---|
| Flight computer | `main/` | Sensor + server working; radio just enabled, untested; logger disabled (no SD card) |
| Ground station receiver | `ground-station/ground_station_rx/` | Firmware written, not yet flashed/tested |
| Ground station local server | `ground-station/ground_station_rx/gs_server.ino` | 4 of 5 known bugs fixed; one remaining (Ch. 3.4) |
| Relay script | `relay/relay.py` | Written, not yet run against real hardware |
| Cloud dashboard | ~~`cloud-dashboard/`~~ | Removed — no longer part of the architecture |

---

## Chapter 2 — Flight Computer (`main/`)

### Concept

This is the board that actually flies. It's built on the **ESP32-C61-DevKitC-1**, chosen for its RISC-V core and small form factor. Because the C61 isn't yet supported by the plain Arduino IDE toolchain, this board is built with **ESP-IDF** (via `idf.py`) rather than the Arduino IDE — that's the reason its build system, dependency management, and even file extension (`.cpp` instead of `.ino`) differ from the ground station.

The code follows an **orchestration pattern**: `main.cpp` owns `setup()`/`loop()` and timing, but delegates all actual hardware work to four independent modules (sensor, server, logger, radio). Each module owns one piece of hardware and exposes a small, deliberately narrow interface — `main.cpp` never touches I2C, SPI, or WiFi calls directly. This is what let the server be disabled/re-enabled and the radio be toggled on without touching any other module.

### Reference — module map

| Module | Files | Owns | Key functions |
|---|---|---|---|
| Sensor | `sensor.h/.cpp` | I2C bus, BMP390 | `initSensor()`, `readBmp()`, `printBmpReading()` |
| Server | `server.h/.cpp` | WiFi AP + HTTP | `initServer()`, `updateBMPReading()`, `handleClients()` |
| Logger | `logger.h/.cpp` | SD card, SPI | `initLogger()`, `logReading()`, `flushLogs()` |
| Radio | `radio.h/.cpp` | RFM69, SPI | `initRadio()`, `transmitReading()` |

Note that **logger and radio share the SPI bus** with different chip-select pins — this is why both modules exist as separate files but neither owns "SPI" outright the way sensor owns I2C.

### Reference — pin assignments (ESP32-C61-DevKitC-1)

| Signal | Pin | Used by |
|---|---|---|
| I2C SDA | GPIO 28 | Sensor (BMP390) |
| I2C SCL | GPIO 27 | Sensor (BMP390) |
| SPI SCK | GPIO 6 | Radio + Logger (shared bus) |
| SPI MOSI | GPIO 7 | Radio + Logger (shared bus) |
| SPI MISO | GPIO 2 | Radio + Logger (shared bus) |
| RFM69 CS | GPIO 5 | Radio (chip select) |
| RFM69 INT | GPIO 8 | Radio (DIO0/interrupt) |
| SD CS | GPIO 4 | Logger (chip select) |

All defined in `main/config.h`, which is intentionally the *only* file that should need editing for pin/credential changes.

### Reference — data structures

Three different structs exist for the same underlying reading, each shaped for a different consumer:

```cpp
// sensor.h — the sensor module's own internal representation
struct BmpReading {
    float pressure;      // hPa
    float temperatureC;
    float altitudeM;
    bool  valid;          // whether the I2C read actually succeeded
};

// radio.h — the wire format sent over RFM69 (exactly 16 bytes, packed, no `valid` flag —
// a packet is only ever sent when the reading was already validated)
struct __attribute__((packed)) RadioPacket {
    uint32_t timestamp_ms;
    float    temperatureC;
    float    pressureHpa;
    float    altitudeM;
};

// logger.h — one row of the SD card CSV
struct LogEntry {
    unsigned long timestamp;
    float temperatureC;
    float pressureHpa;
    float altitudeM;
};
```

`RadioPacket` is the most important of the three: it's a **cross-repo contract**. The ground station has its own independent copy of this exact struct (`ground-station/ground_station_rx/radio_packet.h`) that must stay byte-for-byte identical or the receiver will misparse incoming packets. See Chapter 3.2.

### Reference — current init/loop state

As of this writing, `main.cpp`'s `setup()`:

```cpp
initModule(initSensor, "sensor", "Check wiring and reset.", 1000);
initModule(initServer, "server", "Check WiFi credentials and reset.", 5000);
// initModule(initLogger, "logger", "Check SD card wiring and reset.", 1000);
initModule(initRadio,  "radio",  "Check wiring and reset.", 1000);
```

| Module | State | Why |
|---|---|---|
| Sensor | **Enabled** | Verified working — altitude, temperature, pressure all tested by hand |
| Server | **Enabled**, AP-only | STA mode (connecting out to a phone hotspot) is commented out in `server.cpp` — it was blocking `initServer()` on a WiFi timeout. AP mode alone works; browser reaches it at `http://192.168.4.1:8080` |
| Logger | **Disabled** | No SD card on hand yet. `logReading()`/`flushLogs()` calls in `loop()` are commented out to match |
| Radio | **Enabled** (just turned on) | Not yet tested end-to-end against the ground station |

`initModule()` itself is a small helper that turns "call an init function, print a labeled error, and halt on failure" into one line — it takes a function pointer, a module name, a hint string, and a retry-delay in ms, so each `setup()` line reads as one declarative statement instead of a repeated if/print/loop block.

### Reference — build system

The flight computer is built with ESP-IDF, targeting `esp32c61`. Dependencies are vendored as **git submodules** under `main/libraries/`, referenced with relative paths in `main/CMakeLists.txt` — this was a deliberate fix for the original code, which had hardcoded absolute Windows paths (`C:/Users/diazm/...`) that only worked on one machine.

| Submodule | Path | Upstream |
|---|---|---|
| RadioHead | `main/libraries/RadioHead` | PaulStoffregen/RadioHead |
| Adafruit BMP3XX | `main/libraries/Adafruit_BMP3XX_Library` | adafruit/Adafruit_BMP3XX |
| Adafruit BusIO | `main/libraries/Adafruit_BusIO` | adafruit/Adafruit_BusIO |
| Adafruit Unified Sensor | `main/libraries/Adafruit_Unified_Sensor` | adafruit/Adafruit_Sensor |

Build/flash/monitor: `idf.py build flash monitor -p COMx`

---

## Chapter 3 — Ground Station (`ground-station/ground_station_rx/`)

### Concept

This is a **second, separate board** — a plain ESP32 (not the C61), chosen specifically because it *is* supported by the ordinary Arduino IDE, unlike the flight computer. It sits on the ground during flight, listens for the flight computer's radio broadcasts, and re-exposes that data in two ways: as NDJSON lines over USB serial (for the relay script), and — once `gs_server.ino` is finished — as a local webpage anyone nearby can load.

It's a receive-only counterpart to the flight computer's radio module: same frequency, same modem configuration, no transmitting, no acknowledgment. The flight computer has no idea whether the ground station is listening at all — it just broadcasts every reading unconditionally.

### Reference — pin assignments (plain ESP32)

| Signal | Pin | Notes |
|---|---|---|
| RFM69 CS | GPIO 10 | Configurable in `config.h` |
| RFM69 INT | GPIO 9 | DIO0/interrupt, configurable in `config.h` |
| SPI (SCK/MOSI/MISO) | board defaults | Not explicitly pinned — uses the plain ESP32's default hardware SPI pins |

### Reference — the RadioPacket contract

`ground-station/ground_station_rx/radio_packet.h` duplicates `main/radio.h`'s struct exactly:

```cpp
struct __attribute__((packed)) RadioPacket {
  uint32_t timestamp_ms;
  float    temperatureC;
  float    pressureHpa;
  float    altitudeM;
};
static_assert(sizeof(RadioPacket) == 16, "...");
```

The `static_assert` catches a size mismatch at *compile time* on the ground station side, but it **cannot** catch a field being reordered or renamed in a way that keeps the size at 16 bytes — that would compile cleanly on both sides and silently corrupt every received value. This duplication is flagged as a refactor candidate in Chapter 6.

### Reference — receiver logic (`ground_station_rx.ino`)

RF init sequence (must match `main/radio.cpp` exactly, or packets won't demodulate):
```cpp
rf.init();
rf.setFrequency(915.0);
rf.setModemConfig(RH_RF69::GFSK_Rb4_8Fd9_6);
```

Output protocol: one NDJSON line per event over serial at 115200 baud, four line types distinguished by a `type` field:

| Type | Emitted when | Purpose |
|---|---|---|
| `boot` | Once, at startup | Confirms RF config actually applied |
| `telemetry` | Valid packet received | The actual reading, plus `seq`, `rx_ms`, and `rssi` |
| `error` | Packet received but wrong length | Distinguishes RF noise/corruption from a real reading |
| `heartbeat` | Every 5s of silence | Lets the relay script tell "radio quiet" apart from "board unplugged" |

`seq` is a ground-station-assigned counter (increments per valid packet, resets on reboot) — it's the ordering/dedup key used downstream.

### Reference — `gs_server.ino` (in progress, 1 known bug remaining)

**What it's trying to do:** mirror the flight computer's `server.cpp` — host a local WiFi AP with an HTTP page showing the latest reading — but fed by received radio packets instead of a local sensor.

**Bugs found during the original review — 4 of 5 now fixed:**

1. ~~`lastBmp` is declared as a `RadioPacket` initialized with 4 values (`{0.0f, 0.0f, 0.0f, false}`), but `RadioPacket` only has 3 fields and none of them are `bool`. Won't compile as-is.~~
2. ~~`#include "sensor.h"` — this header doesn't exist anywhere in `ground-station/`; it was carried over from copy-pasting `main/server.cpp`.~~
3. ~~The HTML formatting references `lastBmp.pressure` and omits altitude entirely — but `RadioPacket`'s actual fields are `pressureHpa`, `temperatureC`, and `altitudeM`.~~
4. ~~`HTTP_PORT` is used but not defined anywhere in `ground-station/ground_station_rx/config.h` (it only exists in the flight computer's `config.h`, a separate file).~~
5. **Still open — the important one:** nothing in `ground_station_rx.ino`'s receive loop ever calls anything to update `lastBmp`. The struct/field bugs are fixed, but the webpage will still always show the zero-initialized values until the receive loop's `if (rf.recv(...))` branch calls an update function (an `updateLastReading(*pkt)`-style function, mirroring `updateBMPReading()`'s role on the flight computer) each time a valid telemetry packet arrives.

The remaining fix is small — one function in `gs_server.ino`, one call added to `ground_station_rx.ino`'s receive branch — but it's the one that actually makes the page live.

---

## Chapter 4 — Relay Script (`relay/relay.py`)

### Concept

This script's entire design is driven by one assumption: **it cannot trust the network.** Whether that's because a laptop isn't at the field at all, or because the field has no signal, the script treats internet connectivity as an opportunistic bonus, never a requirement. Every line received from the ground station is written to a local file *immediately and unconditionally* — that local NDJSON log is the resilience backstop for the whole system. Anything network-related happens *after* that, on a background thread, and can fail silently without losing data.

This local-first design remains valuable even after removing the cloud dashboard (Chapter 5) — a laptop-side log is still a useful second copy of flight data alongside the ground station's own view and the flight computer's future SD card log.

### Reference

| Class | Responsibility |
|---|---|
| `RelayConfig` | CLI args / env vars: serial port, cloud ingest URL, API token, baud rate |
| `SerialReader` | Opens the serial port (auto-detects by device description, or `--port` override); reconnects automatically on unplug |
| `CloudRelay` | Writes every line to `relay/logs/<session_id>.ndjson`; if a cloud URL is configured, also queues telemetry records for a background POST worker with exponential backoff (2s → 60s cap) and an on-disk outbox for anything that fails |

**Open question for the refactor:** with the cloud dashboard removed, does `CloudRelay`'s POST-worker/outbox logic get deleted, or kept dormant (harmless if `--ingest-url` is never passed) in case a future non-remote use case wants it? See Chapter 6.

---

## Chapter 5 — Cloud Dashboard — REMOVED

### Concept

Originally: a Next.js app on Vercel with a Postgres backend, polled by a browser for live charts, fed by the relay script's background POST worker. It was designed under the assumption that *some* internet connectivity might be available near the launch site, even if unreliable — the whole relay design (local-first, cloud-opportunistic) was built to degrade gracefully if that connectivity never showed up.

**That assumption no longer holds.** The launch site is confirmed to be remote enough in Texas that a cloud solution isn't a realistic viewing method at all — not "unreliable," just not applicable. Rather than keep dead infrastructure around, the `cloud-dashboard/` directory has been deleted from the working tree (currently showing as pending deletions in `git status`, not yet committed).

### Reference — cleanup still needed (tracked in Chapter 6)

- `git status` shows `cloud-dashboard/*` as deleted but **uncommitted** — needs a commit once the refactor settles.
- `TELEMETRY.md` has since been **deleted entirely** as well (also uncommitted) — it no longer needs a cleanup pass because it's simply gone. Open decision: write a fresh, cloud-free integration guide once the refactor lands, or fold what's still relevant (ground station wiring, relay usage) into `README.md` instead.
- `relay.py`'s cloud-POST code path (see Chapter 4) is now unused by default but still present.

---

## Chapter 6 — Known Issues & Refactor Scope

Punch list, roughly in the order it makes sense to tackle:

1. **Wire `gs_server.ino` to the receiver loop** (Ch. 3.4) — the one remaining bug: add an update function and call it from `ground_station_rx.ino`'s receive branch so the webpage reflects real packets instead of zeros.
2. **Decide on `RadioPacket` duplication** (Ch. 3.2) — two hand-synced copies of the same struct is a silent-corruption risk. Both sides now carry a `static_assert(sizeof==16)` guard, but that still can't catch a field reorder. Options: a shared header included by both builds (awkward across ESP-IDF vs Arduino toolchains), a code-gen step, or a stronger guard (e.g., a version/magic byte in the packet itself).
3. **Decide the fate of `CloudRelay` in `relay.py`** (Ch. 4) — delete, or keep dormant.
4. **Commit the `cloud-dashboard/` and `TELEMETRY.md` deletions** (Ch. 5) once the above is settled — both currently sit uncommitted in `git status`.
5. **Write a replacement for `TELEMETRY.md`** (Ch. 5) — it's gone now, not just outdated. Decide: fresh cloud-free integration guide, or fold into `README.md`.
6. **Test the radio path end-to-end** — flight computer → ground station — now that both have radio code enabled (this was the immediate next step before this doc was requested).
7. **Get an SD card and re-enable the logger** on the flight computer.
8. **Revisit `server.cpp` vs `gs_server.ino` duplication** — both are near-identical HTML/CSS for a stat-display page. Not urgent, but worth a look once both are working: could a shared snippet or template reduce the copy-paste, given the two boards build with different toolchains (ESP-IDF vs Arduino IDE)?

---

## Appendix A — Full Pin Reference

| Signal | Flight computer (C61) | Ground station (ESP32) |
|---|---|---|
| I2C SDA | GPIO 28 | — (no sensor) |
| I2C SCL | GPIO 27 | — (no sensor) |
| SPI SCK | GPIO 6 | board default |
| SPI MOSI | GPIO 7 | board default |
| SPI MISO | GPIO 2 | board default |
| Radio CS | GPIO 5 | GPIO 10 |
| Radio INT | GPIO 8 | GPIO 9 |
| SD CS | GPIO 4 | — (no SD card) |

## Appendix B — Repo Map (telemetry-relevant paths)

```
Georges_RocketSensor/
├── main/                              # Flight computer (ESP-IDF, ESP32-C61)
│   ├── main.cpp
│   ├── config.h
│   ├── sensor.h/.cpp
│   ├── server.h/.cpp
│   ├── logger.h/.cpp
│   ├── radio.h/.cpp
│   ├── CMakeLists.txt
│   └── libraries/                     # git submodules
├── ground-station/
│   └── ground_station_rx/             # Ground station (Arduino IDE, plain ESP32)
│       ├── ground_station_rx.ino
│       ├── gs_server.ino              # in progress, see Ch. 3.4
│       ├── config.h
│       └── radio_packet.h
├── relay/
│   ├── relay.py
│   ├── requirements.txt
│   └── logs/                          # gitignored
├── cloud-dashboard/                   # REMOVED, see Ch. 5
├── TELEMETRY.md                       # REMOVED, see Ch. 5/6 (needs a replacement)
└── docs/
    └── DEVLOG.md                      # this file
```
