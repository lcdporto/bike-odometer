<script lang="ts">
	import { Badge } from '$lib/components/ui/badge';
	import { ChevronRight, MapPin, Clock } from '@lucide/svelte';
	import type { Trip } from '$lib/stores/trips.svelte';
	
	interface Props {
		trips: Trip[];
		onTripSelect: (trip: Trip) => void;
	}
	
	let { trips, onTripSelect }: Props = $props();
</script>

<div class="flex h-full flex-col gap-3 overflow-y-auto">
	{#each trips as trip (trip.id)}
		<button
			onclick={() => onTripSelect(trip)}
			class="flex items-center gap-3 rounded-xl border border-border bg-card p-4 text-left transition-colors hover:bg-muted/50 active:bg-muted"
		>
			<div class="flex-1 min-w-0 space-y-2">
				<div class="flex items-center justify-between gap-2">
					<span class="text-sm font-semibold text-foreground">{trip.date}</span>
					<Badge variant="secondary" class="text-xs shrink-0">
						{trip.startTime} - {trip.endTime}
					</Badge>
				</div>
				
				<div class="flex items-center gap-4 text-xs text-muted-foreground">
					<div class="flex items-center gap-1">
						<MapPin class="h-3.5 w-3.5" />
						<span class="font-mono font-medium">{trip.distance.toFixed(2)} km</span>
					</div>
					<div class="flex items-center gap-1">
						<Clock class="h-3.5 w-3.5" />
						<span class="font-mono">{trip.duration} min</span>
					</div>
					<span class="text-muted-foreground/70 font-mono">{trip.avgSpeed.toFixed(1)} km/h avg</span>
				</div>
			</div>
			
			<ChevronRight class="h-5 w-5 text-muted-foreground shrink-0" />
		</button>
	{/each}
</div>
