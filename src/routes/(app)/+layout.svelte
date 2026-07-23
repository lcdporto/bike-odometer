<script lang="ts">
	import { Bike, Bluetooth, BarChart3, Battery, History } from '@lucide/svelte';
	import { Select, SelectContent, SelectItem, SelectTrigger } from '$lib/components/ui/select';
	import { goto } from '$app/navigation';
	import { resolve } from '$app/paths';
	import { page } from '$app/state';
	import { sensorState } from '$lib/stores/sensor.svelte';
	import { bleDevicesState } from '$lib/stores/ble-devices.svelte';
	import type { Sensor } from '$lib/stores/sensor.svelte';
	import { writeWheelSizeDescriptor } from '$lib/services/ble';
	import { saveSensorWheelSize } from '$lib/persistence/sqlite';
	import { initializeAppDataFromSensors } from '$lib/state/app-state';
	import { WHEEL_SIZES, DEFAULT_WHEEL_SIZE } from '$lib/config/wheels';
	import { resolveDisplaySensorName } from '$lib/utils';
	import { onMount } from 'svelte';

	let connectedSensor = $state<Sensor | null>(null);
	let wheelSize = $state(DEFAULT_WHEEL_SIZE);
	let isInitializing = $state(true);
	let liveConnectedDeviceIds = $derived(new Set(bleDevicesState.connectedDevices.map((device) => device.id)));
	let isLiveSensorSelected = $derived(
		connectedSensor ? liveConnectedDeviceIds.has(connectedSensor.id) : false
	);

	let currentPath = $derived(page.url.pathname);
	let isCurrentActive = $derived(currentPath === '/current');
	let isHistoryActive = $derived(currentPath.startsWith('/history'));

	$effect(() => {
		connectedSensor = sensorState.connectedSensor;
		wheelSize = sensorState.wheelSize;
	});

	onMount(async () => {
		await initializeAppDataFromSensors();
		isInitializing = false;
	});

	$effect(() => {
		if (!isInitializing && !sensorState.isConnected) {
			goto(resolve('/pairing'));
		}
	});

	async function handleWheelSizeChange(nextWheelSize: string) {
		if (!nextWheelSize || nextWheelSize === sensorState.wheelSize) {
			return;
		}

		const activeSensor = sensorState.connectedSensor;
		if (!activeSensor) {
			wheelSize = nextWheelSize;
			sensorState.setWheelSize(nextWheelSize);
			return;
		}

		try {
			await writeWheelSizeDescriptor(activeSensor.id, nextWheelSize);
			sensorState.setWheelSize(nextWheelSize);
			wheelSize = nextWheelSize;
			await saveSensorWheelSize(activeSensor, nextWheelSize);
		} catch (error) {
			console.error('Failed to update wheel size descriptor:', error);
			wheelSize = sensorState.wheelSize;
		}
	}

	function handleDisconnect() {
		sensorState.disconnectSensor();
		goto(resolve('/pairing'));
	}

	let { children } = $props();
</script>

<div class="flex h-dvh flex-col bg-background">
	<!-- Header -->
	<header
		style:padding-top="max(env(safe-area-inset-top), 12px)"
		class="flex items-center justify-between gap-2 border-b border-border px-4 py-3">
		<div class="flex items-center gap-2">
			<div class="flex h-9 w-9 items-center justify-center rounded-lg bg-primary/10">
				<Bike class="h-5 w-5 text-primary" />
			</div>
			<div>
				<h1 class="text-base font-bold tracking-tight text-foreground">Bike Counter</h1>
				<button
					onclick={handleDisconnect}
					class="flex items-center gap-1.5 text-xs hover:underline {isLiveSensorSelected ? 'text-primary' : 'text-muted-foreground'}"
				>
					{#if isLiveSensorSelected}
						<Bluetooth class="h-3 w-3" />
					{:else}
						<History class="h-3 w-3" />
					{/if}
					{connectedSensor ? resolveDisplaySensorName(connectedSensor.name, connectedSensor.id) : 'Unknown'}
				</button>
			</div>
		</div>

		<div class="flex items-center gap-2">
			{#if sensorState.batteryPercent !== null}
				<Battery class="h-4 w-4" />
				<span class="font-medium">{sensorState.batteryPercent}%</span>
				<span class="text-muted-foreground">{(sensorState.batteryMillivolts! / 1000).toFixed(2)} V</span>
			{/if}
			<Select type="single" value={wheelSize} onValueChange={handleWheelSizeChange}>
				<SelectTrigger class="w-[92px] bg-card text-sm">
					{WHEEL_SIZES.find((s) => s.value === wheelSize)?.label || 'Wheel'}
				</SelectTrigger>
				<SelectContent>
					{#each WHEEL_SIZES as size (size.value)}
						<SelectItem value={size.value}>{size.label}</SelectItem>
					{/each}
				</SelectContent>
			</Select>
		</div>
	</header>

	<main class="flex-1 overflow-hidden p-4">
		{@render children()}
	</main>

	<nav
		class="flex items-center justify-around border-t border-border bg-card px-4 py-2"
		style:padding-bottom="max(env(safe-area-inset-bottom), 8px)">
		<a
			href={resolve('/current')}
			class="flex flex-col items-center gap-1 px-4 py-1.5 rounded-lg transition-colors {isCurrentActive
				? 'text-primary bg-primary/10'
				: 'text-muted-foreground'}"
		>
			<BarChart3 class="h-5 w-5" />
			<span class="text-xs font-medium">Current</span>
		</a>
		<a
			href={resolve('/history')}
			class="flex flex-col items-center gap-1 px-4 py-1.5 rounded-lg transition-colors {isHistoryActive
				? 'text-primary bg-primary/10'
				: 'text-muted-foreground'}"
		>
			<History class="h-5 w-5" />
			<span class="text-xs font-medium">History</span>
		</a>
	</nav>
</div>
