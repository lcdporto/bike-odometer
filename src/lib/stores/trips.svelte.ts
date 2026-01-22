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

export const tripsState: Trip[] = $state([]);

export function setTrips(trips: Trip[]) {
	tripsState.length = 0;
	tripsState.push(...trips);
}

export function addTrip(trip: Trip) {
	tripsState.unshift(trip);
}

export function getTripById(id: string): Trip | undefined {
	return tripsState.find((t) => t.id === id);
}
