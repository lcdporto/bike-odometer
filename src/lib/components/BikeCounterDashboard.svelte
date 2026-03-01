<script lang="ts">
	import { Select, SelectContent, SelectItem, SelectTrigger } from '$lib/components/ui/select';
	import { WHEEL_SIZES } from '$lib/config/wheels';
	import { initializeAppDataFromSensors } from '$lib/state/app-state';
	import type { Sensor } from '$lib/stores/sensor.svelte';
	import { sensorState } from '$lib/stores/sensor.svelte';
	import type { Trip } from '$lib/stores/trips.svelte';
	import { tripsState } from '$lib/stores/trips.svelte';
	import { calculateDistance, formatDistanceKm, getWheelCircumference } from '$lib/utils';
	import { BarChart3, Bike, Bluetooth, CircleDot, History, Timer, TrendingUp } from '@lucide/svelte';
	import DistanceChart from './DistanceChart.svelte';
	import SensorSelector from './SensorSelector.svelte';
	import StatCard from './StatCard.svelte';
	import TripDetail from './TripDetail.svelte';
	import TripHistory from './TripHistory.svelte';
	
	type View = 'pairing' | 'current' | 'history' | 'trip-detail';
	
	let view = $state<View>('pairing');
	let selectedTrip = $state<Trip | null>(null);
	
	let wheelCircumference = $derived(getWheelCircumference(sensorState.wheelSize));
	
	// Calculate totals from ALL trips, not just rotation buckets
	let allBuckets = $derived.by(() => {
		return tripsState.trips.flatMap(trip => trip.buckets);
	});
	
	let totalRotations = $derived(allBuckets.reduce((sum, bucket) => sum + bucket.rotations, 0));
	let totalDistance = $derived(calculateDistance(totalRotations, wheelCircumference));
	let totalMinutes = $derived(allBuckets.length * 5);
	
	// Log state for debugging
	$effect(() => {
		console.log('BikeCounterDashboard state:', {
			isConnected: sensorState.isConnected,
			sensor: sensorState.connectedSensor?.name,
			wheelSize: sensorState.wheelSize,
			tripCount: tripsState.count,
			bucketCount: allBuckets.length,
			totalRotations,
			totalDistance,
			totalMinutes
		});
	});
	
	// Calculate cumulative distance data for the chart from all trips
	let cumulativeDistanceData = $derived.by(() => {
		let cumulative = 0;
		const allBucketsWithTrip = tripsState.trips.flatMap(trip => 
			trip.buckets.map(bucket => ({
				...bucket,
				tripId: trip.id
			}))
		).sort((a, b) => a.timestamp - b.timestamp); // Sort by timestamp to ensure chronological order
		
		return allBucketsWithTrip.map((bucket) => {
				const distanceThisBucket = calculateDistance(bucket.rotations, sensorState.wheelCircumference);
			cumulative += distanceThisBucket;
			return {
				time: bucket.time,
				distance: cumulative
			};
		});
	});
	
	// Initialize app data from sensors on mount
	$effect(() => {
		initializeAppDataFromSensors();
	});
	
	function handleTripSelect(trip: Trip) {
		selectedTrip = trip;
		view = 'trip-detail';
	}
	
	function handleSensorConnect(sensor: Sensor) {
		view = 'current';
	}
	
	function handleDisconnect() {
		sensorState.disconnectSensor();
		view = 'pairing';
	}
	
	function handleWheelSizeChange(newSize: string) {
		sensorState.setWheelSize(newSize);
	}
</script>

{#if view === 'pairing'}
	<SensorSelector onConnect={handleSensorConnect} />
{:else}
	<div class="flex h-dvh flex-col bg-background">
		<!-- Header -->
		<header class="flex items-center justify-between gap-2 border-b border-border px-4 py-3">
			<div class="flex items-center gap-2">
				<div class="flex h-9 w-9 items-center justify-center rounded-lg bg-primary/10">
					<Bike class="h-5 w-5 text-primary" />
				</div>
				<div>
					<h1 class="text-base font-bold tracking-tight text-foreground">Bike Counter</h1>
					<button
						onclick={handleDisconnect}
						class="flex items-center gap-1.5 text-xs text-primary hover:underline"
					>
						<Bluetooth class="h-3 w-3" />
						{sensorState.connectedSensor?.name || 'Unknown'}
					</button>
				</div>
			</div>
			
			<Select type="single" value={sensorState.wheelSize} onValueChange={handleWheelSizeChange}>
				<SelectTrigger class="w-[100px] bg-card text-sm">
					{WHEEL_SIZES.find(s => s.value === sensorState.wheelSize)?.label || 'Wheel'}
				</SelectTrigger>
				<SelectContent>
					{#each WHEEL_SIZES as size (size.value)}
						<SelectItem value={size.value}>{size.label}</SelectItem>
					{/each}
				</SelectContent>
			</Select>
		</header>
		
		<main class="flex-1 overflow-hidden p-4">
			{#if view === 'current'}
				<div class="flex h-full flex-col gap-4">
					<div class="grid grid-cols-3 gap-3">
						<StatCard
							title="Distance"
							value="{formatDistanceKm(totalDistance)} km"
							icon={TrendingUp}
							variant="primary"
							class="col-span-2"
						/>
						<StatCard title="Duration" value="{totalMinutes}m" icon={Timer} variant="default" />
					</div>
					
					<div class="flex-1 min-h-0">
						<DistanceChart data={cumulativeDistanceData} class="h-full" />
					</div>
					
					<div class="flex items-center justify-center gap-2 text-xs text-muted-foreground">
						<CircleDot class="h-3 w-3" />
						<span>{totalRotations.toLocaleString()} rotations</span>
					</div>
				</div>
			{:else if view === 'history'}
				<TripHistory trips={tripsState.trips} onTripSelect={handleTripSelect} />
			{:else if view === 'trip-detail' && selectedTrip}
				<TripDetail
					trip={selectedTrip}
					wheelCircumference={wheelCircumference}
					onBack={() => (view = 'history')}
				/>
			{/if}
		</main>
		
		<nav class="flex items-center justify-around border-t border-border bg-card px-4 py-2">
			<button
				onclick={() => (view = 'current')}
				class="flex flex-col items-center gap-1 px-4 py-1.5 rounded-lg transition-colors {view ===
				'current'
					? 'text-primary bg-primary/10'
					: 'text-muted-foreground'}"
			>
				<BarChart3 class="h-5 w-5" />
				<span class="text-xs font-medium">Current</span>
			</button>
			<button
				onclick={() => (view = 'history')}
				class="flex flex-col items-center gap-1 px-4 py-1.5 rounded-lg transition-colors {view ===
					'history' || view === 'trip-detail'
					? 'text-primary bg-primary/10'
					: 'text-muted-foreground'}"
			>
				<History class="h-5 w-5" />
				<span class="text-xs font-medium">History</span>
			</button>
		</nav>
	</div>
{/if}
