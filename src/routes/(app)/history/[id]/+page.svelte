<script lang="ts">
	import TripDetail from '$lib/components/TripDetail.svelte';
	import { tripsState, type Trip } from '$lib/stores/trips.svelte';
	import { sensorState } from '$lib/stores/sensor.svelte';
	import { goto } from '$app/navigation';
	import { page } from '$app/state';
	import { onMount } from 'svelte';
	import { DEFAULT_WHEEL_SIZE } from '$lib/config/wheels';
	import { getWheelCircumference } from '$lib/state/app-state';

	let trip: Trip|null = $state(null);
	let wheelSize: string = $state(DEFAULT_WHEEL_SIZE);

	let wheelCircumference = $derived(getWheelCircumference(wheelSize));
	let tripId = $derived(page.params.id);

	function handleBack() {
		goto('/history');
	}

	onMount(() => {
			// no-op, keep onMount for platform parity
		});

	$effect(() => {
		wheelSize = sensorState.wheelSize;
		if (!sensorState.isConnected) {
			goto('/pairing');
		}
	});

	$effect(() => {
		trip = tripsState.find((t) => t.id === tripId) ?? null;
		if (!trip && tripsState.length > 0) {
			goto('/history');
		}
	});
</script>

{#if trip}
	<TripDetail {trip} {wheelCircumference} onBack={handleBack} />
{:else}
	<div class="flex h-full items-center justify-center">
		<p class="text-muted-foreground">Loading trip details...</p>
	</div>
{/if}
