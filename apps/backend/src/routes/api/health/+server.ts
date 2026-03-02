import { json } from '@sveltejs/kit';

export function GET() {
	return json({ status: 'ok', service: 'cycle-sensor-backend', timestamp: Date.now() });
}
