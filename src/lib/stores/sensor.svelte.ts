import { DEFAULT_WHEEL_SIZE } from '$lib/config/wheels';
import { getWheelCircumference, calculateDistance } from '$lib/utils';

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

class SensorStore {
	isConnected = $state(false);
	connectedSensor = $state<Sensor | null>(null);
	rotationBuckets = $state<RotationBucket[]>([]);
	wheelSize = $state(DEFAULT_WHEEL_SIZE);

	// Derived values
	wheelCircumference = $derived(
		this.wheelSize ? getWheelCircumference(this.wheelSize) : 0
	);

	totalRotations = $derived(
		this.rotationBuckets.reduce((sum, bucket) => sum + bucket.rotations, 0)
	);

	totalDistance = $derived(
		calculateDistance(this.totalRotations, this.wheelCircumference)
	);

	totalMinutes = $derived(this.rotationBuckets.length * 5);

	// Actions
	connectSensor(sensor: Sensor) {
		this.isConnected = true;
		this.connectedSensor = sensor;
	}

	disconnectSensor() {
		this.isConnected = false;
		this.connectedSensor = null;
		this.rotationBuckets = [];
		this.wheelSize = DEFAULT_WHEEL_SIZE;
	}

	addRotationBucket(bucket: RotationBucket) {
		this.rotationBuckets.push(bucket);
	}

	setRotationBuckets(buckets: RotationBucket[]) {
		this.rotationBuckets = buckets;
	}

	setWheelSize(size: string) {
		this.wheelSize = size;
	}
}

export const sensorState = new SensorStore();
