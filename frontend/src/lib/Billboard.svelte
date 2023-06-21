<head>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css" integrity="sha512-iecdLmaskl7CVkqkXNQ/ZH/XLlvWZOJyj7Yy7tcenmpD1ypASozpmT/E0iPtmFIB46ZmdtAc9eNBvH0H/ZpiBw==" crossorigin="anonymous" referrerpolicy="no-referrer"/>
</head>

<script lang="ts">
    import '$lib/Billboard';
    import BillboardGroup from './BillboardGroup.svelte';

    export let config: BillboardConfig;

    let curr: number = 0;
    let interval: number;
    $: {
        clearInterval(interval);
        if(config.interval > 0 && config.groups.length > 1)
            interval = setInterval(() => curr++, config.interval * 1000);
    }
</script>

<div>
    {#if config.groups.length > 0}
        <BillboardGroup group={config.groups[curr % config.groups.length]} />
    {/if}
</div>
