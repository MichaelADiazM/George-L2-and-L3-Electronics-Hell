# UML — Georges Rocket Sensor

Companion to [`DEVLOG.md`](DEVLOG.md). That document explains *why* each piece exists; this one shows the *shape* of the code — what the classes/modules are, what data flows between them, and (in the last diagram) what a live system actually looks like at one instant.

## Notation

Most of this codebase is C/C++ written as **free functions + file-scope statics**, not real classes — `main.cpp`'s `initSensor()` and `sensor.cpp`'s static `Adafruit_BMP3XX bmp` are a "module," not an object. To draw a useful class diagram anyway, each `.cpp`/`.h` pair (or `.ino` file) is modeled as one UML class: its free functions become methods, its file-scope `static` variables become private attributes. This is a standard technique for reverse-engineering procedural code into UML — it's not claiming these are literally C++ classes.

The one subsystem with real classes is `relay/relay.py` (genuine Python OOP) — that diagram is a normal, unmodified class diagram.

Stereotypes used below:
- `<<library>>` — external dependency (RadioHead, ESP32 WebServer, Adafruit libraries), not code in this repo
- `<<packed, 16 bytes>>` — a wire-format struct with `__attribute__((packed))`
- `<<instance>>` — object-diagram notation: a concrete object at runtime, not a type

Relationship key: `-->` calls/uses, `..>` dependency (produces or consumes a value), `*--` composition (owns an instance for its whole lifetime).

Diagrams use Mermaid's default theme with no custom fill colors, so they render correctly in both GitHub's light and dark modes (an earlier hand-drawn diagram in this repo broke under dark mode specifically because of custom `style` fills — see `docs/wiring-diagram.svg` for why that one is a plain SVG instead).

---

## 1. Class Diagram — Flight Computer (`main/`)

```mermaid
classDiagram
    class Main {
        <<orchestrator, main.cpp>>
        -lastBMPReadAt : unsigned long
        -lastLogFlushAt : unsigned long
        -readCount : int
        +setup()
        +loop()
        +initModule(initFunc, module, hint, delayMs)
    }

    class Sensor {
        <<sensor.h/.cpp>>
        +initSensor() bool
        +readBmp() BmpReading
        +printBmpReading(r)
    }

    class Server {
        <<server.h/.cpp>>
        -lastBmp : BmpReading
        -server : WebServer
        +initServer() bool
        +updateBMPReading(reading)
        +handleClients()
        -handleRoot()
    }

    class Logger {
        <<logger.h/.cpp — currently disabled>>
        -logBuffer : LogEntry[25]
        -bufferCount : int
        -logFile : File
        +initLogger() bool
        +logReading(timestamp, reading)
        +flushLogs()
    }

    class Radio {
        <<radio.h/.cpp>>
        -rf : RH_RF69
        +initRadio() bool
        +transmitReading(packet)
    }

    class BmpReading {
        +pressure : float
        +temperatureC : float
        +altitudeM : float
        +valid : bool
    }

    class RadioPacket {
        <<packed, 16 bytes>>
        +timestamp_ms : uint32_t
        +temperatureC : float
        +pressureHpa : float
        +altitudeM : float
    }

    class LogEntry {
        +timestamp : unsigned long
        +temperatureC : float
        +pressureHpa : float
        +altitudeM : float
    }

    class RH_RF69 { <<library: RadioHead>> }
    class WebServer { <<library: ESP32 WebServer>> }
    class Adafruit_BMP3XX { <<library: Adafruit>> }
    class File { <<library: SD>> }

    Main --> Sensor : calls
    Main --> Server : calls
    Main --> Logger : calls (disabled)
    Main --> Radio : calls
    Main ..> RadioPacket : constructs in loop()

    Sensor ..> BmpReading : returns
    Sensor *-- Adafruit_BMP3XX : owns

    Server ..> BmpReading : caches as lastBmp
    Server *-- WebServer : owns

    Logger ..> BmpReading : reads from
    Logger ..> LogEntry : buffers
    Logger *-- File : owns

    Radio ..> RadioPacket : transmits
    Radio *-- RH_RF69 : owns
```

**Reading this diagram alongside the code:** `Main` never touches `RH_RF69`, `WebServer`, or `Adafruit_BMP3XX` directly — every hardware call goes through exactly one module. That's why disabling the radio or the logger in `main.cpp` (commenting out one `initModule(...)` line) doesn't risk breaking anything else; no other module holds a reference to it.

---

## 2. Class Diagram — Ground Station (`ground-station/ground_station_rx/`)

```mermaid
classDiagram
    class GroundStationReceiver {
        <<ground_station_rx.ino>>
        -packet_seq : uint16_t
        -last_heartbeat_ms : unsigned long
        -HEARTBEAT_INTERVAL_MS : const unsigned long
        +setup()
        +loop()
    }

    class GroundStationServer {
        <<gs_server.ino — in progress>>
        -lastBmp : RadioPacket
        -server : WebServer
        +initServer() bool
        -handleRoot()
    }

    class RadioPacket {
        <<packed, 16 bytes>>
        <<duplicate of main/radio.h — must stay in sync>>
        +timestamp_ms : uint32_t
        +temperatureC : float
        +pressureHpa : float
        +altitudeM : float
    }

    class RH_RF69 { <<library: RadioHead>> }
    class WebServer { <<library: ESP32 WebServer>> }

    GroundStationReceiver *-- RH_RF69 : owns
    GroundStationReceiver ..> RadioPacket : parses from radio buffer
    GroundStationServer *-- WebServer : owns
    GroundStationServer ..> RadioPacket : displays as lastBmp

    note for GroundStationReceiver "Known gap (DEVLOG.md Ch. 3.4, item 5):
    on a valid packet, loop() never calls
    anything on GroundStationServer to
    update lastBmp. The dotted line that
    SHOULD exist between these two classes
    — receiver notifies server — doesn't
    exist in the code yet."
```

