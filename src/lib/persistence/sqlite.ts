import { CapacitorSQLite, SQLiteConnection, type SQLiteDBConnection } from '@capacitor-community/sqlite';
import type { Sensor } from '$lib/stores/sensor.svelte';
import type { Trip } from '$lib/stores/trips.svelte';
import { formatTime24h, getWheelCircumference, calculateDistance, isPlaceholderDeviceName } from '$lib/utils';

const DB_NAME = 'cycleSensor';
let sqlite: SQLiteConnection | undefined = undefined;
let db: SQLiteDBConnection | undefined = undefined;
let connectionPromise: Promise<SQLiteDBConnection> | null = null;

async function ensureConnection(): Promise<SQLiteDBConnection> {
	// If there's already a connection in progress, wait for it
	if (connectionPromise) {
		console.log('[SQLite] waiting for existing connection promise');
		return connectionPromise;
	}
	
	// Create the connection promise to prevent race conditions
	connectionPromise = (async () => {
		try {
			if (!sqlite) {
				console.log('[SQLite] creating SQLiteConnection');
				sqlite = new SQLiteConnection(CapacitorSQLite);
			}

			// Check and fix any inconsistent connection states
			try {
				await sqlite.checkConnectionsConsistency();
				console.log('[SQLite] connections consistency checked');
			} catch (e) {
				console.log('[SQLite] could not check connections consistency:', e);
			}

			// If we already have an open connection, return it
			if (db) {
				try {
					const isOpen = await db.isDBOpen();
					if (isOpen.result) {
						console.log('[SQLite] returning existing open connection');
						return db;
					}
				} catch {
					// Connection is stale, reset it
					db = undefined;
				}
			}

			// Try to retrieve existing connection
			try {
				console.log('[SQLite] trying to retrieve existing connection');
				db = await sqlite.retrieveConnection(DB_NAME, false);
				console.log('[SQLite] retrieved existing connection');
			} catch (e) {
				console.log('[SQLite] could not retrieve connection:', e);
				db = undefined;
			}

			// If no connection exists, create a new one
			if (!db) {
				console.log('[SQLite] creating new connection');
				db = await sqlite.createConnection(DB_NAME, false, 'no-encryption', 1, false);
				console.log('[SQLite] connection created');
			}

			// Open the database
			try {
				const isOpen = await db.isDBOpen();
				if (!isOpen.result) {
					console.log('[SQLite] opening database');
					await db.open();
				}
			} catch {
				console.log('[SQLite] opening database (after check failed)');
				await db.open();
			}

			console.log('[SQLite] ensuring schema');
			await ensureSchema(db);
			console.log('[SQLite] connection ready');
			return db;
		} catch (error) {
			// Reset state on error so next call can retry
			connectionPromise = null;
			throw error;
		}
	})();
	
	return connectionPromise;
}

async function ensureSchema(conn: SQLiteDBConnection) {
	const statements = `
	CREATE TABLE IF NOT EXISTS sensors (
		id TEXT PRIMARY KEY,
		name TEXT,
		strength INTEGER,
		wheel_size TEXT,
		last_seen INTEGER
	);
	CREATE TABLE IF NOT EXISTS trips (
		id TEXT PRIMARY KEY,
		sensor_id TEXT,
		start_date INTEGER,
		wheel_size TEXT,
		distance REAL,
		duration INTEGER,
		avg_speed REAL,
		total_rotations INTEGER,
		FOREIGN KEY(sensor_id) REFERENCES sensors(id)
	);
	CREATE TABLE IF NOT EXISTS buckets (
		trip_id TEXT,
		idx INTEGER,
		rotations INTEGER,
		timestamp INTEGER,
		PRIMARY KEY (trip_id, idx),
		FOREIGN KEY(trip_id) REFERENCES trips(id)
	);
	`; // separators are supported by exec
	await conn.execute(statements);
}

