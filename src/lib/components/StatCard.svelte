<script lang="ts">
	import { Card, CardContent } from '$lib/components/ui/card';
	import { cn } from '$lib/utils';
	import type { Component } from 'svelte';
	
	interface Props {
		title: string;
		value: string;
		subtitle?: string;
		icon: Component;
		variant?: 'default' | 'primary' | 'accent';
		class?: string;
	}
	
	let { title, value, subtitle, icon: Icon, variant = 'default', class: className }: Props = $props();
</script>

<Card
	class={cn(
		'bg-card border-border transition-all',
		variant === 'primary' && 'border-primary/30 bg-primary/5',
		variant === 'accent' && 'border-accent/30 bg-accent/5',
		className
	)}
>
	<CardContent class="p-3">
		<div class="flex items-start justify-between">
			<div class="space-y-0.5">
				<p class="text-xs text-muted-foreground">{title}</p>
				<p
					class={cn(
						'text-xl font-bold font-mono tracking-tight',
						variant === 'primary' && 'text-primary',
						variant === 'accent' && 'text-accent',
						variant === 'default' && 'text-foreground'
					)}
				>
					{value}
				</p>
				{#if subtitle}
					<p class="text-xs text-muted-foreground">{subtitle}</p>
				{/if}
			</div>
			<div
				class={cn(
					'flex h-8 w-8 items-center justify-center rounded-lg',
					variant === 'primary' && 'bg-primary/10',
					variant === 'accent' && 'bg-accent/10',
					variant === 'default' && 'bg-muted'
				)}
			>
				<Icon
					class={cn(
						'h-4 w-4',
						variant === 'primary' && 'text-primary',
						variant === 'accent' && 'text-accent',
						variant === 'default' && 'text-muted-foreground'
					)}
				/>
			</div>
		</div>
	</CardContent>
</Card>