**This diagram doubles as a bug map.** `GroundStationReceiver` and `GroundStationServer` are drawn with no relationship between them, and that's accurate — nothing in the current code connects them. That missing edge *is* the one open bug from `DEVLOG.md` Chapter 3.4: fixing it means adding exactly one call (receiver → server, "here's a new reading") and drawing that edge in for real.

---

## 3. Class Diagram — Relay Script (`relay/relay.py`)

This one needs no procedural-to-OOP translation — it's genuine Python classes.

```mermaid
classDiagram
    class Main {
        <<module-level main() function>>
        +main()
    }

    class RelayConfig {
        -args
        +port : str
        +ingest_url : str
        +api_token : str
        +baud : int
        +__init__()
    }

    class SerialReader {
        -config : RelayConfig
        -ser
        -port : str
        +__init__(config)
        +find_and_open() bool
        +readline_with_retry() str
        +close()
    }

    class CloudRelay {
        -config : RelayConfig
        -session_id : str
        -session_file : Path
        -outbox : Queue
        -retry_backoff : float
        -max_backoff : float
        -last_successful_post_time : float
        -post_worker_thread : Thread
        +__init__(config)
        +log_line(line)
        -_post_worker()
        +flush_outbox_on_shutdown()
    }

    Main --> RelayConfig : creates
    Main --> SerialReader : creates
    Main --> CloudRelay : creates
    Main ..> SerialReader : reads lines from
    Main ..> CloudRelay : forwards lines to (log_line)
    SerialReader --> RelayConfig : reads
    CloudRelay --> RelayConfig : reads
```

Note: `RelayConfig.port` / `.ingest_url` / `.api_token` / `.baud` are technically Python `@property` getters over `self.args`, not stored fields — shown here as attributes because that's how every caller uses them (`config.port`), which is the more useful view for understanding data flow.

`CloudRelay` has no direct reference to `SerialReader` in the code — `main()` is the only thing that wires a line from one to the other. That's intentional: it keeps the "read from serial" and "log/relay" concerns decoupled enough that either could be tested independently.

---

## 4. Object Diagram — Runtime Snapshot

Mermaid has no dedicated object-diagram syntax, so this reuses `classDiagram` with concrete instance names and a `<<instance>>` stereotype — the standard workaround. Unlike the diagrams above (which describe *types*, valid at all times), this one is a snapshot: the actual objects and values that exist at one moment during a test flight, after packet #42 has just been sent and received.

```mermaid
classDiagram
    class rf_flight["rf : RH_RF69 (flight computer)"] {
        <<instance>>
        csPin = 5
        intPin = 8
        frequencyMHz = 915.0
        modemConfig = GFSK_Rb4_8Fd9_6
        txPower = 20
    }

    class rf_ground["rf : RH_RF69 (ground station)"] {
        <<instance>>
        csPin = 10
        intPin = 9
        frequencyMHz = 915.0
        modemConfig = GFSK_Rb4_8Fd9_6
    }

    class bmp_instance["bmp : Adafruit_BMP3XX"] {
        <<instance>>
        i2cSda = 28
        i2cScl = 27
        tempOversampling = 8X
        pressOversampling = 4X
        outputDataRate = 25Hz
    }

    class server_flight["server : WebServer (flight computer)"] {
        <<instance>>
        port = 8080
        apSsid = "rocketfinder"
        mode = AP-only, STA path disabled
    }

    class server_ground["server : WebServer (ground station)"] {
        <<instance>>
        port = 8080
    }

    class packet42["packet42 : RadioPacket"] {
        <<instance, snapshot>>
        timestamp_ms = 128340
        temperatureC = 22.53
        pressureHpa = 1013.25
        altitudeM = 125.02
    }

    bmp_instance ..> packet42 : reading sourced from
    rf_flight ..> packet42 : transmits
    rf_ground ..> packet42 : receives, rssi = -67
    server_flight ..> bmp_instance : displays latest reading from
    server_ground ..> packet42 : SHOULD display, doesn't yet
```

The two `rf` instances are the same *class* (`RH_RF69`) with deliberately different pin configurations — this is the concrete version of the pin table in `DEVLOG.md` Appendix A. `server_ground`'s dashed relationship to `packet42` is marked "should display, doesn't yet" for the same reason as the missing edge in Diagram 2: it's the same bug, shown at the instance level instead of the type level.

---

## Keeping this in sync

These diagrams were built by reading `main/*.{h,cpp}`, `ground-station/ground_station_rx/*.{ino,h}`, and `relay/relay.py` directly as of this writing. If the refactor tracked in `DEVLOG.md` Chapter 6 changes any of those files' structure (new modules, merged responsibilities, the `RadioPacket` duplication fix, etc.), regenerate the affected diagram rather than hand-patching it — procedural-to-UML translation is easy to get subtly wrong by editing text without re-reading the source.
