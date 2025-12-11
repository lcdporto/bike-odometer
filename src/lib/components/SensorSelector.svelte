<script lang="ts">
	import { Button } from '$lib/components/ui/button';
	import type { Sensor } from '$lib/stores/sensor';
	import { Bluetooth, BluetoothSearching, Loader2, Signal, SignalLow, SignalMedium } from '@lucide/svelte';
	
	interface Props {
		onConnect: (sensor: Sensor) => void;
	}
	
	let { onConnect }: Props = $props();
	
	const MOCK_SENSORS = [
		{ id: 'sensor-1', name: 'BC-001 Front', signalStrength: 92 },
		{ id: 'sensor-2', name: 'BC-002 Rear', signalStrength: 78 },
		{ id: 'sensor-3', name: 'BC-003 City Bike', signalStrength: 45 },
		{ id: 'sensor-4', name: 'BC-004 MTB', signalStrength: 61 }
	] satisfies Sensor[];
	
	let isScanning = $state(false);
	let sensors = $state<Sensor[]>(MOCK_SENSORS);
	let connectingId = $state<string | null>(null);
	
	let sortedSensors = $derived(
		[...sensors].sort((a, b) => b.signalStrength - a.signalStrength)
	);
	
	function handleConnect(sensor: Sensor) {
		connectingId = sensor.id;
		setTimeout(() => {
			onConnect(sensor);
		}, 1500);
	}
	
	function handleRescan() {
		isScanning = true;
		sensors = [];
		setTimeout(() => {
			isScanning = false;
			sensors = MOCK_SENSORS.map((s) => ({
				...s,
				signalStrength: Math.max(20, Math.min(100, s.signalStrength + Math.floor(Math.random() * 20) - 10))
			}));
		}, 2000);
	}
	
	function SignalIcon({ strength }: { strength: number }) {
		if (strength >= 75) return Signal;
		if (strength >= 50) return SignalMedium;
		return SignalLow;
	}
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
		{#if isScanning}
			<div class="flex flex-1 flex-col items-center justify-center gap-4">
				<div class="relative">
					<BluetoothSearching class="h-16 w-16 text-primary animate-pulse" />
					<span class="absolute -right-1 -top-1 flex h-4 w-4">
						<span class="absolute inline-flex h-full w-full animate-ping rounded-full bg-primary opacity-75"></span>
						<span class="relative inline-flex h-4 w-4 rounded-full bg-primary"></span>
					</span>
				</div>
				<p class="text-sm text-muted-foreground">Scanning for nearby sensors...</p>
			</div>
		{:else if sensors.length === 0}
			<div class="flex flex-1 flex-col items-center justify-center gap-4">
				<Bluetooth class="h-16 w-16 text-muted-foreground/50" />
				<p class="text-sm text-muted-foreground">No sensors found</p>
				<Button variant="outline" onclick={handleRescan}>Scan Again</Button>
			</div>
		{:else}
			<div class="flex flex-1 flex-col gap-3">
				<div class="flex items-center justify-between">
					<p class="text-sm text-muted-foreground">
						{sensors.length} sensor{sensors.length !== 1 ? 's' : ''} found
					</p>
					<Button variant="ghost" size="sm" onclick={handleRescan} class="text-xs">Rescan</Button>
				</div>
				
				<div class="flex flex-col gap-2">
					{#each sortedSensors as sensor (sensor.id)}
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
	</main>
	
	<footer
		style:padding-bottom={"max(env(safe-area-inset-bottom), 12px)"}
		class="border-t border-border px-4 py-3">
		<p class="text-center text-xs text-muted-foreground">
			Make sure your bike sensor is powered on and within range
		</p>
	</footer>
</div>
