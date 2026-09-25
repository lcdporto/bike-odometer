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

function startOfLocalDay(date: Date): Date {
	return new Date(date.getFullYear(), date.getMonth(), date.getDate());
}

function localDateKey(date: Date): string {
	return `${date.getFullYear()}-${date.getMonth()}-${date.getDate()}`;
}

function localMonthKey(date: Date): string {
	return `${date.getFullYear()}-${date.getMonth()}`;
}

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

export function getTripDayBounds(trips: Trip[]): { first: Date; last: Date } | null {
	const buckets = getSortedTripBuckets(trips);
	if (buckets.length === 0) return null;

	return {
		first: startOfLocalDay(new Date(buckets[0].timestamp)),
		last: startOfLocalDay(new Date(buckets[buckets.length - 1].timestamp))
	};
}

export function getDailyDistanceData(
	trips: Trip[],
	wheelCircumference: number,
	startDate: Date,
	endDate: Date
): DistanceDataPoint[] {
	const rotationsByDay = new Map<string, number>();

	for (const bucket of getSortedTripBuckets(trips)) {
		const date = new Date(bucket.timestamp);
		const key = localDateKey(date);
		rotationsByDay.set(key, (rotationsByDay.get(key) ?? 0) + bucket.rotations);
	}

	const points: DistanceDataPoint[] = [];
	const cursor = startOfLocalDay(startDate);
	const end = startOfLocalDay(endDate);

	while (cursor <= end) {
		const rotations = rotationsByDay.get(localDateKey(cursor)) ?? 0;
		points.push({
			time: formatTimelineLabel(cursor.getTime()),
			distance: Number.parseFloat(
				calculateDistance(rotations, wheelCircumference).toFixed(2)
			)
		});
		cursor.setDate(cursor.getDate() + 1);
	}

	return points;
}

export function getMonthlyDistanceData(
	trips: Trip[],
	wheelCircumference: number,
	endDate: Date,
	monthCount = 12
): DistanceDataPoint[] {
	const rotationsByMonth = new Map<string, number>();

	for (const bucket of getSortedTripBuckets(trips)) {
		const date = new Date(bucket.timestamp);
		const key = localMonthKey(date);
		rotationsByMonth.set(key, (rotationsByMonth.get(key) ?? 0) + bucket.rotations);
	}

	const endMonth = new Date(endDate.getFullYear(), endDate.getMonth(), 1);
	const cursor = new Date(endMonth.getFullYear(), endMonth.getMonth() - monthCount + 1, 1);
	const points: DistanceDataPoint[] = [];

	for (let index = 0; index < monthCount; index++) {
		const rotations = rotationsByMonth.get(localMonthKey(cursor)) ?? 0;
		points.push({
			time: cursor.toLocaleDateString('en-US', { month: 'short' }),
			distance: Number.parseFloat(
				calculateDistance(rotations, wheelCircumference).toFixed(2)
			)
		});
		cursor.setMonth(cursor.getMonth() + 1);
	}

	return points;
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
