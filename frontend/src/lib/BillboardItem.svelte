<script lang="ts">
    import '$lib/Billboard'

    export let item: BillboardItem;
    export let pos: number = 0; // 0 = left most, 1 = between others, 2 = right most
</script>

<a href={item.link !== undefined ? item.link : ""} target={item.link !== undefined ? "_blank" : ""}>
    {#if item.type == "text"}
        <p>{item.text}</p>
    {:else if item.type == "image"}
        <img src={item.image} alt="Icon for ${item.link}"/>
    {:else if item.type == "icon"}
        <i class={item.icon}></i>
    {:else if item.type == "both"}
        {#if item.first == "text"}
            <p>{item.text}</p>
        {/if}

        {#if item.icon !== undefined}
            <i class={item.icon}></i>
        {:else if item.image !== undefined} 
            <img src={item.image} alt="Icon for ${item.link}" />
        {/if}

        {#if item.first === undefined || item.first !== "text"}
            <p>{item.text}</p>
        {/if}
    {/if}
</a>

<style>
    a {
        display: inline-flex;
        align-items: center;
        line-height: 0;
        overflow: hidden;

        color: inherit;
        text-decoration: none;

        height: var(--height);
    }

    p {
        margin: 0 var(--item-inner-spacing) 0 var(--item-inner-spacing);
    }

    img {
        height: var(--item-height);
        margin: 0 var(--item-inner-spacing) 0 var(--item-inner-spacing);
    }

    i {
        font-size: var(--item-height);
        margin: 0 var(--item-inner-spacing) 0 var(--item-inner-spacing);
    }
</style>
