<script lang="ts">
	import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '$lib/components/ui/card';
	import { CircleDot, Timer, TrendingUp } from '@lucide/svelte';
	import StatCard from '$lib/components/StatCard.svelte';
	import DistanceChart from '$lib/components/DistanceChart.svelte';
	import { sensorState, type RotationBucket } from '$lib/stores/sensor.svelte';
	import { onMount } from 'svelte';
	import { goto } from '$app/navigation';
	import { DEFAULT_WHEEL_SIZE } from '$lib/config/wheels';
	import { getWheelCircumference, initializeAppDataFromSensors } from '$lib/state/app-state';

	let rotationBuckets: RotationBucket[] = $state([]);
	let wheelSize: string = $state(DEFAULT_WHEEL_SIZE);
	let isConnected: boolean = false;

	let wheelCircumference = $derived(getWheelCircumference(wheelSize));
	let totalRotations = $derived(rotationBuckets.reduce((sum, bucket) => sum + bucket.rotations, 0));
	let totalDistance = $derived((totalRotations * wheelCircumference) / 1000);
	let totalMinutes = $derived(rotationBuckets.length * 5);

	let distanceData = $derived(rotationBuckets.map((bucket, index) => {
		const cumulativeRotations = rotationBuckets
			.slice(0, index + 1)
			.reduce((sum, b) => sum + b.rotations, 0);
		const distance = (cumulativeRotations * wheelCircumference) / 1000;
		return {
			time: bucket.time,
			distance: Number.parseFloat(distance.toFixed(2))
		};
	}));

	onMount(() => {
			initializeAppDataFromSensors();
	});

	$effect(() => {
		isConnected = sensorState.isConnected;
		rotationBuckets = sensorState.rotationBuckets;
		wheelSize = sensorState.wheelSize;

		if (!sensorState.isConnected) {
			goto('/pairing');
		}
	});
</script>

<div class="flex h-full flex-col gap-4">
	<div class="grid grid-cols-3 gap-3">
		<StatCard
			title="Distance"
			value="{totalDistance.toFixed(2)} km"
			subtitle="{(totalDistance * 0.621371).toFixed(2)} mi"
			icon={TrendingUp}
			variant="primary"
			class="col-span-2"
		/>
		<StatCard title="Duration" value="{totalMinutes}m" icon={Timer} variant="default" />
	</div>

	<div class="flex-1 min-h-0">
		<DistanceChart data={distanceData} class="h-full" />
	</div>

	<div class="flex items-center justify-center gap-2 text-xs text-muted-foreground">
		<CircleDot class="h-3 w-3" />
		<span>{totalRotations.toLocaleString()} rotations</span>
	</div>
</div>
