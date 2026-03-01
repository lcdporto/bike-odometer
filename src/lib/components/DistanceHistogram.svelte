<script lang="ts">
  import * as Card from "$lib/components/ui/card/index.js";
  import * as Chart from "$lib/components/ui/chart/index.js";
  import { scaleBand } from "d3-scale";
  import { BarChart, type ChartContextValue } from "layerchart";
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
	
	// Calculate tick interval based on data length to avoid overlapping labels
	// Show ~5-8 labels maximum
	let tickInterval = $derived(Math.max(1, Math.ceil(chartData.length / 6)));
	
	// Create a map of times to show
	let showTimeLabels = $derived(new Set(
		chartData.filter((_, index) => index % tickInterval === 0).map(d => d.time)
	));
</script>

<Card.Root class={className}>
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
					xAxis: { 
						format: (d: string) => {
							// Only show labels for selected times to prevent overlap
							return showTimeLabels.has(d) ? d : '';
						}
					},
        }}
      >
        {#snippet tooltip()}
          <Chart.Tooltip hideLabel />
        {/snippet}
      </BarChart>
    </Chart.Container>
  </Card.Content>
</Card.Root>
