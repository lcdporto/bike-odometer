import { getSensorWithTrips } from '$lib/persistence/sqlite';
import { sensorState } from '$lib/stores/sensor.svelte';
import type { Trip } from '$lib/stores/trips.svelte';
import { PUBLIC_BACKEND_API_URL } from '$env/static/public';
import type {
	SyncPushRequest,
	SyncPushResponse,
	SyncTripRecord,
	SyncBucketRecord
} from '$lib/types/sync';

function getSyncApiBaseUrl(): string {
	return PUBLIC_BACKEND_API_URL;
}

function toSyncRecords(sensorId: string, wheelSize: string, syncedAt: number, trips: Trip[]) {
	const tripRecords: SyncTripRecord[] = [];
	const bucketRecords: SyncBucketRecord[] = [];

	for (const trip of trips) {
		const startDate = trip.buckets[0]?.timestamp ?? syncedAt;
		tripRecords.push({
			id: trip.id,
			sensorId,
			startDate,
			wheelSize,
			distance: trip.distance,
			duration: trip.duration,
			avgSpeed: trip.avgSpeed,
			totalRotations: trip.totalRotations,
			updatedAt: syncedAt,
			deletedAt: null
		});

		for (let idx = 0; idx < trip.buckets.length; idx++) {
			const bucket = trip.buckets[idx];
			bucketRecords.push({
				tripId: trip.id,
				idx,
				rotations: bucket.rotations,
				timestamp: bucket.timestamp,
				updatedAt: syncedAt,
				deletedAt: null
			});
		}
	}

	return {
		tripRecords,
		bucketRecords
	};
}

export async function syncSensorSnapshot(sensorId: string): Promise<SyncPushResponse | null> {
	const sensorData = await getSensorWithTrips(sensorId);
	if (!sensorData) {
		return null;
	}

	const syncedAt = Date.now();
	const { tripRecords, bucketRecords } = toSyncRecords(
		sensorId,
		sensorData.wheelSize,
		syncedAt,
		sensorData.trips
	);

	const payload: SyncPushRequest = {
		sensorId,
		sensor: {
			id: sensorData.sensor.id,
			name: sensorData.sensor.name,
			wheelSize: sensorData.wheelSize,
			lastSeen: syncedAt,
			updatedAt: syncedAt,
			deletedAt: null
		},
		trips: tripRecords,
		buckets: bucketRecords
	};

	const response = await fetch(`${getSyncApiBaseUrl()}/api/sync/push`, {
		method: 'POST',
		headers: {
			'content-type': 'application/json'
		},
		body: JSON.stringify(payload)
	});

	if (!response.ok) {
		const text = await response.text();
		throw new Error(`Sync push failed (${response.status}): ${text}`);
	}

	return (await response.json()) as SyncPushResponse;
}

export async function syncConnectedSensorSnapshot(): Promise<SyncPushResponse | null> {
	const sensorId = sensorState.connectedSensor?.id;
	if (!sensorId) {
		return null;
	}

	return syncSensorSnapshot(sensorId);
}
