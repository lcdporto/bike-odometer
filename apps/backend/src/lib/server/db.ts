import { Database } from 'bun:sqlite';
import { mkdirSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import type { SyncBucketRecord, SyncSensorRecord, SyncTripRecord } from '$lib/types/sync';

const BUCKET_MINUTES = 5;

const dbPath = resolve(process.cwd(), 'data', 'sync.db');
mkdirSync(dirname(dbPath), { recursive: true });

const db = new Database(dbPath, { create: true });

db.exec('PRAGMA journal_mode = WAL;');

db.exec(`
CREATE TABLE IF NOT EXISTS sensors (
	id TEXT PRIMARY KEY,
	name TEXT NOT NULL,
	wheel_size TEXT NOT NULL,
	last_seen INTEGER NOT NULL,
	updated_at INTEGER NOT NULL,
	deleted_at INTEGER
);

CREATE TABLE IF NOT EXISTS trips (
	id TEXT PRIMARY KEY,
	sensor_id TEXT NOT NULL,
	start_date INTEGER NOT NULL,
	wheel_size TEXT NOT NULL,
	updated_at INTEGER NOT NULL,
	deleted_at INTEGER,
	FOREIGN KEY(sensor_id) REFERENCES sensors(id)
);

CREATE TABLE IF NOT EXISTS buckets (
	trip_id TEXT NOT NULL,
	idx INTEGER NOT NULL,
	rotations INTEGER NOT NULL,
	timestamp INTEGER NOT NULL,
	updated_at INTEGER NOT NULL,
	deleted_at INTEGER,
	PRIMARY KEY (trip_id, idx),
	FOREIGN KEY(trip_id) REFERENCES trips(id)
);

CREATE INDEX IF NOT EXISTS idx_trips_sensor_id ON trips(sensor_id);
CREATE INDEX IF NOT EXISTS idx_trips_updated_at ON trips(updated_at);
CREATE INDEX IF NOT EXISTS idx_buckets_trip_id ON buckets(trip_id);
CREATE INDEX IF NOT EXISTS idx_buckets_updated_at ON buckets(updated_at);
`);

const upsertSensorStmt = db.query(`
INSERT INTO sensors (id, name, wheel_size, last_seen, updated_at, deleted_at)
VALUES (?, ?, ?, ?, ?, ?)
ON CONFLICT(id) DO UPDATE SET
	name = excluded.name,
	wheel_size = excluded.wheel_size,
	last_seen = excluded.last_seen,
	updated_at = excluded.updated_at,
	deleted_at = excluded.deleted_at
WHERE excluded.updated_at >= sensors.updated_at;
`);

const upsertTripStmt = db.query(`
INSERT INTO trips (
	id, sensor_id, start_date, wheel_size, updated_at, deleted_at
)
VALUES (
	?, ?, ?, ?, ?, ?
)
ON CONFLICT(id) DO UPDATE SET
	sensor_id = excluded.sensor_id,
	start_date = excluded.start_date,
	wheel_size = excluded.wheel_size,
	updated_at = excluded.updated_at,
	deleted_at = excluded.deleted_at
WHERE excluded.updated_at >= trips.updated_at;
`);

const upsertBucketStmt = db.query(`
INSERT INTO buckets (trip_id, idx, rotations, timestamp, updated_at, deleted_at)
VALUES (?, ?, ?, ?, ?, ?)
ON CONFLICT(trip_id, idx) DO UPDATE SET
	rotations = excluded.rotations,
	timestamp = excluded.timestamp,
	updated_at = excluded.updated_at,
	deleted_at = excluded.deleted_at
WHERE excluded.updated_at >= buckets.updated_at;
`);

const selectSensorStmt = db.query(`
SELECT id, name, wheel_size, last_seen, updated_at, deleted_at
FROM sensors
WHERE id = ?;
`);

const selectTripsStmt = db.query(`
SELECT id, sensor_id, start_date, wheel_size, updated_at, deleted_at
FROM trips
WHERE sensor_id = ? AND updated_at > ?;
`);

const selectBucketsStmt = db.query(`
SELECT b.trip_id, b.idx, b.rotations, b.timestamp, b.updated_at, b.deleted_at
FROM buckets b
INNER JOIN trips t ON t.id = b.trip_id
WHERE t.sensor_id = ? AND b.updated_at > ?;
`);

const transactPush = db.transaction((sensor: SyncSensorRecord, trips: SyncTripRecord[], buckets: SyncBucketRecord[]) => {
	upsertSensorStmt.run(
		sensor.id,
		sensor.name,
		sensor.wheelSize,
		sensor.lastSeen,
		sensor.updatedAt,
		sensor.deletedAt
	);

	for (const trip of trips) {
		upsertTripStmt.run(
			trip.id,
			trip.sensorId,
			trip.startDate,
			trip.wheelSize,
			trip.updatedAt,
			trip.deletedAt
		);
	}

	for (const bucket of buckets) {
		upsertBucketStmt.run(
			bucket.tripId,
			bucket.idx,
			bucket.rotations,
			bucket.timestamp,
			bucket.updatedAt,
			bucket.deletedAt
		);
	}
});

export function pushSyncData(sensor: SyncSensorRecord, trips: SyncTripRecord[], buckets: SyncBucketRecord[]) {
	transactPush(sensor, trips, buckets);
}

export function pullSyncData(sensorId: string, since: number) {
	const sensorRow = selectSensorStmt.get(sensorId) as
		| {
				id: string;
				name: string;
				wheel_size: string;
				last_seen: number;
				updated_at: number;
				deleted_at: number | null;
		  }
		| undefined;

	const tripsRows = selectTripsStmt.all(sensorId, since) as Array<{
		id: string;
		sensor_id: string;
		start_date: number;
		wheel_size: string;
		updated_at: number;
		deleted_at: number | null;
	}>;

	const bucketRows = selectBucketsStmt.all(sensorId, since) as Array<{
		trip_id: string;
		idx: number;
		rotations: number;
		timestamp: number;
		updated_at: number;
		deleted_at: number | null;
	}>;

	const sensor = sensorRow
		? {
				id: sensorRow.id,
				name: sensorRow.name,
				wheelSize: sensorRow.wheel_size,
				lastSeen: sensorRow.last_seen,
				updatedAt: sensorRow.updated_at,
				deletedAt: sensorRow.deleted_at
		  }
		: null;

	const trips = tripsRows.map((trip) => ({
		id: trip.id,
		sensorId: trip.sensor_id,
		startDate: trip.start_date,
		wheelSize: trip.wheel_size,
		updatedAt: trip.updated_at,
		deletedAt: trip.deleted_at
	}));

	const buckets = bucketRows.map((bucket) => ({
		tripId: bucket.trip_id,
		idx: bucket.idx,
		rotations: bucket.rotations,
		timestamp: bucket.timestamp,
		updatedAt: bucket.updated_at,
		deletedAt: bucket.deleted_at
	}));

	return { sensor, trips, buckets };
}

export interface DashboardBucket {
	idx: number;
	rotations: number;
	timestamp: number;
}

export interface DashboardTrip {
	id: string;
	startDate: number;
	wheelSize: string;
	distance: number;
	duration: number;
	avgSpeed: number;
	totalRotations: number;
	updatedAt: number;
	buckets: DashboardBucket[];
}

export interface DashboardSensor {
	id: string;
	name: string;
	wheelSize: string;
	lastSeen: number;
	updatedAt: number;
	trips: DashboardTrip[];
}

function getWheelCircumference(wheelSize: string): number {
	const diameterInches = Number.parseFloat(wheelSize);
	return Number.isFinite(diameterInches) ? diameterInches * Math.PI * 0.0254 : 0;
}

function getTripStats(wheelSize: string, buckets: DashboardBucket[]) {
	const totalRotations = buckets.reduce((sum, bucket) => sum + bucket.rotations, 0);
	const duration = buckets.length * BUCKET_MINUTES;
	const distance = (totalRotations * getWheelCircumference(wheelSize)) / 1000;

	return {
		distance,
		duration,
		avgSpeed: duration > 0 ? (distance / duration) * 60 : 0,
		totalRotations
	};
}

export function getDashboardData(): DashboardSensor[] {
	const sensors = db
		.query(
			`SELECT id, name, wheel_size, last_seen, updated_at
			 FROM sensors
			 WHERE deleted_at IS NULL
			 ORDER BY last_seen DESC;`
		)
		.all() as Array<{
			id: string;
			name: string;
			wheel_size: string;
			last_seen: number;
			updated_at: number;
		}>;

	const tripStmt = db.query(
		`SELECT id, start_date, wheel_size, updated_at
		 FROM trips
		 WHERE sensor_id = ? AND deleted_at IS NULL
		 ORDER BY start_date DESC;`
	);

	const bucketStmt = db.query(
		`SELECT idx, rotations, timestamp
		 FROM buckets
		 WHERE trip_id = ? AND deleted_at IS NULL
		 ORDER BY idx ASC;`
	);

	return sensors.map((sensor) => {
		const trips = tripStmt.all(sensor.id) as Array<{
			id: string;
			start_date: number;
			wheel_size: string;
			updated_at: number;
		}>;

		return {
			id: sensor.id,
			name: sensor.name,
			wheelSize: sensor.wheel_size,
			lastSeen: sensor.last_seen,
			updatedAt: sensor.updated_at,
			trips: trips.map((trip) => {
				const buckets = (
					bucketStmt.all(trip.id) as Array<{ idx: number; rotations: number; timestamp: number }>
				).map((bucket) => ({
					idx: bucket.idx,
					rotations: bucket.rotations,
					timestamp: bucket.timestamp
				}));
				const stats = getTripStats(trip.wheel_size, buckets);

				return {
					id: trip.id,
					startDate: trip.start_date,
					wheelSize: trip.wheel_size,
					...stats,
					updatedAt: trip.updated_at,
					buckets
				};
			})
		};
	});
}
