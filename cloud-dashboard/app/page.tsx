/**
 * app/page.tsx
 * --------
 * Main telemetry dashboard page.
 * Polls /api/readings every 1-2 seconds, displays live stats and altitude chart.
 * Matches the visual language of main/server.cpp: centered layout, big stat numbers,
 * Font Awesome icons, but with real CSS and a Recharts altitude chart.
 */

'use client';

import { useEffect, useState } from 'react';
import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  ResponsiveContainer,
} from 'recharts';

interface Reading {
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

export default function Dashboard() {
  const [readings, setReadings] = useState<Reading[]>([]);
  const [lastReadingTime, setLastReadingTime] = useState<number>(0);
  const [sessionId, setSessionId] = useState<string>('');
  const [inputSessionId, setInputSessionId] = useState<string>('');
  const [staleness, setStaleness] = useState<number>(0);
  const [isConnected, setIsConnected] = useState<boolean>(false);

  // Parse session ID from URL or localStorage
  useEffect(() => {
    const params = new URLSearchParams(window.location.search);
    const urlSessionId = params.get('session_id');
    const storedSessionId = localStorage.getItem('lastSessionId');

    const activeSessionId = urlSessionId || storedSessionId || '';
    if (activeSessionId) {
      setSessionId(activeSessionId);
      setInputSessionId(activeSessionId);
      localStorage.setItem('lastSessionId', activeSessionId);
    }
  }, []);

  // Poll for readings
  useEffect(() => {
    if (!sessionId) return;

    const poll = async () => {
      try {
        const url = `/api/readings?session_id=${encodeURIComponent(sessionId)}`;
        const response = await fetch(url);
        if (!response.ok) {
          setIsConnected(false);
          return;
        }

        const data = await response.json();
        setReadings(data);

        if (data.length > 0) {
          setLastReadingTime(Date.now());
          setIsConnected(true);
        }
      } catch (error) {
        console.error('Fetch error:', error);
        setIsConnected(false);
      }
    };

    // Poll immediately, then every 1.5 seconds
    poll();
    const interval = setInterval(poll, 1500);

    return () => clearInterval(interval);
  }, [sessionId]);

  // Update staleness every second
  useEffect(() => {
    const interval = setInterval(() => {
      if (lastReadingTime > 0) {
        const secondsSince = Math.floor((Date.now() - lastReadingTime) / 1000);
        setStaleness(secondsSince);
      }
    }, 1000);

    return () => clearInterval(interval);
  }, [lastReadingTime]);

  const handleSessionChange = (e: React.FormEvent<HTMLFormElement>) => {
    e.preventDefault();
    if (inputSessionId.trim()) {
      setSessionId(inputSessionId);
      localStorage.setItem('lastSessionId', inputSessionId);
    }
  };

  if (!sessionId) {
    return (
      <div style={styles.container}>
        <div style={styles.card}>
          <h1 style={styles.title}>🚀 Rocket Telemetry Dashboard</h1>
          <p style={styles.subtitle}>Enter a session ID to view flight data:</p>
          <form onSubmit={handleSessionChange} style={styles.form}>
            <input
              type="text"
              placeholder="Session UUID (e.g., 550e8400-e29b-41d4-a716-446655440000)"
              value={inputSessionId}
              onChange={(e) => setInputSessionId(e.target.value)}
              style={styles.input}
            />
            <button type="submit" style={styles.button}>
              Load Session
            </button>
          </form>
        </div>
      </div>
    );
  }

  const latest = readings.length > 0 ? readings[readings.length - 1] : null;
  const chartData = readings.map((r) => ({
    t_ms: Number(r.t_ms),
    alt_m: r.alt_m,
  }));

  const staleStyle: React.CSSProperties =
    staleness > 5 ? { color: '#d32f2f' } : { color: '#4caf50' };

  return (
    <div style={styles.container}>
      <div style={styles.header}>
        <h1 style={styles.title}>🚀 Ground Station</h1>
        <p style={{ ...styles.subtitle, marginBottom: '10px' }}>
          Session: <code style={styles.code}>{sessionId.slice(0, 8)}...</code>
        </p>
        <p style={{ ...styles.subtitle, ...staleStyle }}>
          {staleness === 0
            ? '🔴 No data yet'
            : `Last update: ${staleness}s ago`}
        </p>
      </div>

      <div style={styles.statsGrid}>
        {/* Temperature */}
        <div style={styles.statCard}>
          <div style={styles.iconLarge}>🌡️</div>
          <div style={styles.statValue}>
            {latest ? latest.temp_c.toFixed(1) : '--'}
          </div>
          <div style={styles.statUnit}>°C</div>
          <div style={styles.label}>Temperature</div>
        </div>

        {/* Pressure */}
        <div style={styles.statCard}>
          <div style={styles.iconLarge}>📊</div>
          <div style={styles.statValue}>
            {latest ? latest.press_hpa.toFixed(1) : '--'}
          </div>
          <div style={styles.statUnit}>hPa</div>
          <div style={styles.label}>Pressure</div>
        </div>

        {/* Altitude */}
        <div style={styles.statCard}>
          <div style={styles.iconLarge}>⛰️</div>
          <div style={styles.statValue}>
            {latest ? latest.alt_m.toFixed(0) : '--'}
          </div>
          <div style={styles.statUnit}>m</div>
          <div style={styles.label}>Altitude</div>
        </div>

        {/* Signal */}
        <div style={styles.statCard}>
          <div style={styles.iconLarge}>📡</div>
          <div style={styles.statValue}>
            {latest && latest.rssi ? latest.rssi : '--'}
          </div>
          <div style={styles.statUnit}>dBm</div>
          <div style={styles.label}>Signal (RSSI)</div>
        </div>
      </div>

      {/* Altitude vs. Time Chart */}
      {chartData.length > 1 && (
        <div style={styles.chartContainer}>
          <h2 style={{ marginBottom: '20px', textAlign: 'center' }}>
            Altitude Profile
          </h2>
          <ResponsiveContainer width="100%" height={300}>
            <LineChart data={chartData}>
              <CartesianGrid strokeDasharray="3 3" />
              <XAxis
                dataKey="t_ms"
                label={{ value: 'Mission Time (ms)', position: 'insideBottomRight', offset: -5 }}
              />
              <YAxis
                label={{
                  value: 'Altitude (m)',
                  angle: -90,
                  position: 'insideLeft',
                }}
              />
              <Tooltip
                formatter={(value: any) => `${value.toFixed(1)} m`}
                labelFormatter={(value: any) => `t = ${value} ms`}
              />
              <Line
                type="monotone"
                dataKey="alt_m"
                stroke="#4caf50"
                dot={false}
                name="Altitude"
              />
            </LineChart>
          </ResponsiveContainer>
        </div>
      )}

      {/* Packet Info */}
      {latest && (
        <div style={styles.infoBox}>
          <p>
            <strong>Packets received:</strong> {readings.length}
          </p>
          <p>
            <strong>Flight time:</strong> {(Number(latest.t_ms) / 1000).toFixed(2)} s
          </p>
          <p>
            <strong>Ground RX time:</strong> {(Number(latest.rx_ms) / 1000).toFixed(2)} s
          </p>
        </div>
      )}

      {/* Back button */}
      <div style={styles.footer}>
        <button
          onClick={() => {
            setSessionId('');
            setInputSessionId('');
          }}
          style={styles.backButton}
        >
          ← Back to Session Select
        </button>
      </div>
    </div>
  );
}

const styles: Record<string, React.CSSProperties> = {
  container: {
    minHeight: '100vh',
    backgroundColor: '#f5f5f5',
    padding: '20px',
    fontFamily: 'Arial, sans-serif',
  },
  header: {
    textAlign: 'center',
    marginBottom: '40px',
  },
  title: {
    fontSize: '2.5rem',
    margin: '0 0 10px 0',
    color: '#1a1a1a',
  },
  subtitle: {
    fontSize: '0.95rem',
    color: '#666',
    margin: '0',
  },
  code: {
    backgroundColor: '#e0e0e0',
    padding: '2px 6px',
    borderRadius: '3px',
    fontFamily: 'monospace',
    fontSize: '0.85rem',
  },
  form: {
    display: 'flex',
    gap: '10px',
    justifyContent: 'center',
    flexWrap: 'wrap',
  },
  input: {
    padding: '10px 15px',
    fontSize: '1rem',
    border: '1px solid #ddd',
    borderRadius: '4px',
    minWidth: '300px',
  },
  button: {
    padding: '10px 20px',
    fontSize: '1rem',
    backgroundColor: '#4caf50',
    color: '#fff',
    border: 'none',
    borderRadius: '4px',
    cursor: 'pointer',
    fontWeight: 'bold',
  },
  card: {
    backgroundColor: '#fff',
    padding: '30px',
    borderRadius: '8px',
    boxShadow: '0 2px 8px rgba(0,0,0,0.1)',
    maxWidth: '600px',
    margin: '0 auto',
  },
  statsGrid: {
    display: 'grid',
    gridTemplateColumns: 'repeat(auto-fit, minmax(150px, 1fr))',
    gap: '20px',
    marginBottom: '40px',
  },
  statCard: {
    backgroundColor: '#fff',
    padding: '20px',
    borderRadius: '8px',
    boxShadow: '0 2px 8px rgba(0,0,0,0.1)',
    textAlign: 'center',
  },
  iconLarge: {
    fontSize: '2.5rem',
    marginBottom: '10px',
  },
  statValue: {
    fontSize: '2rem',
    fontWeight: 'bold',
    color: '#1a1a1a',
    margin: '5px 0',
  },
  statUnit: {
    fontSize: '0.85rem',
    color: '#666',
    marginBottom: '10px',
  },
  label: {
    fontSize: '0.9rem',
    color: '#999',
    marginTop: '5px',
  },
  chartContainer: {
    backgroundColor: '#fff',
    padding: '20px',
    borderRadius: '8px',
    boxShadow: '0 2px 8px rgba(0,0,0,0.1)',
    marginBottom: '40px',
  },
  infoBox: {
    backgroundColor: '#fff',
    padding: '20px',
    borderRadius: '8px',
    boxShadow: '0 2px 8px rgba(0,0,0,0.1)',
    marginBottom: '40px',
    fontSize: '0.95rem',
    color: '#666',
  },
  footer: {
    textAlign: 'center',
    marginTop: '40px',
  },
  backButton: {
    padding: '10px 20px',
    fontSize: '0.95rem',
    backgroundColor: '#2196f3',
    color: '#fff',
    border: 'none',
    borderRadius: '4px',
    cursor: 'pointer',
  },
};
