<script lang="ts">
	import TripHistory from '$lib/components/TripHistory.svelte';
	import { tripsState } from '$lib/stores/trips.svelte';
	import { sensorState } from '$lib/stores/sensor.svelte';
	import { goto } from '$app/navigation';
	import { onMount } from 'svelte';
	import { initializeAppDataFromSensors } from '$lib/state/app-state';

	function handleTripSelect(trip: typeof tripsState.trips[0]) {
		goto(`/history/${trip.id}`);
	}

	onMount(() => {
		initializeAppDataFromSensors();
	});

	$effect(() => {
		if (!sensorState.isConnected) {
			goto('/pairing');
		}
	});
</script>

<TripHistory trips={tripsState.trips} onTripSelect={handleTripSelect} />
