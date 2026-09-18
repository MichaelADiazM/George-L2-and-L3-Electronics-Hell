/**
 * app/layout.tsx
 * --------
 * Next.js root layout.
 */

import type { Metadata } from 'next';

export const metadata: Metadata = {
  title: 'Rocket Telemetry Dashboard',
  description: 'Real-time flight telemetry monitoring',
};

export default function RootLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    <html lang="en">
      <body style={{ margin: 0, padding: 0 }}>
        {children}
      </body>
    </html>
  );
}
