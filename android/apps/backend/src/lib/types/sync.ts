export type NullableNumber = number | null;

export interface SyncSensorRecord {
	id: string;
	name: string;
	wheelSize: string;
	lastSeen: number;
	updatedAt: number;
	deletedAt: NullableNumber;
}

export interface SyncTripRecord {
	id: string;
	sensorId: string;
	startDate: number;
	wheelSize: string;
	updatedAt: number;
	deletedAt: NullableNumber;
}

export interface SyncBucketRecord {
	tripId: string;
	idx: number;
	rotations: number;
	timestamp: number;
	updatedAt: number;
	deletedAt: NullableNumber;
}

export interface SyncPushRequest {
	sensorId: string;
	sensor: SyncSensorRecord;
	trips: SyncTripRecord[];
	buckets: SyncBucketRecord[];
}

export interface SyncPushResponse {
	appliedAt: number;
}

export interface SyncPullRequest {
	sensorId: string;
	since: number;
}

export interface SyncPullResponse {
	serverTimestamp: number;
	sensor: SyncSensorRecord | null;
	trips: SyncTripRecord[];
	buckets: SyncBucketRecord[];
}
