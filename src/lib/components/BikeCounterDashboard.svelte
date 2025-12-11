<script lang="ts">
	import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '$lib/components/ui/card';
	import { Select, SelectContent, SelectItem, SelectTrigger } from '$lib/components/ui/select';
	import { BarChart3, Bike, Bluetooth, CircleDot, History, Timer, TrendingUp } from '@lucide/svelte';
	import SensorSelector from './SensorSelector.svelte';
	import StatCard from './StatCard.svelte';
	import TripDetail from './TripDetail.svelte';
	import type { Trip } from '$lib/stores/trips';
	import TripHistory from './TripHistory.svelte';
	import { WHEEL_SIZES, DEFAULT_WHEEL_SIZE } from '$lib/config/wheels';
	
	interface Sensor {
		id: string;
		name: string;
		signalStrength: number;
	}
	
	interface RotationBucket {
		time: string;
		rotations: number;
		timestamp: number;
	}
	
	type View = 'pairing' | 'current' | 'history' | 'trip-detail';
	
	let wheelSize = $state(DEFAULT_WHEEL_SIZE);
	let rotationBuckets = $state<RotationBucket[]>([]);
	let isConnected = $state(false);
	let connectedSensor = $state<Sensor | null>(null);
	let view = $state<View>('pairing');
	let selectedTrip = $state<Trip | null>(null);
	
	let wheelCircumference = $derived((Number.parseInt(wheelSize) * Math.PI) / 1000);
	let totalRotations = $derived(rotationBuckets.reduce((sum, bucket) => sum + bucket.rotations, 0));
	let totalDistance = $derived((totalRotations * wheelCircumference) / 1000);
	let totalMinutes = $derived(rotationBuckets.length * 5);
	
	let tripHistory = $derived(generateMockTripHistory(wheelCircumference));

	function generateMockRotations() {
		const baseRotations = Math.floor(Math.random() * 50) + 80;
		const variation = Math.floor(Math.random() * 40) - 20;
		return Math.max(0, baseRotations + variation);
	}
	
	function generateMockTripHistory(wheelCircumference: number): Trip[] {
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
					time: new Date(timestamp).toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit' }),
					rotations,
					timestamp
				});
			}
			
			const totalRotations = buckets.reduce((sum, b) => sum + b.rotations, 0);
			const distance = (totalRotations * wheelCircumference) / 1000;
			const duration = bucketCount * 5;
			const avgSpeed = (distance / duration) * 60;
			
			const startDate = new Date(tripStart);
			trips.push({
				id: `trip-${t}`,
				date: startDate.toLocaleDateString('en-US', { weekday: 'short', month: 'short', day: 'numeric' }),
				startTime: startDate.toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit' }),
				endTime: new Date(tripStart + duration * 60 * 1000).toLocaleTimeString('en-US', {
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
	
	$effect(() => {
		if (!isConnected) return;
		
		const initialBuckets: RotationBucket[] = [];
		const now = Date.now();
		
		for (let i = 5; i >= 0; i--) {
			const timestamp = now - i * 5 * 60 * 1000;
			const date = new Date(timestamp);
			initialBuckets.push({
				time: date.toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit' }),
				rotations: generateMockRotations(),
				timestamp
			});
		}
		
		rotationBuckets = initialBuckets;
		
		const interval = setInterval(() => {
			const timestamp = Date.now();
			const date = new Date(timestamp);
			const newRotations = generateMockRotations();
			
			rotationBuckets = [
				...rotationBuckets.slice(-11),
				{
					time: date.toLocaleTimeString('en-US', { hour: '2-digit', minute: '2-digit' }),
					rotations: newRotations,
					timestamp
				}
			];
		}, 3000);
		
		return () => clearInterval(interval);
	});
	
	function handleTripSelect(trip: Trip) {
		selectedTrip = trip;
		view = 'trip-detail';
	}
	
	function handleSensorConnect(sensor: Sensor) {
		connectedSensor = sensor;
		isConnected = true;
		view = 'current';
	}
	
	function handleDisconnect() {
		isConnected = false;
		connectedSensor = null;
		view = 'pairing';
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
						{connectedSensor?.name}
					</button>
				</div>
			</div>
			
			<Select type="single" bind:value={wheelSize}>
				<SelectTrigger class="w-[100px] bg-card text-sm">
					{WHEEL_SIZES.find(s => s.value === wheelSize)?.label || 'Wheel'}
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
							value="{totalDistance.toFixed(2)} km"
							subtitle="{(totalDistance * 0.621371).toFixed(2)} mi"
							icon={TrendingUp}
							variant="primary"
							class="col-span-2"
						/>
						<StatCard title="Duration" value="{totalMinutes}m" icon={Timer} variant="default" />
					</div>
					
					<div class="flex-1 min-h-0">
						<Card class="flex flex-col bg-card border-border h-full">
							<CardHeader class="pb-2">
								<CardTitle class="text-sm text-foreground">Cumulative Distance</CardTitle>
								<CardDescription class="text-xs text-muted-foreground">Total distance over time (km)</CardDescription>
							</CardHeader>
							<CardContent class="flex-1 min-h-0 pb-4 flex items-center justify-center">
								<p class="text-muted-foreground text-sm">Chart placeholder</p>
							</CardContent>
						</Card>
					</div>
					
					<div class="flex items-center justify-center gap-2 text-xs text-muted-foreground">
						<CircleDot class="h-3 w-3" />
						<span>{totalRotations.toLocaleString()} rotations</span>
					</div>
				</div>
			{:else if view === 'history'}
				<TripHistory trips={tripHistory} onTripSelect={handleTripSelect} />
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
