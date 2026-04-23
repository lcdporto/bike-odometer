import type { RotationBucket } from '$lib/stores/sensor.svelte';
import type { Trip } from '$lib/stores/trips.svelte';
import { calculateDistance, formatTime24h } from '$lib/utils';

const FIVE_MINUTES_MS = 5 * 60 * 1000;

export type SensorTripDescriptor = {
	id: number;
	startDate: number;
	buckets: number[];
};

export type DistanceDataPoint = {
	time: string;
	distance: number;
};

function formatTripDate(date: Date): string {
	return date.toLocaleDateString('en-US', {
		weekday: 'short',
		month: 'short',
		day: 'numeric'
	});
}

function formatTimelineLabel(timestamp: number): string {
	return new Date(timestamp).toLocaleString('en-US', {
		month: 'short',
		day: 'numeric'
	});
}

export function getSortedTripBuckets(trips: Trip[]): RotationBucket[] {
	return trips
		.flatMap((trip) => trip.buckets)
		.sort((a, b) => a.timestamp - b.timestamp);
}

export function getTripTotals(trips: Trip[], wheelCircumference: number) {
	const buckets = getSortedTripBuckets(trips);
	const totalRotations = buckets.reduce((sum, bucket) => sum + bucket.rotations, 0);

	return {
		totalRotations,
		totalDistance: calculateDistance(totalRotations, wheelCircumference),
		totalMinutes: buckets.length * 5
	};
}

export function getCumulativeDistanceData(
	trips: Trip[],
	wheelCircumference: number
): DistanceDataPoint[] {
	let cumulativeRotations = 0;

	return getSortedTripBuckets(trips).map((bucket) => {
		cumulativeRotations += bucket.rotations;

		return {
			time: formatTimelineLabel(bucket.timestamp),
			distance: Number.parseFloat(
				calculateDistance(cumulativeRotations, wheelCircumference).toFixed(2)
			)
		};
	});
}

export function getBucketDistanceData(trip: Trip, wheelCircumference: number): DistanceDataPoint[] {
	return trip.buckets.map((bucket) => ({
		time: bucket.time,
		distance: Number.parseFloat(
			(calculateDistance(bucket.rotations, wheelCircumference) * 1000).toFixed(0)
		)
	}));
}

export function tripFromSensorDescriptor(
	descriptor: SensorTripDescriptor,
	wheelCircumference: number
): Trip {
	const startDate = new Date(descriptor.startDate * 1000);
	const buckets = descriptor.buckets.map((rotations, idx) => {
		const timestamp = startDate.getTime() + idx * FIVE_MINUTES_MS;
		return {
			time: formatTime24h(new Date(timestamp)),
			rotations,
			timestamp
		};
	});
	const totalRotations = buckets.reduce((sum, bucket) => sum + bucket.rotations, 0);
	const distance = calculateDistance(totalRotations, wheelCircumference);
	const duration = buckets.length * 5;
	const endDate = new Date(startDate.getTime() + duration * 60 * 1000);

	return {
		id: `trip-${descriptor.id}`,
		date: formatTripDate(startDate),
		startTime: formatTime24h(startDate),
		endTime: formatTime24h(endDate),
		distance,
		duration,
		avgSpeed: duration > 0 ? (distance / duration) * 60 : 0,
		totalRotations,
		buckets
	};
}
