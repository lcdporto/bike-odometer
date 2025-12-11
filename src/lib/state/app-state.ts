import { get } from 'svelte/store';
import { sensorStore, type RotationBucket, type Sensor } from '$lib/stores/sensor';
import { tripsStore, generateMockTripHistory } from '$lib/stores/trips';
import { DEFAULT_WHEEL_SIZE } from '$lib/config/wheels';

const MOCK_SENSOR: Sensor = {
	id: 'mock-sensor',
	name: 'Bike Sensor',
	signalStrength: 90
};

export function getWheelCircumference(size: string): number {
	const parsed = Number.parseInt(size, 10);
	return Number.isFinite(parsed) ? (parsed * Math.PI) / 1000 : 0;
}

function generateMockRotations() {
	const baseRotations = Math.floor(Math.random() * 50) + 80;
	const variation = Math.floor(Math.random() * 40) - 20;
	return Math.max(0, baseRotations + variation);
}

function generateMockRotationBuckets(count = 6): RotationBucket[] {
	const buckets: RotationBucket[] = [];
	const now = Date.now();
	for (let i = count; i >= 1; i--) {
		const timestamp = now - i * 5 * 60 * 1000;
		const date = new Date(timestamp);
		buckets.push({
			time: date.toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit' }),
			rotations: generateMockRotations(),
			timestamp
		});
	}
	return buckets;
}

export function ensureMockSensorData(wheelSize: string = DEFAULT_WHEEL_SIZE) {
	const state = get(sensorStore);
	if (!state.isConnected) {
		sensorStore.connect(MOCK_SENSOR);
	}
	if (state.wheelSize !== wheelSize) {
		sensorStore.setWheelSize(wheelSize);
	}
	if (state.rotationBuckets.length === 0) {
		sensorStore.setRotationBuckets(generateMockRotationBuckets());
	}
}

export function ensureMockTripHistory(wheelSize: string = DEFAULT_WHEEL_SIZE) {
	const trips = get(tripsStore);
	if (trips.length > 0) return;
	const wheelCircumference = getWheelCircumference(wheelSize);
	tripsStore.setTrips(generateMockTripHistory(wheelCircumference));
}

export function initializeMockAppState(wheelSize: string = DEFAULT_WHEEL_SIZE) {
	ensureMockSensorData(wheelSize);
	ensureMockTripHistory(wheelSize);
}
