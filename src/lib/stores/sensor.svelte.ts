import { DEFAULT_WHEEL_SIZE } from '$lib/config/wheels';
import { getWheelCircumference } from '$lib/utils';

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
	wheelSize = $state(DEFAULT_WHEEL_SIZE);

	// Derived values
	wheelCircumference = $derived(
		this.wheelSize ? getWheelCircumference(this.wheelSize) : 0
	);

	// Actions
	connectSensor(sensor: Sensor) {
		this.isConnected = true;
		this.connectedSensor = sensor;
	}

	disconnectSensor() {
		this.isConnected = false;
		this.connectedSensor = null;
		this.wheelSize = DEFAULT_WHEEL_SIZE;
	}

	setWheelSize(size: string) {
		this.wheelSize = size;
	}
}

export const sensorState = new SensorStore();