export async function run(sql: string, params: (string | number)[] = []) {
	const conn = await ensureConnection();
	return conn.run(sql, params);
}

export async function query<T = Record<string, unknown>>(sql: string, params: (string | number)[] = []): Promise<T[]> {
	const conn = await ensureConnection();
	const res = await conn.query(sql, params);
	return (res?.values as T[]) ?? [];
}

export async function resetDb() {
	if (db) {
		await db.close();
		await sqlite?.closeConnection(DB_NAME, false);
		db = undefined;
	}
}

type TripRow = {
	id: string;
	sensor_id: string;
	start_date: number;
	wheel_size: string;
	distance: number;
	duration: number;
	avg_speed: number;
	total_rotations: number;
};

type BucketRow = { rotations: number; timestamp: number; idx: number };

const FIVE_MINUTES_MS = 5 * 60 * 1000;

function toUnixSeconds(timestampMs: number): number {
	return Math.floor(timestampMs / 1000);
}

function normalizePersistedTimestamp(timestamp: number): number {
	return timestamp < 1_000_000_000_000 ? timestamp * 1000 : timestamp;
}

function normalizeBucketTimestamp(timestamp: number, tripStartDate: number, bucketIndex: number): number {
	if (timestamp >= 1_000_000_000_000) {
		return timestamp;
	}

	if (bucketIndex > 0 && timestamp - tripStartDate === bucketIndex * FIVE_MINUTES_MS) {
		return normalizePersistedTimestamp(tripStartDate) + bucketIndex * FIVE_MINUTES_MS;
	}

	return normalizePersistedTimestamp(timestamp);
}

async function loadTripsForSensor(sensorId: string): Promise<Trip[]> {
	const tripRows = await query<TripRow>(
		`SELECT * FROM trips WHERE sensor_id = ? ORDER BY start_date DESC;`,
		[sensorId]
	);

	const trips: Trip[] = [];
	for (const row of tripRows) {
		const startTimestamp = normalizePersistedTimestamp(row.start_date);
		const buckets = await query<BucketRow>(
			`SELECT rotations, timestamp, idx FROM buckets WHERE trip_id = ? ORDER BY idx ASC;`,
			[row.id]
		);
		const formattedBuckets = buckets.map((bucket) => {
			const bucketTimestamp = normalizeBucketTimestamp(bucket.timestamp, row.start_date, bucket.idx);
			return {
				time: formatTime24h(new Date(bucketTimestamp)),
				rotations: bucket.rotations,
				timestamp: bucketTimestamp
			};
		});

		// Recalculate distance using correct wheel circumference formula
		// wheel_size is diameter in inches, convert to circumference in meters
		const wheelCircumferenceMeters = getWheelCircumference(row.wheel_size);
		const correctedDistance = calculateDistance(row.total_rotations, wheelCircumferenceMeters);
		const correctedAvgSpeed = row.duration > 0 ? (correctedDistance / row.duration) * 60 : 0;

		trips.push({
			id: row.id,
			date: new Date(startTimestamp).toLocaleDateString('en-US', {
				weekday: 'short',
				month: 'short',
				day: 'numeric'
			}),
			startTime: formatTime24h(new Date(startTimestamp)),
			endTime: formatTime24h(new Date(startTimestamp + row.duration * 60 * 1000)),
			distance: correctedDistance,
			duration: row.duration,
			avgSpeed: correctedAvgSpeed,
			totalRotations: row.total_rotations,
			buckets: formattedBuckets
		});
	}
	return trips;
}

async function resolvePersistedSensorName(sensor: Sensor): Promise<string> {
	let sensorName = sensor.name;
	if (isPlaceholderDeviceName(sensorName)) {
		const existing = await query<{ name: string }>(
			`SELECT name FROM sensors WHERE id = ? LIMIT 1;`,
			[sensor.id]
		);
		const existingName = existing[0]?.name;
		if (!isPlaceholderDeviceName(existingName)) {
			sensorName = existingName;
		}
	}

	return sensorName;
}

