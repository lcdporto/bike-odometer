<script lang="ts" module>
	import { cn, type WithElementRef } from '$lib/utils.js';
	import type { HTMLAttributes } from 'svelte/elements';

	export type CardProps = WithElementRef<HTMLAttributes<HTMLDivElement>>;
</script>

<script lang="ts">
	let { class: className, ref = $bindable(null), children, ...restProps }: CardProps = $props();
</script>

<div bind:this={ref} data-slot="card" class={cn('bg-card text-card-foreground rounded-xl border shadow-sm', className)} {...restProps}>
	{@render children?.()}
</div>
