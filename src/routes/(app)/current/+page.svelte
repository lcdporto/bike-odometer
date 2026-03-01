<script lang="ts">
	import { goto } from '$app/navigation';
	import { resolve } from '$app/paths';
	import DistanceChart from '$lib/components/DistanceChart.svelte';
	import StatCard from '$lib/components/StatCard.svelte';
	import { initializeAppDataFromSensors } from '$lib/state/app-state';
	import { sensorState } from '$lib/stores/sensor.svelte';
	import { tripsState } from '$lib/stores/trips.svelte';
	import { calculateDistance, formatDistanceKm, formatDurationDhm } from '$lib/utils';
	import { CircleDot, Timer, TrendingUp } from '@lucide/svelte';
	import { onMount } from 'svelte';

	let allBuckets = $derived.by(() => {
		return tripsState.trips
			.flatMap((trip) => trip.buckets)
			.sort((a, b) => a.timestamp - b.timestamp);
	});

	function formatTimelineLabel(timestamp: number): string {
		const date = new Date(timestamp);
		return date.toLocaleString('en-US', {
			month: 'short',
			day: 'numeric'
		});
	}

	let totalRotations = $derived(allBuckets.reduce((sum, bucket) => sum + bucket.rotations, 0));
	let totalDistance = $derived(calculateDistance(totalRotations, sensorState.wheelCircumference));
	let totalMinutes = $derived(allBuckets.length * 5);

	let distanceData = $derived.by(() => {
		let cumulativeRotations = 0;
		return allBuckets.map((bucket) => {
			cumulativeRotations += bucket.rotations;
			const distance = calculateDistance(cumulativeRotations, sensorState.wheelCircumference);
			return {
				time: formatTimelineLabel(bucket.timestamp),
				distance: Number.parseFloat(distance.toFixed(2))
			};
		});
	});

	onMount(() => {
		initializeAppDataFromSensors();
	});

	$effect(() => {
		if (!sensorState.isConnected) {
			goto(resolve('/pairing'));
		}
	});
</script>

<div class="flex h-full flex-col gap-4">
	<div class="grid grid-cols-2 gap-3">
		<StatCard
			title="Distance"
			value="{formatDistanceKm(totalDistance)} km"
			icon={TrendingUp}
			variant="primary"
			iconAlign="center"
		/>
		<StatCard title="Duration" value={formatDurationDhm(totalMinutes)} icon={Timer} variant="default" iconAlign="center" />
	</div>

	<div class="flex-1 min-h-0">
		<DistanceChart data={distanceData} class="h-full" />
	</div>

	<div class="flex items-center justify-center gap-2 text-xs text-muted-foreground">
		<CircleDot class="h-3 w-3" />
		<span>{totalRotations.toLocaleString()} rotations</span>
	</div>
</div>
