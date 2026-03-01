<script lang="ts">
	import { Button } from '$lib/components/ui/button';
	import type { Sensor } from '$lib/stores/sensor.svelte';
	import { Bluetooth, BluetoothSearching, Loader2, Signal, SignalLow, SignalMedium, Clock } from '@lucide/svelte';
	import { getAllSensors, getSensorWithTrips } from '$lib/persistence/sqlite';
	import { sensorState } from '$lib/stores/sensor.svelte';
	import { tripsState } from '$lib/stores/trips.svelte';
	import { bleDevicesState } from '$lib/stores/ble-devices.svelte';
	
	interface Props {
		onConnect: (sensor: Sensor) => void;
	}
	
	let { onConnect }: Props = $props();
	
	let isLoading = $state(false);
	let historicalSensors = $state<Array<Sensor & { lastSeen?: number }>>([]);
	let connectingId = $state<string | null>(null);
	let errorMessage = $state<string | null>(null);
	
	// Get connected devices from store
	let connectedDevices = $derived(bleDevicesState.connectedDevices);
	
	// Filter historical sensors to exclude currently connected ones
	let filteredHistoricalSensors = $derived(
		historicalSensors.filter(
			(sensor) => !connectedDevices.some((device) => device.id === sensor.id)
		)
	);
	
	function formatTimeAgo(timestamp: number): string {
		const now = Date.now();
		const diff = now - timestamp;
		const seconds = Math.floor(diff / 1000);
		const minutes = Math.floor(seconds / 60);
		const hours = Math.floor(minutes / 60);
		const days = Math.floor(hours / 24);
		
		if (days > 0) return `${days}d ago`;
		if (hours > 0) return `${hours}h ago`;
		if (minutes > 0) return `${minutes}m ago`;
		return 'just now';
	}
	
	async function loadHistoricalSensorsFromDB() {
		isLoading = true;
		errorMessage = null;
		
		try {
			const dbSensors = await getAllSensors();
			historicalSensors = dbSensors;
			console.log(`Loaded ${historicalSensors.length} historical sensors from DB`);
		} catch (error) {
			console.error('Failed to load sensors from DB:', error);
			errorMessage = 'Failed to load sensors from database.';
		} finally {
			isLoading = false;
		}
	}
	
	async function handleConnect(sensor: Sensor) {
		connectingId = sensor.id;
		errorMessage = null;
		
		try {
			// Load sensor data from DB
			const sensorData = await getSensorWithTrips(sensor.id);
			
			if (!sensorData) {
				throw new Error('Sensor data not found in database');
			}
			
			console.log('Loaded sensor data:', {
				sensorId: sensor.id,
				wheelSize: sensorData.wheelSize,
				tripCount: sensorData.trips.length,
				latestTripBuckets: sensorData.trips[0]?.buckets.length || 0
			});
			
			// Log all trips retrieved
			console.log('===== TRIPS RETRIEVED =====');
			console.log(`Total trips: ${sensorData.trips.length}`);
			// Calculate wheel circumference in meters from diameter in inches
			const wheelDiameterInches = parseFloat(sensorData.wheelSize);
			const wheelCircumferenceMeters = (wheelDiameterInches * Math.PI * 0.0254);
			console.log(`Wheel size: ${wheelDiameterInches}" (circumference: ${wheelCircumferenceMeters.toFixed(3)} m)`);
			
			sensorData.trips.forEach((trip, tripIndex) => {
				console.log(`\nTrip ${tripIndex + 1}:`, {
					id: trip.id,
					startTime: trip.startTime,
					endTime: trip.endTime,
					totalRotations: trip.totalRotations,
					totalDistance: `${trip.distance.toFixed(3)} km`,
					bucketCount: trip.buckets.length
				});
				
				// Log bucket details with distance and wheel spins
				console.log(`  Buckets for Trip ${tripIndex + 1}:`);
				trip.buckets.forEach((bucket, bucketIndex) => {
					const distanceInBucketMeters = bucket.rotations * wheelCircumferenceMeters;
					console.log(`    Bucket ${bucketIndex + 1}:`, {
						timestamp: new Date(bucket.timestamp).toISOString(),
						wheelSpins: bucket.rotations,
						distance: `${distanceInBucketMeters.toFixed(2)} m (${(distanceInBucketMeters / 1000).toFixed(3)} km)`
					});
				});
			});
			console.log('===========================');
			
			// Apply to state
			sensorState.setWheelSize(sensorData.wheelSize);
			sensorState.connectSensor(sensor);
			tripsState.setTrips(sensorData.trips);
			
			// Set rotation buckets from latest trip
			if (sensorData.trips.length > 0) {
				const latestTrip = sensorData.trips[0];
				sensorState.setRotationBuckets(latestTrip.buckets);
				console.log('Set rotation buckets:', latestTrip.buckets.length, 'buckets');
			} else {
				sensorState.setRotationBuckets([]);
				console.log('No trips found, set empty rotation buckets');
			}
			
			// Call the onConnect callback
			onConnect(sensor);
		} catch (error) {
			console.error('Failed to connect to sensor:', error);
			errorMessage = 'Failed to load sensor data. Please try again.';
			connectingId = null;
		}
	}
	
	async function handleRefresh() {
		await loadHistoricalSensorsFromDB();
	}
	
	// Load historical sensors on mount
	$effect(() => {
		loadHistoricalSensorsFromDB();
	});
</script>

