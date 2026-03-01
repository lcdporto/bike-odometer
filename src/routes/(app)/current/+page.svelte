<script lang="ts">
	import { goto } from '$app/navigation';
	import { resolve } from '$app/paths';
	import DistanceChart from '$lib/components/DistanceChart.svelte';
	import StatCard from '$lib/components/StatCard.svelte';
	import { initializeAppDataFromSensors } from '$lib/state/app-state';
	import { sensorState } from '$lib/stores/sensor.svelte';
	import { calculateDistance, formatDistanceKm } from '$lib/utils';
	import { CircleDot, Timer, TrendingUp } from '@lucide/svelte';
	import { onMount } from 'svelte';

	let distanceData = $derived(sensorState.rotationBuckets.map((bucket, index) => {
		const cumulativeRotations = sensorState.rotationBuckets
			.slice(0, index + 1)
			.reduce((sum, b) => sum + b.rotations, 0);
		const distance = calculateDistance(cumulativeRotations, sensorState.wheelCircumference);
		return {
			time: bucket.time,
			distance: Number.parseFloat(distance.toFixed(2))
		};
	}));

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
	<div class="grid grid-cols-3 gap-3">
		<StatCard
			title="Distance"
			value="{formatDistanceKm(sensorState.totalDistance)} km"
			icon={TrendingUp}
			variant="primary"
			class="col-span-2"
		/>
		<StatCard title="Duration" value="{sensorState.totalMinutes}m" icon={Timer} variant="default" />
	</div>

	<div class="flex-1 min-h-0">
		<DistanceChart data={distanceData} class="h-full" />
	</div>

	<div class="flex items-center justify-center gap-2 text-xs text-muted-foreground">
		<CircleDot class="h-3 w-3" />
		<span>{sensorState.totalRotations.toLocaleString()} rotations</span>
	</div>
</div>
