<script lang="ts">
    import type { ComponentType } from "svelte";

    export let list: any[];
    export let component: ComponentType;
    export let props: (item: any) => any;
    export let create: () => any;
    export let selected: number = 0;

    function add() {
        list.push(create());
        selected = list.length - 1;
    }

    function remove() {
        list.splice(selected, 1);
        if(selected >= list.length) selected = list.length - 1;
        list = list;
    }

    function up() {
        if(selected === 0) return;
        list[selected] = list.splice(selected - 1, 1, list[selected])[0];
        selected--;
    }

    function down() {
        if(selected >= list.length - 1) return;
        list[selected] = list.splice(selected + 1, 1, list[selected])[0];
        selected++;
    }
</script>

<div class="list">
    <ol>
        {#each list as item, index}
            <li class={selected === index ? "selected" : ""}>
                <button on:click={() => selected = index}>
                    <i class="fa-solid fa-chevron-right" style="margin-right: 1em;"></i>
                    <svelte:component this={component} {...props(item)} />
                </button>
            </li>
        {/each}
    </ol>

    <div>
        <button on:click={add}>
            <i class="fa-solid fa-plus"></i>
        </button>
        <div style="float: right;">
            <button on:click={up}>
                <i class="fa-solid fa-chevron-up"></i>
            </button>
            <button on:click={down}>
                <i class="fa-solid fa-chevron-down"></i>
            </button>
            <span style="margin-left: 1em;"></span>
            <button on:click={remove}>
                <i class="fa-solid fa-trash-can"></i>
            </button>
        </div>
    </div>
</div>

<style>
    button {
        background: none;
        color: inherit;
        border: none;
        padding: 0;
        font: inherit;
        cursor: pointer;
        outline: inherit;
    }

    .list {
        display: flex;
        flex-flow: column;
    }

    ol {
        flex: 1 1 auto;
        padding: 0.25em 0 0 0;
        margin: 0;
        display: block;
    }

    li {
        margin: 0 0 0.75em 0;
        list-style-type: none;
        border: 1px solid gray;
        border-radius: 0.25em;
    }

    li.selected {
        margin: 0 0.5em 0.75em 0.5em !important;
        border: 1px solid var(--text-color) !important;
    }

    li > button {
        display: inline-flex;
        align-items: center;
        padding: 0.5em;
        width: 100%;
    }
</style>
