/**
 * app/api/ingest/route.ts
 * --------
 * Cloud ingest endpoint: receives NDJSON telemetry from the relay script,
 * authenticates via Bearer token, and stores in Postgres with deduplication.
 *
 * POST /api/ingest
 * Authorization: Bearer <token>
 * Content-Type: application/json
 *
 * {
 *   "session_id": "<uuid>",
 *   "readings": [
 *     {"seq": 1, "rx_ms": 100, "t_ms": 50, "temp_c": 22.5, "press_hpa": 1013.25, "alt_m": 125.0, "rssi": -67, "received_at_utc": "2026-09-17T18:04:12.345Z"},
 *     ...
 *   ]
 * }
 *
 * Response:
 * 200 OK
 * {"accepted": 5}
 */

import { NextRequest, NextResponse } from 'next/server';
import { initDatabase, upsertReadings, upsertSession } from '@/lib/db';

const API_TOKEN = process.env.INGEST_API_TOKEN || 'change-me-in-production';

export async function POST(request: NextRequest) {
  // Authenticate via Bearer token
  const authHeader = request.headers.get('authorization') || '';
  const token = authHeader.replace(/^Bearer\s+/i, '');

  if (token !== API_TOKEN) {
    return NextResponse.json(
      { error: 'Unauthorized' },
      { status: 401 }
    );
  }

  try {
    // Initialize DB tables if needed
    await initDatabase();

    const body = await request.json();
    const { session_id, readings } = body;

    if (!session_id || !Array.isArray(readings)) {
      return NextResponse.json(
        { error: 'Invalid request: session_id and readings array required' },
        { status: 400 }
      );
    }

    // Ensure session exists
    await upsertSession(session_id);

    // Upsert readings (ON CONFLICT DO NOTHING handles duplicates)
    const accepted = await upsertReadings(
      session_id,
      readings.map((r: any) => ({
        seq: r.seq,
        rx_ms: BigInt(r.rx_ms || 0),
        t_ms: BigInt(r.t_ms || 0),
        temp_c: parseFloat(r.tempC || r.temp_c || 0),
        press_hpa: parseFloat(r.pressHpa || r.press_hpa || 0),
        alt_m: parseFloat(r.altM || r.alt_m || 0),
        rssi: r.rssi ? parseInt(r.rssi) : undefined,
        received_at_utc: r.received_at_utc || new Date().toISOString(),
      }))
    );

    return NextResponse.json({ accepted }, { status: 200 });
  } catch (error) {
    console.error('Ingest error:', error);
    return NextResponse.json(
      { error: 'Internal server error' },
      { status: 500 }
    );
  }
}
