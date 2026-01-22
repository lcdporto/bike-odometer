import { CapacitorSQLite, SQLiteConnection, type SQLiteDBConnection } from '@capacitor-community/sqlite';
import type { Sensor } from '$lib/stores/sensor.svelte';
import type { Trip } from '$lib/stores/trips.svelte';

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

async function loadTripsForSensor(sensorId: string): Promise<Trip[]> {
	const tripRows = await query<TripRow>(
		`SELECT * FROM trips WHERE sensor_id = ? ORDER BY start_date DESC;`,
		[sensorId]
	);

	const trips: Trip[] = [];
	for (const row of tripRows) {
		const buckets = await query<BucketRow>(
			`SELECT rotations, timestamp, idx FROM buckets WHERE trip_id = ? ORDER BY idx ASC;`,
			[row.id]
		);
		const formattedBuckets = buckets.map((b) => ({
			time: new Date(b.timestamp).toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit' }),
			rotations: b.rotations,
			timestamp: b.timestamp
		}));

		trips.push({
			id: row.id,
			date: new Date(row.start_date).toLocaleDateString('en-US', {
				weekday: 'short',
				month: 'short',
				day: 'numeric'
			}),
			startTime: new Date(row.start_date).toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit' }),
			endTime: new Date(row.start_date + row.duration * 60 * 1000).toLocaleTimeString('en-US', {
				hour: '2-digit',
				minute: '2-digit'
			}),
			distance: row.distance,
			duration: row.duration,
			avgSpeed: row.avg_speed,
			totalRotations: row.total_rotations,
			buckets: formattedBuckets
		});
	}

	return trips;
}

export async function saveSensorDescriptor(sensor: Sensor, wheelSize: string, trips: Trip[]) {
	const now = Date.now();
	await run(
		`INSERT OR REPLACE INTO sensors (id, name, strength, wheel_size, last_seen)
		 VALUES (?, ?, ?, ?, ?);`,
		[sensor.id, sensor.name, sensor.signalStrength, wheelSize, now]
	);

	for (const trip of trips) {
		const startTimestamp = trip.buckets[0]?.timestamp ?? now;
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
				[trip.id, i, bucket.rotations, bucket.timestamp]
			);
		}
	}
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
		lastSeen: row.last_seen
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
