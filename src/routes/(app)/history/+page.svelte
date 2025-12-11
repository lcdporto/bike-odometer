<script lang="ts">
	import TripHistory from '$lib/components/TripHistory.svelte';
	import { tripsStore, type Trip } from '$lib/stores/trips';
	import { sensorStore } from '$lib/stores/sensor';
	import { goto } from '$app/navigation';
	import { onMount } from 'svelte';
	import { DEFAULT_WHEEL_SIZE } from '$lib/config/wheels';
	import { ensureMockTripHistory, getWheelCircumference } from '$lib/state/app-state';

	let trips: Trip[] = $state([]);
	let wheelSize: string = $state(DEFAULT_WHEEL_SIZE);

	let wheelCircumference = $derived(getWheelCircumference(wheelSize));

	function handleTripSelect(trip: Trip) {
		goto(`/history/${trip.id}`);
	}

	onMount(() => {
		const unsubscribe = sensorStore.subscribe((state) => {
			wheelSize = state.wheelSize;
			if (!state.isConnected) {
				goto('/pairing');
			}
		});

		const unsubscribeTrips = tripsStore.subscribe((t) => {
			trips = t;
		});

		ensureMockTripHistory(wheelSize);

		return () => {
			unsubscribe();
			unsubscribeTrips();
		};
	});
</script>

<TripHistory {trips} onTripSelect={handleTripSelect} />
