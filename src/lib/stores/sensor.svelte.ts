import { DEFAULT_WHEEL_SIZE } from '$lib/config/wheels';

export interface Sensor {
	id: string;
	name: string;
	signalStrength: number;
}

export interface RotationBucket {
	time: string;
	rotations: number;
	timestamp: number;
}

interface SensorState {
	isConnected: boolean;
	connectedSensor: Sensor | null;
	rotationBuckets: RotationBucket[];
	wheelSize: string;
}

export const sensorState: SensorState = $state({
	isConnected: false,
	connectedSensor: null,
	rotationBuckets: [],
	wheelSize: DEFAULT_WHEEL_SIZE
});

export function connectSensor(sensor: Sensor) {
	sensorState.isConnected = true;
	sensorState.connectedSensor = sensor;
	// Don't clear rotation buckets here - let setRotationBuckets handle it
}

export function disconnectSensor() {
	sensorState.isConnected = false;
	sensorState.connectedSensor = null;
	sensorState.rotationBuckets = [];
	sensorState.wheelSize = DEFAULT_WHEEL_SIZE;
}

export function addRotationBucket(bucket: RotationBucket) {
	sensorState.rotationBuckets = [...sensorState.rotationBuckets.slice(-11), bucket];
}

export function setRotationBuckets(buckets: RotationBucket[]) {
	sensorState.rotationBuckets = buckets;
}

export function setWheelSize(size: string) {
	sensorState.wheelSize = size;
}
