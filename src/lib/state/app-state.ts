import { loadLatestSensorWithTrips } from '$lib/persistence/sqlite';
import { connectSensor, setRotationBuckets, setWheelSize, sensorState } from '$lib/stores/sensor.svelte';
import { setTrips, tripsState } from '$lib/stores/trips.svelte';

export function getWheelCircumference(size: string): number {
	const parsed = Number.parseInt(size, 10);
	return Number.isFinite(parsed) ? (parsed * Math.PI) / 1000 : 0;
}

/**
 * Load the most recently seen sensor from the database and apply to state
 */
export async function initializeAppDataFromSensors() {
	// Don't re-initialize if already connected
	if (sensorState.isConnected && tripsState.length > 0) return;
	
	try {
		const data = await loadLatestSensorWithTrips();
		
		if (!data) {
			console.log('No sensors found in database');
			return;
		}
		
		// Apply to state
		setWheelSize(data.wheelSize);
		connectSensor(data.sensor);
		setTrips(data.trips);
		
		// Set rotation buckets from latest trip
		if (data.trips.length > 0) {
			const latestTrip = data.trips[0];
			setRotationBuckets(latestTrip.buckets);
		} else {
			setRotationBuckets([]);
		}
		
		console.log('Initialized app data from latest sensor:', data.sensor.name);
	} catch (error) {
		console.error('Failed to initialize app data:', error);
	}
}
