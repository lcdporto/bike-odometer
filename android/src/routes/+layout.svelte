<script lang="ts">
	import './layout.css';
	import favicon from '$lib/assets/favicon.svg';
	import { ModeWatcher } from "mode-watcher";
	import { onMount } from 'svelte';
	import { startBackgroundScanning, stopBackgroundScanning } from '$lib/services/ble-background';
	import { Toaster } from "$lib/components/ui/sonner/index.js";
	
	let { children } = $props();
	
	onMount(() => {
		// Start background BLE scanning
		startBackgroundScanning().catch((err) => {
			console.error('Failed to start background scanning:', err);
		});
		
		// Cleanup on unmount
		return () => {
			stopBackgroundScanning();
		};
	});
</script>

<svelte:head>
	<link rel="icon" href={favicon} />
	<title>Bike Counter</title>
	<meta name="description" content="Track your cycling distance and statistics" />
	<meta name="theme-color" content="#102b36" />
</svelte:head>

<Toaster/>
<ModeWatcher />
<div class="h-screen max-h-screen">
	{@render children()}
</div>
