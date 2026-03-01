import { loadLatestSensorWithTrips } from '$lib/persistence/sqlite';
import { sensorState } from '$lib/stores/sensor.svelte';
import { tripsState } from '$lib/stores/trips.svelte';
import { getWheelCircumference } from '$lib/utils';

export { getWheelCircumference };

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
		
		// Apply to state
		sensorState.setWheelSize(data.wheelSize);
		sensorState.connectSensor(data.sensor);
		tripsState.setTrips(data.trips);
		
		// Set rotation buckets from latest trip
		if (data.trips.length > 0) {
			const latestTrip = data.trips[0];
			sensorState.setRotationBuckets(latestTrip.buckets);
		} else {
			sensorState.setRotationBuckets([]);
		}
		
		console.log('Initialized app data from latest sensor:', data.sensor.name);
	} catch (error) {
		console.error('Failed to initialize app data:', error);
	}
}
