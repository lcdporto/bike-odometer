import { getSensorWithTrips, loadLatestSensorWithTrips } from '$lib/persistence/sqlite';
import type { Sensor } from '$lib/stores/sensor.svelte';
import { sensorState } from '$lib/stores/sensor.svelte';
import { tripsState } from '$lib/stores/trips.svelte';
import { getWheelCircumference } from '$lib/utils';

export { getWheelCircumference };

type SensorWithTrips = NonNullable<Awaited<ReturnType<typeof loadLatestSensorWithTrips>>>;

export function applySensorWithTrips(data: SensorWithTrips, sensor: Sensor = data.sensor) {
	sensorState.setWheelSize(data.wheelSize);
	sensorState.connectSensor(sensor);
	tripsState.setTrips(data.trips);
}

/**
 * Load the most recently seen sensor from the database and apply to state
 */
export async function initializeAppDataFromSensors() {
	// Don't re-initialize if already connected
	if (sensorState.isConnected && tripsState.count > 0) return;
	
	try {
		const data = await loadLatestSensorWithTrips();
		
		if (!data) {
			console.log('No sensors found in database');
			return;
		}
		
		applySensorWithTrips(data);
		
		console.log('Initialized app data from latest sensor:', data.sensor.name);
	} catch (error) {
		console.error('Failed to initialize app data:', error);
	}
}

export async function loadAndApplySensorData(sensor: Sensor) {
	const data = await getSensorWithTrips(sensor.id);

	if (!data) {
		throw new Error('Sensor data not found in database');
	}

	applySensorWithTrips(data, sensor);
	return data;
}
