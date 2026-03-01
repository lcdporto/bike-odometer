<script lang="ts">
	import { Bike, Bluetooth, BarChart3, History } from '@lucide/svelte';
	import { Select, SelectContent, SelectItem, SelectTrigger } from '$lib/components/ui/select';
	import { resolve } from '$app/paths';
	import { sensorState } from '$lib/stores/sensor.svelte';
	import type { Sensor } from '$lib/stores/sensor.svelte';
	import { goto } from '$app/navigation';
	import { page } from '$app/state';
	import { WHEEL_SIZES, DEFAULT_WHEEL_SIZE } from '$lib/config/wheels';
	import { resolveDisplaySensorName } from '$lib/utils';

	let connectedSensor = $state<Sensor | null>(null);
	let wheelSize = $state(DEFAULT_WHEEL_SIZE);

	let currentPath = $derived(page.url.pathname);
	let isCurrentActive = $derived(currentPath === '/current');
	let isHistoryActive = $derived(currentPath.startsWith('/history'));

	$effect(() => {
		connectedSensor = sensorState.connectedSensor;
		wheelSize = sensorState.wheelSize;
	});

	$effect(() => {
		if (wheelSize) {
			sensorState.setWheelSize(wheelSize);
		}
	});

	function handleDisconnect() {
		sensorState.disconnectSensor();
		goto(resolve('/pairing'));
	}

	let { children } = $props();
</script>

<div class="flex h-dvh flex-col bg-background"
>
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
					class="flex items-center gap-1.5 text-xs text-primary hover:underline"
				>
					<Bluetooth class="h-3 w-3" />
					{connectedSensor ? resolveDisplaySensorName(connectedSensor.name, connectedSensor.id) : 'Unknown'}
				</button>
			</div>
		</div>

		<Select type="single" bind:value={wheelSize}>
			<SelectTrigger class="w-[100px] bg-card text-sm">
				{WHEEL_SIZES.find((s) => s.value === wheelSize)?.label || 'Wheel'}
			</SelectTrigger>
			<SelectContent>
				{#each WHEEL_SIZES as size (size.value)}
					<SelectItem value={size.value}>{size.label}</SelectItem>
				{/each}
			</SelectContent>
		</Select>
	</header>

	<main class="flex-1 overflow-hidden p-4">
		{@render children()}
	</main>

	<nav class="flex items-center justify-around border-t border-border bg-card px-4 py-2"
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
