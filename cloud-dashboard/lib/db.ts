/**
 * lib/db.ts
 * --------
 * PostgreSQL connection pool and query helpers.
 * Uses Vercel's Postgres integration or a standard pg connection string via DATABASE_URL.
 */

import { Pool, QueryResult } from 'pg';

let pool: Pool | null = null;

export function getPool(): Pool {
  if (!pool) {
    const dbUrl = process.env.DATABASE_URL;
    if (!dbUrl) {
      throw new Error('DATABASE_URL environment variable is not set');
    }
    pool = new Pool({ connectionString: dbUrl });
  }
  return pool;
}

export async function initDatabase(): Promise<void> {
  const client = await getPool().connect();
  try {
    await client.query(`
      CREATE TABLE IF NOT EXISTS readings (
        id BIGSERIAL PRIMARY KEY,
        session_id UUID NOT NULL,
        seq INTEGER NOT NULL,
        rx_ms BIGINT NOT NULL,
        t_ms BIGINT NOT NULL,
        temp_c REAL NOT NULL,
        press_hpa REAL NOT NULL,
        alt_m REAL NOT NULL,
        rssi SMALLINT,
        received_at_utc TIMESTAMPTZ NOT NULL,
        inserted_at TIMESTAMPTZ NOT NULL DEFAULT now(),
        UNIQUE (session_id, seq)
      );

      CREATE INDEX IF NOT EXISTS idx_readings_session_time
        ON readings (session_id, received_at_utc DESC);

      CREATE TABLE IF NOT EXISTS sessions (
        session_id UUID PRIMARY KEY,
        label TEXT,
        started_at TIMESTAMPTZ NOT NULL DEFAULT now()
      );
    `);
  } finally {
    client.release();
  }
}

export async function query(
  text: string,
  params?: any[]
): Promise<QueryResult> {
  const client = await getPool().connect();
  try {
    return await client.query(text, params);
  } finally {
    client.release();
  }
}

export interface Reading {
  id: bigint;
  session_id: string;
  seq: number;
  rx_ms: bigint;
  t_ms: bigint;
  temp_c: number;
  press_hpa: number;
  alt_m: number;
  rssi: number | null;
  received_at_utc: string;
  inserted_at: string;
}

export interface Session {
  session_id: string;
  label: string | null;
  started_at: string;
}

export async function getReadings(
  sessionId: string,
  sinceSeq?: number
): Promise<Reading[]> {
  const params: any[] = [sessionId];
  let whereClause = 'session_id = $1';

  if (sinceSeq !== undefined) {
    whereClause += ` AND seq > $${params.length + 1}`;
    params.push(sinceSeq);
  }

  const result = await query(
    `SELECT * FROM readings WHERE ${whereClause} ORDER BY seq ASC`,
    params
  );

  return result.rows as Reading[];
}

export async function getSessions(): Promise<Session[]> {
  const result = await query(
    'SELECT session_id, label, started_at FROM sessions ORDER BY started_at DESC',
    []
  );
  return result.rows as Session[];
}

export async function upsertReadings(
  sessionId: string,
  readings: Array<{
    seq: number;
    rx_ms: bigint;
    t_ms: bigint;
    temp_c: number;
    press_hpa: number;
    alt_m: number;
    rssi?: number;
    received_at_utc: string;
  }>
): Promise<number> {
  if (readings.length === 0) return 0;

  // Use COPY for bulk insert (faster than individual INSERTs)
  // or INSERT ... ON CONFLICT DO NOTHING for safety
  const values = readings
    .map(
      (r, i) =>
        `('${sessionId}', ${r.seq}, ${r.rx_ms}, ${r.t_ms}, ${r.temp_c}, ${r.press_hpa}, ${r.alt_m}, ${r.rssi ?? 'NULL'}, '${r.received_at_utc}')`
    )
    .join(',');

  const result = await query(
    `INSERT INTO readings (session_id, seq, rx_ms, t_ms, temp_c, press_hpa, alt_m, rssi, received_at_utc)
     VALUES ${values}
     ON CONFLICT (session_id, seq) DO NOTHING`,
    []
  );

  return result.rowCount || 0;
}

export async function upsertSession(
  sessionId: string,
  label?: string
): Promise<void> {
  await query(
    `INSERT INTO sessions (session_id, label, started_at)
     VALUES ($1, $2, NOW())
     ON CONFLICT (session_id) DO UPDATE SET label = COALESCE($2, label)`,
    [sessionId, label || null]
  );
}
