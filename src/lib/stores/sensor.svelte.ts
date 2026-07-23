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
	batteryPercent = $state<number | null>(null);
	batteryMillivolts = $state<number | null>(null);

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
		this.batteryPercent = null;
		this.batteryMillivolts = null;
	}

	setWheelSize(size: string) {
		this.wheelSize = size;
	}

	setBatteryInfo(info: { percent: number; millivolts: number }) {
		this.batteryPercent = info.percent;
		this.batteryMillivolts = info.millivolts;
	}
}

export const sensorState = new SensorStore();
