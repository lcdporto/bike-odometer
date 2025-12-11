<script lang="ts">
	import { goto } from '$app/navigation';
	import { onMount } from 'svelte';
	import { sensorStore } from '$lib/stores/sensor';

	onMount(() => {
		// Redirect based on connection status
		const unsubscribe = sensorStore.subscribe((state) => {
			if (state.isConnected) {
				goto('/current');
			} else {
				goto('/pairing');
			}
		});

		return unsubscribe;
	});
</script>

<div class="flex h-screen items-center justify-center">
	<p class="text-muted-foreground">Loading...</p>
</div>
