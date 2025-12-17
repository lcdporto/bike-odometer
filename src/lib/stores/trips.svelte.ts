import type { RotationBucket } from '$lib/stores/sensor.svelte';
import { SvelteDate } from 'svelte/reactivity';

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

export function generateMockTripHistory(wheelCircumference: number): Trip[] {
	const trips: Trip[] = [];
	const now = Date.now();

	for (let t = 0; t < 5; t++) {
		const tripStart = now - (t + 1) * 24 * 60 * 60 * 1000 - Math.random() * 12 * 60 * 60 * 1000;
		const bucketCount = Math.floor(Math.random() * 8) + 4;
		const buckets: RotationBucket[] = [];

		for (let i = 0; i < bucketCount; i++) {
			const timestamp = tripStart + i * 5 * 60 * 1000;
			const rotations = Math.floor(Math.random() * 60) + 70;
			buckets.push({
				time: new SvelteDate(timestamp).toLocaleTimeString('en-US', {
					hour: '2-digit',
					minute: '2-digit'
				}),
				rotations,
				timestamp
			});
		}

		const totalRotations = buckets.reduce((sum, b) => sum + b.rotations, 0);
		const distance = (totalRotations * wheelCircumference) / 1000;
		const duration = bucketCount * 5;
		const avgSpeed = (distance / duration) * 60;

		const startDate = new SvelteDate(tripStart);
		trips.push({
			id: `trip-${t}`,
			date: startDate.toLocaleDateString('en-US', {
				weekday: 'short',
				month: 'short',
				day: 'numeric'
			}),
			startTime: startDate.toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit' }),
			endTime: new SvelteDate(tripStart + duration * 60 * 1000).toLocaleTimeString('en-US', {
				hour: '2-digit',
				minute: '2-digit'
			}),
			distance,
			duration,
			avgSpeed,
			totalRotations,
			buckets
		});
	}

	return trips;
}
