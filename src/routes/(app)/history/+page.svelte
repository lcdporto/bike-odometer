<script lang="ts">
	import TripHistory from '$lib/components/TripHistory.svelte';
	import { tripsState, type Trip } from '$lib/stores/trips.svelte';
	import { sensorState } from '$lib/stores/sensor.svelte';
	import { goto } from '$app/navigation';
	import { onMount } from 'svelte';
	import { DEFAULT_WHEEL_SIZE } from '$lib/config/wheels';
	import { getWheelCircumference, initializeAppDataFromSensors } from '$lib/state/app-state';

	let trips: Trip[] = $state([]);
	let wheelSize: string = $state(DEFAULT_WHEEL_SIZE);

	let wheelCircumference = $derived(getWheelCircumference(wheelSize));

	function handleTripSelect(trip: Trip) {
		goto(`/history/${trip.id}`);
	}

	onMount(() => {
			initializeAppDataFromSensors();
	});

	$effect(() => {
		wheelSize = sensorState.wheelSize;
		if (!sensorState.isConnected) {
			goto('/pairing');
		}
	});

	$effect(() => {
		trips = tripsState;
	});
</script>

<TripHistory {trips} onTripSelect={handleTripSelect} />
