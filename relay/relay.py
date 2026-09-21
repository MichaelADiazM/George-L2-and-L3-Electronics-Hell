#!/usr/bin/env python3
"""
relay.py
--------
Laptop-side script: reads NDJSON telemetry from the ground station's serial port,
logs every packet locally (resilience backstop), and relays to cloud ingest endpoint
opportunistically (non-blocking, with outbox backfill on reconnection).

Usage:
  python relay.py --port COM3 --ingest-url https://... --api-token secret

Or set environment variables:
  SERIAL_PORT, CLOUD_INGEST_URL, CLOUD_API_TOKEN

Local logs go to relay/logs/<session_id>.ndjson (one JSON object per line).
Failed outbox goes to relay/logs/outbox.ndjson and retries with exponential backoff.
"""

import sys
import json
import os
import time
import uuid
import argparse
from pathlib import Path
from datetime import datetime, timezone
import threading
import queue
import logging
from typing import Optional, Dict, Any
import traceback

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print("ERROR: pyserial not installed. Run: pip install -r requirements.txt")
    sys.exit(1)

try:
    import requests
except ImportError:
    print("ERROR: requests not installed. Run: pip install -r requirements.txt")
    sys.exit(1)

# Setup logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    handlers=[logging.StreamHandler()]
)
logger = logging.getLogger(__name__)

# Paths
LOG_DIR = Path(__file__).parent / "logs"
LOG_DIR.mkdir(exist_ok=True)
OUTBOX_FILE = LOG_DIR / "outbox.ndjson"
ERROR_LOG_FILE = LOG_DIR / "relay_errors.log"

class RelayConfig:
    """Configuration loaded from CLI args and environment."""
    def __init__(self):
        parser = argparse.ArgumentParser(description=__doc__)
        parser.add_argument(
            '--port',
            default=os.getenv('SERIAL_PORT'),
            help='Serial port (e.g., COM3). Default: auto-detect. Env: SERIAL_PORT'
        )
        parser.add_argument(
            '--ingest-url',
            default=os.getenv('CLOUD_INGEST_URL'),
            help='Cloud ingest API endpoint. Env: CLOUD_INGEST_URL'
        )
        parser.add_argument(
            '--api-token',
            default=os.getenv('CLOUD_API_TOKEN'),
            help='Bearer token for cloud API. Env: CLOUD_API_TOKEN'
        )
        parser.add_argument(
            '--baud',
            type=int,
            default=115200,
            help='Serial baud rate (default: 115200)'
        )
        self.args = parser.parse_args()

    @property
    def port(self) -> str:
        return self.args.port

    @property
    def ingest_url(self) -> Optional[str]:
        return self.args.ingest_url

    @property
    def api_token(self) -> Optional[str]:
        return self.args.api_token

    @property
    def baud(self) -> int:
        return self.args.baud


class SerialReader:
    """Manages serial connection with auto-reconnect on unplug/replug."""

    def __init__(self, config: RelayConfig):
        self.config = config
        self.ser = None
        self.port = None

    def find_and_open(self) -> bool:
        """Auto-discover serial port by device description, or use --port override."""
        if self.config.port:
            self.port = self.config.port
            logger.info(f"Using explicit port: {self.port}")
        else:
            # Auto-detect: look for common USB serial device descriptions
            found = False
            for info in list_ports.comports():
                if any(desc in info.description for desc in ['CP210', 'CH340', 'USB Serial', 'UART']):
                    self.port = info.device
                    logger.info(f"Auto-detected port {self.port}: {info.description}")
                    found = True
                    break

            if not found:
                logger.error("No USB serial device found. Use --port to specify manually.")
                return False

        try:
            self.ser = serial.Serial(self.port, self.config.baud, timeout=1.0)
            logger.info(f"Opened {self.port} at {self.config.baud} baud")
            return True
        except Exception as e:
            logger.error(f"Failed to open {self.port}: {e}")
            return False

    def readline_with_retry(self) -> Optional[str]:
        """Read a line from serial, with auto-reconnect on failure."""
        while True:
            try:
                if self.ser is None:
                    if not self.find_and_open():
                        time.sleep(2)
                        continue

                line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                if line:
                    return line
                # No data yet, loop to retry
                time.sleep(0.01)

            except Exception as e:
                logger.warning(f"Serial read error: {e}. Reconnecting...")
                if self.ser:
                    try:
                        self.ser.close()
                    except:
                        pass
                self.ser = None
                time.sleep(2)

    def close(self):
        if self.ser:
            try:
                self.ser.close()
            except:
                pass


