import { DEFAULT_WHEEL_SIZE, WHEEL_SIZES } from '$lib/config/wheels';
import { connectSensor, sensorState, setRotationBuckets, setWheelSize, type RotationBucket, type Sensor } from '$lib/stores/sensor.svelte';
import { setTrips, tripsState, type Trip } from '$lib/stores/trips.svelte';

type SensorScanResult = { macAddress: string; strength: number; name: string };

type SensorDescriptorTrip = {
	id: number;
 /**
  * Start date of the trip in unix ms
  **/
	startDate: number;
  /**
   * Buckets of rotations per 5-minute interval
   */
	buckets: number[];
};

type SensorDescriptor = {
  /**
   * Wheel size in inches
   */
	wheelSize: number;
	trips: SensorDescriptorTrip[];
};

export function getWheelCircumference(size: string): number {
	const parsed = Number.parseInt(size, 10);
	return Number.isFinite(parsed) ? (parsed * Math.PI) / 1000 : 0;
}

function inchesToWheelSizeValue(inches: number): string {
	return String(inches || Number.parseInt(DEFAULT_WHEEL_SIZE, 10));
}

function toRotationBuckets(trip: SensorDescriptorTrip): RotationBucket[] {
	return trip.buckets.map((rotations, idx) => {
		const timestamp = trip.startDate + idx * 5 * 60 * 1000;
		return {
			time: new Date(timestamp).toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit' }),
			rotations,
			timestamp
		};
	});
}

function toTrip(trip: SensorDescriptorTrip, wheelSize: string): Trip {
	const wheelCircumference = getWheelCircumference(wheelSize);
	const buckets = toRotationBuckets(trip);
	const totalRotations = buckets.reduce((sum, b) => sum + b.rotations, 0);
	const distance = (totalRotations * wheelCircumference) / 1000;
	const duration = buckets.length * 5;
	const avgSpeed = duration > 0 ? (distance / duration) * 60 : 0;
	const startDate = new Date(trip.startDate);
	const endDate = new Date(trip.startDate + duration * 60 * 1000);

	return {
		id: `trip-${trip.id}`,
		date: startDate.toLocaleDateString('en-US', {
			weekday: 'short',
			month: 'short',
			day: 'numeric'
		}),
		startTime: startDate.toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit' }),
		endTime: endDate.toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit' }),
		distance,
		duration,
		avgSpeed,
		totalRotations,
		buckets
	};
}

function applySensorDescriptor(sensor: Sensor, descriptor: SensorDescriptor) {
	const wheelSize = inchesToWheelSizeValue(descriptor.wheelSize);
	setWheelSize(wheelSize);
	connectSensor(sensor);

	const trips = descriptor.trips.map((trip) => toTrip(trip, wheelSize));
	setTrips(trips);

	if (descriptor.trips.length > 0) {
		const latest = [...descriptor.trips].sort((a, b) => b.startDate - a.startDate)[0];
		setRotationBuckets(toRotationBuckets(latest));
	} else {
		setRotationBuckets([]);
	}
}

function randomWheelSizeInches(): number {
	const choices = WHEEL_SIZES.map((w) => Number.parseInt(w.value, 10));
	return choices[Math.floor(Math.random() * choices.length)] ?? Number.parseInt(DEFAULT_WHEEL_SIZE, 10);
}

function generateMockDescriptor(): SensorDescriptor {
	const wheelSize = randomWheelSizeInches();
	const tripCount = Math.floor(Math.random() * 3) + 1;
	const trips: SensorDescriptorTrip[] = [];
	const now = Date.now();
	for (let i = 0; i < tripCount; i++) {
		const startDate = now - (i + 1) * 24 * 60 * 60 * 1000;
		const bucketCount = Math.floor(Math.random() * 8) + 4;
		const buckets = Array.from({ length: bucketCount }, () => Math.floor(Math.random() * 60) + 70);
		trips.push({ id: i + 1, startDate, buckets });
	}
	return { wheelSize, trips };
}

export async function scanForSensors(): Promise<SensorScanResult[]> {
	// Mock scan results
	return [
		{ macAddress: 'AA:BB:CC:DD:EE:01', strength: -42, name: 'Bike Sensor A' },
		{ macAddress: 'AA:BB:CC:DD:EE:02', strength: -55, name: 'Bike Sensor B' }
	];
}

export async function getSensorData(macAddress: string): Promise<SensorDescriptor> {
	// Mock descriptor payload; replace with real BLE read later
	void macAddress;
	return generateMockDescriptor();
}

export async function primeSensorsFromScan() {
	const sensors = await scanForSensors();
	if (!sensors.length) return;

	const primary = sensors[0];
	const descriptor = await getSensorData(primary.macAddress);
	applySensorDescriptor(
		{
			id: primary.macAddress,
			name: primary.name,
			signalStrength: primary.strength
		},
		descriptor
	);
}

export async function initializeAppDataFromSensors() {
	if (sensorState.isConnected && tripsState.length > 0) return;
	await primeSensorsFromScan();
}
