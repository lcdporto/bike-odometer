<script lang="ts">
	import TripDetail from '$lib/components/TripDetail.svelte';
	import { tripsState } from '$lib/stores/trips.svelte';
	import { sensorState } from '$lib/stores/sensor.svelte';
	import { goto } from '$app/navigation';
	import { page } from '$app/state';

	let tripId = $derived(page.params.id);
	let trip = $derived(tripId ? tripsState.getTripById(tripId) ?? null : null);

	function handleBack() {
		goto('/history');
	}

	$effect(() => {
		if (!sensorState.isConnected) {
			goto('/pairing');
		}
	});

	$effect(() => {
		if (!trip && tripsState.count > 0) {
			goto('/history');
		}
	});
</script>

{#if trip}
	<TripDetail {trip} wheelCircumference={sensorState.wheelCircumference} onBack={handleBack} />
{:else}
	<div class="flex h-full items-center justify-center">
		<p class="text-muted-foreground">Loading trip details...</p>
	</div>
{/if}
