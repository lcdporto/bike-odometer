<script lang="ts">
	import TripDetail from '$lib/components/TripDetail.svelte';
	import { tripsStore, type Trip } from '$lib/stores/trips';
	import { sensorStore } from '$lib/stores/sensor';
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
		const unsubscribe = sensorStore.subscribe((state) => {
			wheelSize = state.wheelSize;
			if (!state.isConnected) {
				goto('/pairing');
			}
		});

		const unsubscribeTrips = tripsStore.subscribe((trips) => {
			trip = trips.find((t) => t.id === tripId) ?? null;
			if (!trip && trips.length > 0) {
				goto('/history');
			}
		});

		return () => {
			unsubscribe();
			unsubscribeTrips();
		};
	});
</script>

{#if trip}
	<TripDetail {trip} {wheelCircumference} onBack={handleBack} />
{:else}
	<div class="flex h-full items-center justify-center">
		<p class="text-muted-foreground">Loading trip details...</p>
	</div>
{/if}
