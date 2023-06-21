<script lang="ts">
    import '$lib/Billboard';

    export let group: BillboardGroup;
    export let click: () => void = () => {};
</script>

<button on:click={click}>
    {#each group as item, index}
        <a href={item.link !== undefined ? item.link : ""} target={item.link !== undefined && item.link !== "" ? "_blank" : ""}
            class={(index === 0 ? "left" : "") + " " + (index === group.length - 1 ? "right" : "")}>
            {#if item.type == "text" && item.text !== undefined}
                <p>{item.text}</p>
            {:else if item.type == "image" && item.image !== undefined}
                <img src={item.image} />
            {:else if item.type == "icon" && item.icon !== undefined}
                <i class={item.icon}></i>
            {:else if item.type == "both"}
                {#if item.first == "text"}
                    <p>{item.text === undefined ? "" : item.text}</p>
                {/if}

                {#if item.icon !== undefined}
                    <i class={item.icon}></i>
                {:else if item.image !== undefined} 
                    <img src={item.image} />
                {/if}

                {#if item.first !== "text"}
                    <p>{item.text === undefined ? "" : item.text}</p>
                {/if}
            {/if}
        </a>
    {/each}
</button>

<style>
    button {
        display: inline-flex;
        align-items: center;
        line-height: 0;
        overflow: hidden;

        padding: 0;
        height: var(--height);
        color: var(--text-color);
        background: var(--background);
        border-radius: var(--border-radius);

        border: none;
        font: inherit;
        cursor: pointer;
        outline: inherit;
    }

    a {
        display: inline-flex;
        align-items: center;
        line-height: 0;
        overflow: hidden;

        height: var(--height);
        padding: 0 calc(var(--item-inner-spacing) / 2);

        color: inherit;
        text-decoration: none;
    }

    a.left {
        border-top-left-radius: var(--border-radius);
        border-bottom-left-radius: var(--border-radius);
        padding-left: var(--item-spacing);
    }

    a.right {
        border-top-right-radius: var(--border-radius);
        border-bottom-right-radius: var(--border-radius);
        padding-right: var(--item-spacing);
    }

    a:hover {
        background: var(--hover-background);
    }

    p {
        white-space: nowrap;
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