async function upsertSensor(sensor: Sensor, wheelSize: string, lastSeen: number) {
	const sensorName = await resolvePersistedSensorName(sensor);
	await run(
		`INSERT OR REPLACE INTO sensors (id, name, strength, wheel_size, last_seen)
		 VALUES (?, ?, ?, ?, ?);`,
		[sensor.id, sensorName, sensor.signalStrength, wheelSize, lastSeen]
	);
}

export async function saveSensorDescriptor(sensor: Sensor, wheelSize: string, trips: Trip[]) {
	const now = Date.now();
	const nowSeconds = toUnixSeconds(now);
	await upsertSensor(sensor, wheelSize, nowSeconds);

	for (const trip of trips) {
		const startTimestamp = toUnixSeconds(trip.buckets[0]?.timestamp ?? now);
		await run(`DELETE FROM buckets WHERE trip_id = ?;`, [trip.id]);
		await run(`DELETE FROM trips WHERE id = ?;`, [trip.id]);
		await run(
			`INSERT INTO trips (id, sensor_id, start_date, wheel_size, distance, duration, avg_speed, total_rotations)
			 VALUES (?, ?, ?, ?, ?, ?, ?, ?);`,
			[
				trip.id,
				sensor.id,
				startTimestamp,
				wheelSize,
				trip.distance,
				trip.duration,
				trip.avgSpeed,
				trip.totalRotations
			]
		);
		for (let i = 0; i < trip.buckets.length; i++) {
			const bucket = trip.buckets[i];
			await run(
				`INSERT OR REPLACE INTO buckets (trip_id, idx, rotations, timestamp) VALUES (?, ?, ?, ?);`,
				[trip.id, i, bucket.rotations, toUnixSeconds(bucket.timestamp)]
			);
		}
	}
}

export async function saveSensorWheelSize(sensor: Sensor, wheelSize: string) {
	await upsertSensor(sensor, wheelSize, toUnixSeconds(Date.now()));
}

export async function loadLatestSensorWithTrips(): Promise<{ sensor: Sensor; wheelSize: string; trips: Trip[] } | null> {
	const sensors = await query<{
		id: string;
		name: string;
		strength: number;
		wheel_size: string;
		last_seen: number;
	}>(`SELECT * FROM sensors ORDER BY last_seen DESC LIMIT 1;`);
	if (!sensors.length) return null;
	const sensorRow = sensors[0];
	const trips = await loadTripsForSensor(sensorRow.id);
	return {
		sensor: { id: sensorRow.id, name: sensorRow.name, signalStrength: sensorRow.strength },
		wheelSize: sensorRow.wheel_size,
		trips
	};
}

export async function getAllSensors(): Promise<Array<Sensor & { lastSeen?: number }>> {
	const rows = await query<{
		id: string;
		name: string;
		strength: number;
		last_seen: number;
	}>(`SELECT id, name, strength, last_seen FROM sensors ORDER BY last_seen DESC;`);
	
	return rows.map((row) => ({
		id: row.id,
		name: row.name,
		signalStrength: row.strength,
		lastSeen: normalizePersistedTimestamp(row.last_seen)
	}));
}

export async function getSensorWithTrips(sensorId: string): Promise<{ sensor: Sensor; wheelSize: string; trips: Trip[] } | null> {
	const sensors = await query<{
		id: string;
		name: string;
		strength: number;
		wheel_size: string;
	}>(`SELECT * FROM sensors WHERE id = ?;`, [sensorId]);
	
	if (!sensors.length) return null;
	
	const sensorRow = sensors[0];
	const trips = await loadTripsForSensor(sensorRow.id);
	
	return {
		sensor: { id: sensorRow.id, name: sensorRow.name, signalStrength: sensorRow.strength },
		wheelSize: sensorRow.wheel_size,
		trips
	};
}