<div class="flex h-dvh flex-col bg-background">
	<header
		style:padding-top={"max(env(safe-area-inset-top), 16px)"}
		class="flex items-center justify-center border-b border-border px-4 py-4">
		<div class="flex items-center gap-3">
			<div class="flex h-10 w-10 items-center justify-center rounded-full bg-primary/10">
				<Bluetooth class="h-5 w-5 text-primary" />
			</div>
			<h1 class="text-lg font-bold text-foreground">Connect Sensor</h1>
		</div>
	</header>
	
	<main class="flex flex-1 flex-col p-4">
		{#if errorMessage}
			<div class="mb-4 rounded-lg border border-red-500/50 bg-red-500/10 p-3">
				<p class="text-sm text-red-500">{errorMessage}</p>
			</div>
		{/if}
		
		{#if isLoading}
			<div class="flex flex-1 flex-col items-center justify-center gap-4">
				<div class="relative">
					<BluetoothSearching class="h-16 w-16 text-primary animate-pulse" />
					<span class="absolute -right-1 -top-1 flex h-4 w-4">
						<span class="absolute inline-flex h-full w-full animate-ping rounded-full bg-primary opacity-75"></span>
						<span class="relative inline-flex h-4 w-4 rounded-full bg-primary"></span>
					</span>
				</div>
				<p class="text-sm text-muted-foreground">Loading sensors...</p>
			</div>
		{:else if connectedDevices.length === 0 && filteredHistoricalSensors.length === 0}
			<div class="flex flex-1 flex-col items-center justify-center gap-4">
				<Bluetooth class="h-16 w-16 text-muted-foreground/50" />
				<p class="text-sm text-muted-foreground">No sensors found</p>
				<Button variant="outline" onclick={handleRefresh}>Refresh</Button>
			</div>
		{:else}
			<div class="flex flex-1 flex-col gap-4 overflow-y-auto">
				<!-- Connected Devices Section -->
				{#if connectedDevices.length > 0}
					<div class="flex flex-col gap-2">
						<div class="flex items-center justify-between">
							<h2 class="text-sm font-semibold text-foreground">Connected Devices</h2>
							<span class="text-xs text-muted-foreground">{connectedDevices.length}</span>
						</div>
						
						<div class="flex flex-col gap-2">
							{#each connectedDevices as sensor (sensor.id)}
								<button
									onclick={() => handleConnect(sensor)}
									disabled={connectingId !== null}
									class="flex items-center gap-3 rounded-xl border border-border bg-card p-4 text-left transition-colors hover:bg-accent disabled:opacity-50"
								>
									<div class="flex h-10 w-10 items-center justify-center rounded-full bg-primary/10">
										<Bluetooth class="h-5 w-5 text-primary" />
									</div>
									<div class="flex-1">
										<p class="font-medium text-foreground">{sensor.name}</p>
										<p class="text-xs text-muted-foreground">Signal: {sensor.signalStrength}%</p>
									</div>
									<div class="flex items-center gap-2">
										{#if sensor.signalStrength >= 75}
											<Signal class="h-4 w-4 text-green-500" />
										{:else if sensor.signalStrength >= 50}
											<SignalMedium class="h-4 w-4 text-yellow-500" />
										{:else}
											<SignalLow class="h-4 w-4 text-orange-500" />
										{/if}
										{#if connectingId === sensor.id}
											<Loader2 class="h-5 w-5 animate-spin text-primary" />
										{:else}
											<div class="h-5 w-5"></div>
										{/if}
									</div>
								</button>
							{/each}
						</div>
					</div>
				{/if}
				
				<!-- Historical Devices Section -->
				{#if filteredHistoricalSensors.length > 0}
					<div class="flex flex-col gap-2">
						<div class="flex items-center justify-between">
							<h2 class="text-sm font-semibold text-foreground">Historical Devices</h2>
							<div class="flex items-center gap-2">
								<span class="text-xs text-muted-foreground">{filteredHistoricalSensors.length}</span>
								<Button variant="ghost" size="sm" onclick={handleRefresh} class="text-xs h-7">Refresh</Button>
							</div>
						</div>
						
						<div class="flex flex-col gap-2">
							{#each filteredHistoricalSensors as sensor (sensor.id)}
								<button
									onclick={() => handleConnect(sensor)}
									disabled={connectingId !== null}
									class="flex items-center gap-3 rounded-xl border border-border bg-card p-4 text-left transition-colors hover:bg-accent disabled:opacity-50"
								>
									<div class="flex h-10 w-10 items-center justify-center rounded-full bg-muted">
										<Bluetooth class="h-5 w-5 text-muted-foreground" />
									</div>
									<div class="flex-1">
										<p class="font-medium text-foreground">{sensor.name}</p>
										<div class="flex items-center gap-1 text-xs text-muted-foreground">
											<Clock class="h-3 w-3" />
											<span>{sensor.lastSeen ? formatTimeAgo(sensor.lastSeen) : 'Unknown'}</span>
										</div>
									</div>
									<div class="flex items-center gap-2">
										{#if connectingId === sensor.id}
											<Loader2 class="h-5 w-5 animate-spin text-primary" />
										{:else}
											<div class="h-5 w-5"></div>
										{/if}
									</div>
								</button>
							{/each}
						</div>
					</div>
				{/if}
			</div>
		{/if}
	</main>
	
	<footer
		style:padding-bottom={"max(env(safe-area-inset-bottom), 12px)"}
		class="border-t border-border px-4 py-3">
		<p class="text-center text-xs text-muted-foreground">
			Sensors are discovered automatically in the background
		</p>
	</footer>
</div>
