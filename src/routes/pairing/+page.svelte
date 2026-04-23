<script lang="ts">
	import SensorSelector from '$lib/components/SensorSelector.svelte';
	import { goto } from '$app/navigation';
	import { resolve } from '$app/paths';

	function handleSensorConnect() {
		goto(resolve('/current'));
	}
</script>

<SensorSelector onConnect={handleSensorConnect} />
