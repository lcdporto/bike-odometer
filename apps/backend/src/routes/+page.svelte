<script lang="ts">
	import { Card } from '$lib/components/ui/card';

	let { data } = $props();

	function fmtDate(timestamp: number): string {
		return new Date(timestamp).toLocaleString();
	}

	function fmtDistance(distance: number): string {
		return `${distance.toFixed(2)} km`;
	}
</script>

<main class="mx-auto max-w-6xl space-y-6 p-6">
	<header class="space-y-1">
		<h1 class="text-2xl font-semibold tracking-tight">Cycle Sensor Sync Dashboard</h1>
		<p class="text-muted-foreground text-sm">Last updated: {fmtDate(data.generatedAt)}</p>
	</header>

	<section class="grid gap-4 md:grid-cols-3">
		<Card class="p-4">
			<p class="text-muted-foreground text-sm">Sensors</p>
			<p class="text-2xl font-semibold">{data.summary.sensors}</p>
		</Card>
		<Card class="p-4">
			<p class="text-muted-foreground text-sm">Trips</p>
			<p class="text-2xl font-semibold">{data.summary.trips}</p>
		</Card>
		<Card class="p-4">
			<p class="text-muted-foreground text-sm">Buckets</p>
			<p class="text-2xl font-semibold">{data.summary.buckets}</p>
		</Card>
	</section>

	{#if data.sensors.length === 0}
		<Card class="p-6">
			<p class="text-muted-foreground text-sm">No synced data yet. Collect data in the mobile app to populate this dashboard.</p>
		</Card>
	{:else}
		<section class="space-y-4">
			{#each data.sensors as sensor}
				<Card class="space-y-4 p-5">
					<div class="flex flex-wrap items-start justify-between gap-2">
						<div>
							<h2 class="text-lg font-semibold">{sensor.name}</h2>
							<p class="text-muted-foreground text-xs">{sensor.id}</p>
						</div>
						<div class="text-right text-sm">
							<p>Wheel: {sensor.wheelSize}&quot;</p>
							<p class="text-muted-foreground">Seen: {fmtDate(sensor.lastSeen)}</p>
						</div>
					</div>

					{#if sensor.trips.length === 0}
						<p class="text-muted-foreground text-sm">No trips for this sensor.</p>
					{:else}
						<div class="overflow-x-auto">
							<table class="w-full text-sm">
								<thead>
									<tr class="border-b text-left">
										<th class="p-2 font-medium">Trip</th>
										<th class="p-2 font-medium">Start</th>
										<th class="p-2 font-medium">Distance</th>
										<th class="p-2 font-medium">Duration</th>
										<th class="p-2 font-medium">Avg speed</th>
										<th class="p-2 font-medium">Buckets</th>
									</tr>
								</thead>
								<tbody>
									{#each sensor.trips as trip}
										<tr class="border-b align-top last:border-b-0">
											<td class="p-2">{trip.id}</td>
											<td class="p-2">{fmtDate(trip.startDate)}</td>
											<td class="p-2">{fmtDistance(trip.distance)}</td>
											<td class="p-2">{trip.duration} min</td>
											<td class="p-2">{trip.avgSpeed.toFixed(2)} km/h</td>
											<td class="p-2">{trip.buckets.length}</td>
										</tr>
									{/each}
								</tbody>
							</table>
						</div>
					{/if}
				</Card>
			{/each}
		</section>
	{/if}
</main>
