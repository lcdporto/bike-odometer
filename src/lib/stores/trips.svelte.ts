import type { RotationBucket } from '$lib/stores/sensor.svelte';

export interface Trip {
	id: string;
	date: string;
	startTime: string;
	endTime: string;
	distance: number;
	duration: number;
	avgSpeed: number;
	totalRotations: number;
	buckets: RotationBucket[];
}

class TripsStore {
	trips = $state<Trip[]>([]);

	// Derived values
	count = $derived(this.trips.length);

	// Actions
	setTrips(trips: Trip[]) {
		this.trips = trips;
	}

	addTrip(trip: Trip) {
		this.trips = [trip, ...this.trips];
	}

	getTripById(id: string): Trip | undefined {
		return this.trips.find((t) => t.id === id);
	}
}

export const tripsState = new TripsStore();
