import { json, type RequestHandler } from '@sveltejs/kit';
import { pullSyncData } from '$lib/server/db';
import type { SyncPullRequest, SyncPullResponse } from '$lib/types/sync';

function isValidPullBody(body: unknown): body is SyncPullRequest {
	if (!body || typeof body !== 'object') return false;
	const value = body as Record<string, unknown>;
	if (typeof value.sensorId !== 'string') return false;
	if (typeof value.since !== 'number' || Number.isNaN(value.since)) return false;
	return true;
}

export const POST: RequestHandler = async ({ request }) => {
	const body = await request.json().catch(() => null);
	if (!isValidPullBody(body)) {
		return json({ error: 'Invalid pull payload' }, { status: 400 });
	}

	const data = pullSyncData(body.sensorId, body.since);

	const response: SyncPullResponse = {
		serverTimestamp: Date.now(),
		sensor: data.sensor,
		trips: data.trips,
		buckets: data.buckets
	};

	return json(response);
};
