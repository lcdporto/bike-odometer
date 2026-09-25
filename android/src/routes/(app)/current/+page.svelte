<script lang="ts">
	import DistanceChart from '$lib/components/DistanceChart.svelte';
	import StatCard from '$lib/components/StatCard.svelte';
	import {
		getDailyDistanceData,
		getMonthlyDistanceData,
		getTripDayBounds,
		getTripTotals
	} from '$lib/domain/trips';
	import { sensorState } from '$lib/stores/sensor.svelte';
	import { tripsState } from '$lib/stores/trips.svelte';
	import { formatDistanceKm, formatDurationDhm } from '$lib/utils';
	import { ChevronLeft, ChevronRight, CircleDot, Timer, TrendingUp } from '@lucide/svelte';

	type Period = 'week' | 'month' | 'year';

	const periodOptions: { value: Period; label: string; days: number }[] = [
		{ value: 'week', label: 'Last week', days: 7 },
		{ value: 'month', label: 'Last month', days: 30 },
		{ value: 'year', label: 'Last year', days: 365 }
	];

	let selectedPeriod = $state<Period>('month');
	let periodOffset = $state(0);

	let totals = $derived(getTripTotals(tripsState.trips, sensorState.wheelCircumference));
	let dayBounds = $derived(getTripDayBounds(tripsState.trips));
	let periodDays = $derived(periodOptions.find((option) => option.value === selectedPeriod)?.days ?? 30);
	let visibleRange = $derived.by(() => {
		if (!dayBounds) return null;

		const last = new Date(dayBounds.last);
		const first = new Date(last);

		if (selectedPeriod === 'year') {
			last.setMonth(last.getMonth() - periodOffset * 12);
			first.setTime(last.getTime());
			first.setMonth(first.getMonth() - 11, 1);
			last.setMonth(last.getMonth() + 1, 0);
		} else {
			last.setDate(last.getDate() - periodOffset * periodDays);
			first.setTime(last.getTime());
			first.setDate(first.getDate() - periodDays + 1);
		}

		return { first, last };
	});
	let distanceData = $derived(
		!visibleRange
			? []
			: selectedPeriod === 'year'
				? getMonthlyDistanceData(
					tripsState.trips,
					sensorState.wheelCircumference,
					visibleRange.last
				)
				: getDailyDistanceData(
						tripsState.trips,
						sensorState.wheelCircumference,
						visibleRange.first,
						visibleRange.last
					)
	);
	let rangeLabel = $derived(
		visibleRange
			? `${visibleRange.first.toLocaleDateString(undefined, { month: 'short', day: 'numeric', year: visibleRange.first.getFullYear() !== visibleRange.last.getFullYear() ? 'numeric' : undefined })} – ${visibleRange.last.toLocaleDateString(undefined, { month: 'short', day: 'numeric', year: 'numeric' })}`
			: 'No activity'
	);

	let chartDescription = $derived(selectedPeriod === 'year' ? 'km per month' : 'km per day');
	let canGoPrevious = $derived(
		visibleRange !== null &&
		dayBounds !== null &&
		visibleRange.first.getTime() > dayBounds.first.getTime()
	);

	function selectPeriod(period: Period) {
		selectedPeriod = period;
		periodOffset = 0;
	}
</script>

<div class="flex h-full flex-col gap-4">
	<div class="grid grid-cols-2 gap-3">
		<StatCard
			title="Distance"
			value="{formatDistanceKm(totals.totalDistance)} km"
			icon={TrendingUp}
			variant="primary"
			iconAlign="center"
		/>
		<StatCard title="Duration" value={formatDurationDhm(totals.totalMinutes)} icon={Timer} variant="default" iconAlign="center" />
	</div>

	<div class="flex min-h-0 flex-1 flex-col gap-3">
		<div class="flex items-center justify-between gap-3">
			<div class="min-w-0">
				<p class="text-xs font-medium text-muted-foreground">Viewing period</p>
				<p class="truncate text-sm font-semibold">{rangeLabel}</p>
			</div>
			<div class="flex shrink-0 rounded-lg bg-muted p-1" aria-label="Distance interval">
				{#each periodOptions as option}
					<button
						type="button"
						aria-pressed={selectedPeriod === option.value}
						onclick={() => selectPeriod(option.value)}
						class="rounded-md px-2.5 py-1.5 text-xs font-medium transition-colors {selectedPeriod === option.value
							? 'bg-card text-foreground shadow-sm'
							: 'text-muted-foreground'}"
					>
						{option.value === 'week' ? '7D' : option.value === 'month' ? '30D' : '1Y'}
					</button>
				{/each}
			</div>
		</div>

		<div class="flex items-center justify-between gap-3">
			<button
				type="button"
				aria-label="Previous period"
				onclick={() => periodOffset += 1}
				disabled={!canGoPrevious}
				class="flex items-center gap-1 rounded-md border border-border px-3 py-1.5 text-xs font-medium disabled:opacity-40"
			>
				<ChevronLeft class="h-4 w-4" />
				Previous
			</button>
			<span class="text-xs text-muted-foreground">
				{periodOffset === 0 ? 'Latest period' : `${periodOffset} period${periodOffset === 1 ? '' : 's'} back`}
			</span>
			<button
				type="button"
				aria-label="Next period"
				onclick={() => periodOffset = Math.max(0, periodOffset - 1)}
				disabled={periodOffset === 0}
				class="flex items-center gap-1 rounded-md border border-border px-3 py-1.5 text-xs font-medium disabled:opacity-40"
			>
				Next
				<ChevronRight class="h-4 w-4" />
			</button>
		</div>

		<div class="min-h-0 flex-1">
			<DistanceChart
				data={distanceData}
				class="h-full"
				title="Distance"
				description={chartDescription}
			/>
		</div>
	</div>

	<div class="flex items-center justify-center gap-2 text-xs text-muted-foreground">
		<CircleDot class="h-3 w-3" />
		<span>{totals.totalRotations.toLocaleString()} rotations</span>
	</div>
</div>
