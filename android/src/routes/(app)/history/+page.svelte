<script lang="ts">
	import TripHistory from '$lib/components/TripHistory.svelte';
	import { tripsState } from '$lib/stores/trips.svelte';
	import { goto } from '$app/navigation';
	import { resolve } from '$app/paths';

	function handleTripSelect(trip: typeof tripsState.trips[0]) {
		goto(resolve(`/history/${trip.id}`));
	}
</script>

<TripHistory trips={tripsState.trips} onTripSelect={handleTripSelect} />
