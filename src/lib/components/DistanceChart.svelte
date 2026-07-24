<script lang="ts">
	import { scaleBand } from 'd3-scale';
	import { BarChart, type ChartContextValue } from 'layerchart';
	import * as Card from '$lib/components/ui/card';
	import * as Chart from '$lib/components/ui/chart';
	import { cn, formatDistanceKm } from '$lib/utils';
	import { cubicInOut } from 'svelte/easing';

	interface DistanceDataPoint {
		time: string;
		distance: number;
	}

	interface Props {
		data: DistanceDataPoint[];
		class?: string;
		title?: string;
		description?: string;
	}

	let {
		data,
		class: className,
		title = 'Distance per day',
		description = 'km per day'
	}: Props = $props();

	const chartData = $derived(data.map((d, idx) => ({ ...d, idx })));
	const hasData = $derived(chartData.length > 0);
	const tickInterval = $derived(Math.max(1, Math.ceil(chartData.length / 7)));
	const visibleLabels = $derived(
		new Set(chartData.filter((_, index) => index % tickInterval === 0).map((point) => point.time))
	);
	let context = $state<ChartContextValue>();

	const chartConfig = {
		distance: { label: 'Distance', color: 'var(--chart-2)' }
	} satisfies Chart.ChartConfig;

</script>

<Card.Root class={cn('flex flex-col bg-card border-border', className)}>
	<Card.Header>
		<Card.Title>{title}</Card.Title>
		<Card.Description>{description}</Card.Description>
	</Card.Header>
	<Card.Content class="h-full">
		{#if hasData}
			<Chart.Container config={chartConfig} class="h-full w-full">
				<BarChart
					bind:context
					data={chartData}
					xScale={scaleBand().padding(0.2)}
					x="time"
					axis="x"
					series={[{
						key: 'distance',
						label: chartConfig.distance.label,
						color: chartConfig.distance.color
					}]}
					props={{
						xAxis: {
							format: (value: string) => visibleLabels.has(value) ? value : ''
						},
						yAxis: {
							format: (v: number) => `${formatDistanceKm(Number(v))} km`
						},
						grid: { x: false, y: true },
						highlight: { area: { fill: 'none' } },
						bars: {
							rounded: 'top',
							radius: 5,
							stroke: 'none',
							initialY: context?.height,
							initialHeight: 0,
							motion: {
								y: { type: 'tween', duration: 350, easing: cubicInOut },
								height: { type: 'tween', duration: 350, easing: cubicInOut }
							}
						}
					}}
				>
					{#snippet tooltip()}
						<Chart.Tooltip hideLabel />
					{/snippet}
				</BarChart>
			</Chart.Container>
		{:else}
			<p class="text-muted-foreground text-sm">No data</p>
		{/if}
	</Card.Content>
</Card.Root>
