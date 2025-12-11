<script lang="ts">
	import SensorSelector from '$lib/components/SensorSelector.svelte';
	import { sensorStore, type Sensor } from '$lib/stores/sensor';
	import { goto } from '$app/navigation';

	function handleSensorConnect(sensor: Sensor) {
		sensorStore.connect(sensor);
		goto('/current');
	}
</script>

<SensorSelector onConnect={handleSensorConnect} />
