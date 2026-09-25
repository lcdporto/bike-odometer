<script lang="ts">
	import { goto } from '$app/navigation';
	import { resolve } from '$app/paths';
	import { sensorState } from '$lib/stores/sensor.svelte';

	$effect(() => {
		if (sensorState.isConnected) {
			goto(resolve('/current'));
		} else {
			goto(resolve('/pairing'));
		}
	});
</script>

<div class="flex h-screen items-center justify-center">
	<p class="text-muted-foreground">Loading...</p>
</div>
