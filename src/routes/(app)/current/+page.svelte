<script lang="ts">
	import DistanceChart from '$lib/components/DistanceChart.svelte';
	import StatCard from '$lib/components/StatCard.svelte';
	import { getCumulativeDistanceData, getTripTotals } from '$lib/domain/trips';
	import { sensorState } from '$lib/stores/sensor.svelte';
	import { tripsState } from '$lib/stores/trips.svelte';
	import { formatDistanceKm, formatDurationDhm } from '$lib/utils';
	import { CircleDot, Timer, TrendingUp } from '@lucide/svelte';

	let totals = $derived(getTripTotals(tripsState.trips, sensorState.wheelCircumference));
	let distanceData = $derived(getCumulativeDistanceData(tripsState.trips, sensorState.wheelCircumference));
</script>

<div class="flex h-full flex-col gap-4">
	<div class="grid grid-cols-2 gap-3">
		<StatCard
			title="Distance"
			value="{formatDistanceKm(totals.totalDistance)} km"
			icon={TrendingUp}
			variant="primary"
			iconAlign="center"
		/>
		<StatCard title="Duration" value={formatDurationDhm(totals.totalMinutes)} icon={Timer} variant="default" iconAlign="center" />
	</div>

	<div class="flex-1 min-h-0">
		<DistanceChart data={distanceData} class="h-full" />
	</div>

	<div class="flex items-center justify-center gap-2 text-xs text-muted-foreground">
		<CircleDot class="h-3 w-3" />
		<span>{totals.totalRotations.toLocaleString()} rotations</span>
	</div>
</div>
