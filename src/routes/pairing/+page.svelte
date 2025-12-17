<script lang="ts">
	import SensorSelector from '$lib/components/SensorSelector.svelte';
	import { connectSensor, type Sensor } from '$lib/stores/sensor.svelte';
	import { goto } from '$app/navigation';

	function handleSensorConnect(sensor: Sensor) {
		connectSensor(sensor);
		goto('/current');
	}
</script>

<SensorSelector onConnect={handleSensorConnect} />
