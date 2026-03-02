import { getDashboardData } from '$lib/server/db';

export const load = async () => {
	const sensors = getDashboardData();

	const summary = {
		sensors: sensors.length,
		trips: sensors.reduce((total, sensor) => total + sensor.trips.length, 0),
		buckets: sensors.reduce(
			(total, sensor) =>
				total + sensor.trips.reduce((tripTotal, trip) => tripTotal + trip.buckets.length, 0),
			0
		)
	};

	return {
		summary,
		sensors,
		generatedAt: Date.now()
	};
};
