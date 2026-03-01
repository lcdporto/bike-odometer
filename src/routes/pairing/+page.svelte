<script lang="ts">
	import SensorSelector from '$lib/components/SensorSelector.svelte';
	import { sensorState, type Sensor } from '$lib/stores/sensor.svelte';
	import { goto } from '$app/navigation';
	import { resolve } from '$app/paths';

	function handleSensorConnect(sensor: Sensor) {
		sensorState.connectSensor(sensor);
		goto(resolve('/current'));
	}
</script>

<SensorSelector onConnect={handleSensorConnect} />
