import { writable } from 'svelte/store';
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

const initialState: SensorState = {
	isConnected: false,
	connectedSensor: null,
	rotationBuckets: [],
	wheelSize: DEFAULT_WHEEL_SIZE
};

function createSensorStore() {
	const { subscribe, set, update } = writable<SensorState>(initialState);

	return {
		subscribe,
		connect: (sensor: Sensor) => {
			update((state) => ({
				...state,
				isConnected: true,
				connectedSensor: sensor,
				rotationBuckets: []
			}));
		},
		disconnect: () => {
			set(initialState);
		},
		addRotationBucket: (bucket: RotationBucket) => {
			update((state) => ({
				...state,
				rotationBuckets: [...state.rotationBuckets.slice(-11), bucket]
			}));
		},
		setRotationBuckets: (buckets: RotationBucket[]) => {
			update((state) => ({
				...state,
				rotationBuckets: buckets
			}));
		},
		setWheelSize: (size: string) => {
			update((state) => ({
				...state,
				wheelSize: size
			}));
		}
	};
}

export const sensorStore = createSensorStore();
