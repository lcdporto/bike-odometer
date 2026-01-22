<!-- <script lang="ts">
	import * as Card from '$lib/components/ui/card';
	import * as Chart from '$lib/components/ui/chart';
	import { cn } from '$lib/utils';
	import { scaleBand } from 'd3-scale';
	import { BarChart, type ChartContextValue } from 'layerchart';
	import { cubicInOut } from 'svelte/easing';

	interface DistanceData {
		time: string;
		distance: number;
	}

	interface Props {
		data?: DistanceData[];
		class?: string;
	}

	const mockData: DistanceData[] = [
		{ time: '09:00', distance: 120 },
		{ time: '09:05', distance: 180 },
		{ time: '09:10', distance: 90 },
		{ time: '09:15', distance: 210 },
		{ time: '09:20', distance: 160 },
		{ time: '09:25', distance: 240 }
	];

	const { data: _incoming, class: className }: Props = $props();
	const data = mockData;
	let context = $state<ChartContextValue>();
	let newdata = $derived(data.slice(10));

	$effect(() => {
		console.log('DistanceHistogram data:', newdata);
	});

	const chartConfig = {
		distance: { label: 'Distance', color: 'var(--chart-1)' }
	} satisfies Chart.ChartConfig;

	const series = [
		{
			key: 'distance',
			label: chartConfig.distance.label,
			value: (d: DistanceData) => d.distance,
			color: chartConfig.distance.color
		}
	];

	const chartProps = {
		bars: {
			stroke: 'none',
			rounded: 'all',
			radius: 8,
			initialY: () => context?.height ?? 0,
			initialHeight: 0,
			motion: {
				x: { type: 'tween', duration: 500, easing: cubicInOut },
				width: { type: 'tween', duration: 500, easing: cubicInOut },
				height: { type: 'tween', duration: 500, easing: cubicInOut },
				y: { type: 'tween', duration: 500, easing: cubicInOut }
			}
		},
		highlight: { area: { fill: 'none' } },
		xAxis: { format: (d: string) => d },
		yAxis: {
			format: (v: number) => `${Math.round(Number(v))} m`
		},
		grid: { x: false, y: true },
		tooltip: { item: { format: 'integer' } }
	} as const;
</script>

<Card.Root class={cn('flex flex-col bg-card border-border', className)}>
	<Card.Header>
		<Card.Title>Distance per Interval</Card.Title>
		<Card.Description>Meters traveled each 5-min bucket</Card.Description>
	</Card.Header>
	<Card.Content class="h-full">
		<Chart.Container config={chartConfig} class="w-full h-full">
			<BarChart
				bind:context
				data={newdata}
				xScale={scaleBand().padding(0.25)}
				x="time"
				axis="x"
				series={series}
				props={chartProps}
				renderContext="svg"
			>
				{#snippet tooltip()}
					<Chart.Tooltip hideLabel />
				{/snippet}
			</BarChart>
		</Chart.Container>
	</Card.Content>
</Card.Root> -->


<script lang="ts">
  import { scaleBand } from "d3-scale";
  import { BarChart, type ChartContextValue } from "layerchart";
  import TrendingUpIcon from "@lucide/svelte/icons/trending-up";
  import * as Chart from "$lib/components/ui/chart/index.js";
  import * as Card from "$lib/components/ui/card/index.js";
  import { cubicInOut } from "svelte/easing";

	interface DistanceData {
		time: string;
		distance: number;
	}

	interface Props {
		data: DistanceData[];
		class?: string;
	}

	const { data: chartData, class: className }: Props = $props();

  const chartConfig = {
    distance: { label: "Distance", color: "var(--chart-1)" },
  } satisfies Chart.ChartConfig;

  let context = $state<ChartContextValue>();
</script>

<Card.Root>
  <Card.Header>
		<Card.Title>Distance per Interval</Card.Title>
		<Card.Description>Meters traveled each 5-min bucket</Card.Description>
  </Card.Header>
  <Card.Content>
    <Chart.Container config={chartConfig}>
      <BarChart
        bind:context
        data={chartData}
        xScale={scaleBand().padding(0.25)}
        x="time"
        axis="x"
        series={[{ key: "distance", label: "Distance", color: chartConfig.distance.color }]}
        props={{
          bars: {
            stroke: "none",
            rounded: "all",
            radius: 8,
            // use the height of the chart to animate the bars
            initialY: context?.height,
            initialHeight: 0,
            motion: {
              x: { type: "tween", duration: 500, easing: cubicInOut },
              width: { type: "tween", duration: 500, easing: cubicInOut },
              height: { type: "tween", duration: 500, easing: cubicInOut },
              y: { type: "tween", duration: 500, easing: cubicInOut },
            },
          },
          highlight: { area: { fill: "none" } },
					xAxis: { format: (d:string) => d },
        }}
      >
        {#snippet tooltip()}
          <Chart.Tooltip hideLabel />
        {/snippet}
      </BarChart>
    </Chart.Container>
  </Card.Content>
</Card.Root>
