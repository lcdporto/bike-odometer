<script lang="ts">
	import './layout.css';
	import favicon from '$lib/assets/favicon.svg';
	
	let { children } = $props();
</script>

<svelte:head>
	<link rel="icon" href={favicon} />
	<title>Bike Counter</title>
	<meta name="description" content="Track your cycling distance and statistics" />
	<meta name="theme-color" content="#1a1a2e" />
</svelte:head>

<div class="h-screen max-h-screen">
	{@render children()}
</div>
