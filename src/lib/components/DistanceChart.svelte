<script lang="ts">
	import TrendingUpIcon from '@lucide/svelte/icons/trending-up';
	import { curveNatural } from 'd3-shape';
	import { LineChart } from 'layerchart';
	import * as Card from '$lib/components/ui/card';
	import * as Chart from '$lib/components/ui/chart';
	import { cn } from '$lib/utils';

	interface DistanceDataPoint {
		time: string;
		distance: number;
	}

	interface Props {
		data: DistanceDataPoint[];
		class?: string;
	}

	let { data, class: className }: Props = $props();

	const chartData = $derived(data.map((d, idx) => ({ ...d, idx })));
	const hasData = $derived(chartData.length > 0);

	const chartConfig = {
		distance: { label: 'Distance', color: 'var(--chart-2)' }
	} satisfies Chart.ChartConfig;

	let activeChart = $state<keyof typeof chartConfig>('distance');

	const activeSeries = $derived([
		{
			key: activeChart,
			label: chartConfig[activeChart].label,
			color: chartConfig[activeChart].color
		}
	]);

	const totalDistance = $derived(chartData.at(-1)?.distance ?? 0);
	const startingDistance = $derived(chartData.at(0)?.distance ?? 0);
	const distanceDelta = $derived(totalDistance - startingDistance);
	const percentChange = $derived(
		startingDistance > 0 ? (distanceDelta / startingDistance) * 100 : 0
	);
</script>

<Card.Root class={cn('flex flex-col bg-card border-border', className)}>
	<Card.Header>
		<Card.Title>Cumulative Distance</Card.Title>
		<Card.Description>km</Card.Description>
	</Card.Header>
	<Card.Content class="h-full">
		{#if hasData}
			<Chart.Container config={chartConfig} class="h-full w-full">
				<LineChart
					data={chartData}
					x="idx"
					axis="x"
					series={activeSeries}
					props={{
						spline: { curve: curveNatural, motion: 'tween', strokeWidth: 2 },
						xAxis: {
							format: (v: number) => {
								const i = Math.round(Number(v));
								return chartData[i]?.time ?? '';
							},
							padding: 6
						},
						yAxis: {
							format: (v: number) => `${Number(v).toFixed(2)} km`
						},
						grid: { x: false, y: true },
						highlight: { points: { r: 4 } },
						points: {
							r: 3,
							stroke: 'var(--chart-2)',
							strokeWidth: 2,
							fill: 'var(--card)'
						}
					}}
				>
					{#snippet tooltip()}
						<Chart.Tooltip hideLabel />
					{/snippet}
				</LineChart>
			</Chart.Container>
		{:else}
			<p class="text-muted-foreground text-sm">No data</p>
		{/if}
	</Card.Content>
</Card.Root>
