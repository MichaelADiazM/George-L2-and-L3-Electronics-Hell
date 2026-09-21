# Real-Time Telemetry: Ground Station + Cloud Dashboard

This monorepo now includes a complete pipeline for real-time in-flight telemetry viewing: the flight computer transmits sensor readings via RFM69 radio, a second ESP32 receives them and outputs NDJSON over serial, a laptop relay script logs everything locally and relays to the cloud opportunistically, and a Vercel-hosted Next.js dashboard displays live flight data in a browser.

## Quick start

### 1. Build and flash the ground station
The ground station is a separate Arduino IDE sketch that receives RFM69 packets and outputs NDJSON over serial.

**Hardware:**
- A plain ESP32 dev board (Arduino IDE supported, unlike the C61)
- An RFM69HCW breakout module (same as the flight computer uses)
- Wiring: see `ground-station/ground_station_rx/config.h` for pin assignments

**Build:**
1. Open `ground-station/ground_station_rx/ground_station_rx.ino` in Arduino IDE
2. Install RadioHead library via Library Manager (search "RadioHead")
3. Select your ESP32 board and COM port
4. Upload

**Test:** Open Serial Monitor at 115200 baud. Once the ground station boots, you should see a `{"type":"boot",...}` line. When the flight computer transmits nearby, you'll see `{"type":"telemetry",...}` lines.

### 2. Run the relay script on your laptop

**Install:**
```bash
cd relay
pip install -r requirements.txt
cp .env.example .env
# Edit .env and fill in CLOUD_INGEST_URL and CLOUD_API_TOKEN (or leave blank for local-only logging)
```

**Run:**
```bash
python relay.py --port COM3  # Replace COM3 with your ground station's port
```

The script will:
- Auto-discover your ground station's serial port (or accept `--port COM3` override)
- Log every packet to `relay/logs/<session-id>.ndjson` — your authoritative flight record
- Relay to the cloud endpoint opportunistically (batching, backfill on reconnection, non-blocking)

### 3. Deploy the cloud dashboard to Vercel

**Setup:**
```bash
cd cloud-dashboard
npm install
# Create a Postgres database (Neon or Supabase, both integrate with Vercel)
# Set DATABASE_URL and INGEST_API_TOKEN in a .env.local
npm run dev  # Test locally at http://localhost:3000
```

**Deploy to Vercel:**
1. Push this repo to GitHub
2. In Vercel, create a new project pointing at the `cloud-dashboard/` folder (set it as "Root Directory")
3. Add environment variables: `DATABASE_URL` and `INGEST_API_TOKEN` (should match relay.py's token)
4. Deploy

Once live, the dashboard polls `/api/readings` every 1-2 seconds for live telemetry. Visit `https://your-dashboard.vercel.app/?session_id=<uuid>` to view a flight.

## Architecture & resilience

- **Ground station firmware** (`ground-station/`): plain Arduino sketch, receives RFM69 packets, outputs NDJSON over serial
- **Relay script** (`relay/`): Python + pyserial, reads serial, logs locally unconditionally, relays to cloud non-blocking
- **Cloud dashboard** (`cloud-dashboard/`): Next.js on Vercel, polls `/api/readings` for live display, stores in Postgres with deduplication

**Resilience by design:**
- Every packet is logged locally first (`relay/logs/<session>.ndjson`), even if there's no internet. This is the authoritative flight record.
- Cloud relay is opportunistic: if the ground station loses signal or the laptop loses internet, the relay script queues failed POSTs and retries with exponential backoff. Once connectivity returns, everything uploads automatically.
- The dashboard is read-only and best-effort — it's for live monitoring convenience, not the primary data store.

## Testing

**Bench-test before launch:**
1. Use RadioHead's vendored example (`main/libraries/RadioHead/examples/rf69/rf69_client`) reconfigured to `GFSK_Rb4_8Fd9_6` to send packets to the ground station
2. Verify the relay script logs and relays to the cloud (test with `curl` against the ingest API)
3. Toggle the laptop's internet connection mid-run to verify local logging + outbox backfill

**Full dry-run on test day:**
- Flight computer transmitting → ground station receiving → relay script logging locally → cloud dashboard displaying
- Specifically test: unplug the ground station and replug, to confirm the relay script reconnects cleanly
- Simulate lost internet for the whole flight, then restore connectivity, to confirm backfill uploads everything

## Configuration

**Ground station pins** (`ground-station/ground_station_rx/config.h`):
- `RFM_CS_PIN`: Chip Select (default GPIO 10)
- `RFM_INT_PIN`: Interrupt / G0 (default GPIO 9)
- `RFM_FREQUENCY_MHZ`: 915.0 (must match flight computer)

**Relay script** (`relay/.env` or CLI args):
- `SERIAL_PORT`: COM port of the ground station (auto-detected or specify)
- `CLOUD_INGEST_URL`: cloud ingest endpoint (can be blank for local-only logging)
- `CLOUD_API_TOKEN`: bearer token (shared between relay and cloud)

**Cloud dashboard** (`cloud-dashboard/.env.local`):
- `DATABASE_URL`: Postgres connection string
- `INGEST_API_TOKEN`: must match relay's token

## Known limitations

1. **Serial port reconnection on Windows:** If you unplug the ground station and plug it back into a *different* physical USB port, Windows may reassign the COM number. The relay script will detect this and attempt to reconnect, but on very old Windows systems you may need to specify `--port` explicitly.
2. **No encryption:** Data in transit (serial NDJSON) and over the internet (HTTP to cloud) is not encrypted. For a college rocketry club, this is acceptable, but for mission-critical scenarios, add TLS to the relay → cloud connection.
3. **Vercel serverless limits:** a Vercel function has a max runtime of ~60 seconds per invocation. For very long flights (>30 min of continuous data) with slow poll rates, the dashboard may hit limits. For longer flights, upgrade to Docker + Fly.io/Railway with a persistent WebSocket server.

## Future enhancements

- **True push-based WebSocket updates** instead of polling: deploy to Fly.io/Railway with a Node.js server instead of Vercel serverless
- **Encryption**: add TLS to relay → cloud, sign API tokens cryptographically
- **Multi-session comparison**: add a page to overlay multiple flights' altitude curves
- **Local network mode**: optionally relay to a local network server at the launch field instead of cloud, for scenarios with no internet connectivity
