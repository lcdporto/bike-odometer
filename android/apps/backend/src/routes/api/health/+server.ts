import { json } from '@sveltejs/kit';

function toUnixSeconds(timestampMs: number): number {
	return Math.floor(timestampMs / 1000);
}

export function GET() {
	return json({ status: 'ok', service: 'cycle-sensor-backend', timestamp: toUnixSeconds(Date.now()) });
}
