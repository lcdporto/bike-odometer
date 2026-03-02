import { json, type RequestHandler } from '@sveltejs/kit';
import { pushSyncData } from '$lib/server/db';
import type { SyncPushRequest, SyncPushResponse } from '$lib/types/sync';

function isValidPushBody(body: unknown): body is SyncPushRequest {
	if (!body || typeof body !== 'object') return false;
	const value = body as Record<string, unknown>;
	if (typeof value.sensorId !== 'string') return false;
	if (!value.sensor || typeof value.sensor !== 'object') return false;
	if (!Array.isArray(value.trips)) return false;
	if (!Array.isArray(value.buckets)) return false;
	return true;
}

export const POST: RequestHandler = async ({ request }) => {
	const body = await request.json().catch(() => null);
	if (!isValidPushBody(body)) {
		return json({ error: 'Invalid push payload' }, { status: 400 });
	}

	if (body.sensor.id !== body.sensorId) {
		return json({ error: 'sensorId mismatch' }, { status: 400 });
	}

	if (body.trips.some((trip) => trip.sensorId !== body.sensorId)) {
		return json({ error: 'Trip sensorId mismatch' }, { status: 400 });
	}

	pushSyncData(body.sensor, body.trips, body.buckets);

	const response: SyncPushResponse = {
		appliedAt: Date.now()
	};

	return json(response);
};