class CloudRelay:
    """Handles cloud relay: local logging, opportunistic POSTs, outbox backfill."""

    def __init__(self, config: RelayConfig):
        self.config = config
        self.session_id = str(uuid.uuid4())
        self.session_file = LOG_DIR / f"{self.session_id}.ndjson"
        self.outbox = queue.Queue()  # For background POST worker
        self.retry_backoff = 2.0  # Start at 2 seconds
        self.max_backoff = 60.0
        self.last_successful_post_time = time.time()  # Track when we last had internet

        # Load any previous outbox
        if OUTBOX_FILE.exists():
            logger.info("Loading previous outbox...")
            try:
                with open(OUTBOX_FILE, 'r') as f:
                    for line in f:
                        line = line.strip()
                        if line:
                            try:
                                record = json.loads(line)
                                self.outbox.put(record)
                            except json.JSONDecodeError:
                                pass
            except Exception as e:
                logger.error(f"Failed to load outbox: {e}")

        # Start background POST worker if cloud is configured
        if self.config.ingest_url:
            self.post_worker_thread = threading.Thread(target=self._post_worker, daemon=True)
            self.post_worker_thread.start()
            logger.info(f"Cloud relay enabled: {self.config.ingest_url}")
        else:
            logger.info("Cloud relay disabled (no --ingest-url provided). Local logging only.")

    def log_line(self, line: str):
        """Parse and log a line from ground station; enqueue for cloud if applicable."""
        try:
            record = json.loads(line)
        except json.JSONDecodeError:
            # Malformed JSON: log error, do not crash
            with open(ERROR_LOG_FILE, 'a') as f:
                f.write(f"{datetime.now(timezone.utc).isoformat()} PARSE_ERROR: {line}\n")
            return

        # Always log locally first (resilience backstop)
        record['received_at_utc'] = datetime.now(timezone.utc).isoformat()
        with open(self.session_file, 'a') as f:
            f.write(json.dumps(record) + '\n')
            f.flush()

        # Enqueue for cloud relay if applicable and if it's a telemetry record
        if self.config.ingest_url and record.get('type') == 'telemetry':
            # Throttle: only queue every N-th telemetry for "live" send (keep full resolution in local log)
            # For now, queue all; relay script can throttle if needed
            self.outbox.put(record)

    def _post_worker(self):
        """Background thread: POSTs queued records to cloud with backoff."""
        while True:
            try:
                # Batch up to 100 records for efficiency
                batch = []
                try:
                    while len(batch) < 100:
                        record = self.outbox.get(timeout=5.0)
                        batch.append(record)
                except queue.Empty:
                    pass

                if not batch:
                    continue

                # POST to cloud
                headers = {
                    'Authorization': f'Bearer {self.config.api_token}',
                    'Content-Type': 'application/json'
                }
                payload = {
                    'session_id': self.session_id,
                    'readings': batch
                }

                try:
                    resp = requests.post(
                        self.config.ingest_url,
                        json=payload,
                        headers=headers,
                        timeout=5.0
                    )

                    if resp.status_code == 200:
                        accepted = resp.json().get('accepted', 0)
                        logger.info(f"Cloud POST OK: {accepted} records accepted")
                        self.retry_backoff = 2.0  # Reset backoff on success
                        self.last_successful_post_time = time.time()

                        # Clear outbox file since these were sent
                        try:
                            OUTBOX_FILE.unlink(missing_ok=True)
                        except:
                            pass
                    else:
                        # Failed: re-queue the batch for retry
                        logger.warning(f"Cloud POST failed: {resp.status_code}. Retrying...")
                        for record in reversed(batch):  # Re-queue in order
                            self.outbox.put(record)
                        time.sleep(self.retry_backoff)
                        self.retry_backoff = min(self.retry_backoff * 1.5, self.max_backoff)

                except requests.RequestException as e:
                    logger.warning(f"Cloud connection failed: {e}. Retrying...")
                    for record in reversed(batch):
                        self.outbox.put(record)
                    time.sleep(self.retry_backoff)
                    self.retry_backoff = min(self.retry_backoff * 1.5, self.max_backoff)

            except Exception as e:
                logger.error(f"POST worker error: {e}")
                traceback.print_exc()
                time.sleep(5)

    def flush_outbox_on_shutdown(self):
        """On exit, save any remaining queued items to disk."""
        if self.outbox.empty():
            return

        with open(OUTBOX_FILE, 'w') as f:
            while not self.outbox.empty():
                try:
                    record = self.outbox.get_nowait()
                    f.write(json.dumps(record) + '\n')
                except queue.Empty:
                    break
        logger.info(f"Saved {OUTBOX_FILE.stat().st_size} bytes of pending records to outbox for next run")


def main():
    """Main loop: read from serial, log, relay."""
    config = RelayConfig()
    reader = SerialReader(config)
    relay = CloudRelay(config)

    logger.info(f"Session ID: {relay.session_id}")
    logger.info(f"Local logs: {relay.session_file}")
    logger.info("Starting telemetry receive loop (Ctrl+C to exit)...")

    try:
        while True:
            line = reader.readline_with_retry()
            if line:
                relay.log_line(line)
                # Also print to console for live monitoring
                print(line)

    except KeyboardInterrupt:
        logger.info("Shutting down...")
        relay.flush_outbox_on_shutdown()
        reader.close()
        logger.info("Goodbye.")
        sys.exit(0)

    except Exception as e:
        logger.error(f"Unexpected error: {e}")
        traceback.print_exc()
        relay.flush_outbox_on_shutdown()
        reader.close()
        sys.exit(1)


if __name__ == '__main__':
    main()
