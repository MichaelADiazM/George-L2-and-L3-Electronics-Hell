/**
 * app/api/readings/route.ts
 * --------
 * Fetch readings for a specific session, optionally since a given seq.
 * Used by the frontend for polling live telemetry.
 *
 * GET /api/readings?session_id=<uuid>&since_seq=1234
 *
 * Response:
 * 200 OK
 * [
 *   {"id": 1, "session_id": "...", "seq": 1, "rx_ms": 100, "t_ms": 50, "temp_c": 22.5, "press_hpa": 1013.25, "alt_m": 125.0, "rssi": -67, "received_at_utc": "...", "inserted_at": "..."},
 *   ...
 * ]
 */

import { NextRequest, NextResponse } from 'next/server';
import { getReadings, initDatabase } from '@/lib/db';

export async function GET(request: NextRequest) {
  try {
    await initDatabase();

    const { searchParams } = new URL(request.url);
    const sessionId = searchParams.get('session_id');
    const sinceSeq = searchParams.get('since_seq');

    if (!sessionId) {
      return NextResponse.json(
        { error: 'session_id parameter required' },
        { status: 400 }
      );
    }

    const readings = await getReadings(
      sessionId,
      sinceSeq ? parseInt(sinceSeq) : undefined
    );

    return NextResponse.json(readings, { status: 200 });
  } catch (error) {
    console.error('Readings fetch error:', error);
    return NextResponse.json(
      { error: 'Internal server error' },
      { status: 500 }
    );
  }
}
