<script lang="ts">
	import { goto } from '$app/navigation';
	import { sensorState } from '$lib/stores/sensor.svelte';

	$effect(() => {
		if (sensorState.isConnected) {
			goto('/current');
		} else {
			goto('/pairing');
		}
	});
</script>

<div class="flex h-screen items-center justify-center">
	<p class="text-muted-foreground">Loading...</p>
</div>
