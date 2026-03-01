<script lang="ts">
	import DistanceHistogram from '$lib/components/DistanceHistogram.svelte';
	import { Badge } from '$lib/components/ui/badge';
	import { Button } from '$lib/components/ui/button';
	import type { Trip } from '$lib/stores/trips.svelte';
	import { ArrowLeft, Clock, Gauge, MapPin } from '@lucide/svelte';
	import { calculateDistance, formatDistanceKm } from '$lib/utils';
	
	interface Props {
		trip: Trip;
		wheelCircumference: number;
		onBack: () => void;
	}
	
	let { trip, wheelCircumference, onBack }: Props = $props();
	
	// Helper to convert 12h to 24h format for any legacy data
	function ensureTime24h(timeStr: string): string {
		// If already in 24h format (no AM/PM), return as-is
		if (!timeStr.includes('AM') && !timeStr.includes('PM')) {
			return timeStr;
		}
		// Parse and reformat to 24h
		const match = timeStr.match(/(\d+):(\d+)\s*(AM|PM)/i);
		if (!match) return timeStr;
		let [, hours, minutes, period] = match;
		let h = parseInt(hours);
		if (period.toUpperCase() === 'PM' && h !== 12) h += 12;
		if (period.toUpperCase() === 'AM' && h === 12) h = 0;
		return `${h.toString().padStart(2, '0')}:${minutes}`;
	}
	
	let distancePerBucket = $derived(
		trip.buckets.map((bucket) => {
			const distance = calculateDistance(bucket.rotations, wheelCircumference);
			return {
				time: ensureTime24h(bucket.time),
				distance: Number.parseFloat((distance * 1000).toFixed(0))
			};
		})
	);
</script>

<div class="flex h-full flex-col gap-4">
	<!-- Back button and title -->
	<div class="flex items-center gap-3">
		<Button variant="ghost" size="icon" onclick={onBack} class="shrink-0">
			<ArrowLeft class="h-5 w-5" />
		</Button>
		<div class="flex-1">
			<h2 class="text-lg font-semibold text-foreground">{trip.date}</h2>
			<Badge variant="secondary" class="text-xs">
				{trip.startTime} - {trip.endTime}
			</Badge>
		</div>
	</div>
	
	<!-- Stats row -->
	<div class="grid grid-cols-3 gap-3">
		<div class="flex flex-col items-center rounded-xl bg-card border border-border p-3">
			<MapPin class="h-4 w-4 text-primary mb-1" />
			<span class="text-lg font-bold text-foreground">{formatDistanceKm(trip.distance)}</span>
			<span class="text-xs text-muted-foreground">km</span>
		</div>
		<div class="flex flex-col items-center rounded-xl bg-card border border-border p-3">
			<Clock class="h-4 w-4 text-muted-foreground mb-1" />
			<span class="text-lg font-bold text-foreground">{trip.duration}</span>
			<span class="text-xs text-muted-foreground">min</span>
		</div>
		<div class="flex flex-col items-center rounded-xl bg-card border border-border p-3">
			<Gauge class="h-4 w-4 text-muted-foreground mb-1" />
			<span class="text-lg font-bold text-foreground">{trip.avgSpeed.toFixed(1)}</span>
			<span class="text-xs text-muted-foreground">km/h avg</span>
		</div>
	</div>
	
	<!-- Histogram -->
	<div class="flex-1 min-h-0">
		<DistanceHistogram data={distancePerBucket} class="h-full" />
	</div>
</div>
