<head>
    <title>Truffle Billboard</title>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css" integrity="sha512-iecdLmaskl7CVkqkXNQ/ZH/XLlvWZOJyj7Yy7tcenmpD1ypASozpmT/E0iPtmFIB46ZmdtAc9eNBvH0H/ZpiBw==" crossorigin="anonymous" referrerpolicy="no-referrer"/>
    <style>
        body {
            overflow: hidden;
            padding: 0;
            margin: 0;
        }
    </style>
</head>

<script lang="ts">
    import { onMount } from 'svelte';
    import { initTruffleApp, getEmbed, getAccessToken } from '@trufflehq/sdk';

    import '$lib/Billboard';
    import Billboard from '$lib/Billboard.svelte';
    import BillboardGroup from '$lib/BillboardGroup.svelte';
    import BillboardEditor from '$lib/BillboardEditor.svelte';

    let socket: WebSocket;
    let admin: boolean = false;
    let editing: boolean = false;
    let link: BillboardConfig | undefined;
    let config: BillboardConfig | undefined;
    let theme: BillboardTheme = {
        height: "36px",
        itemHeight: "1.2em",
        itemSpacing: "0.5em",
        itemInnerSpacing: "calc(var(--item-spacing) / 2)",
        borderRadius: "10em",
 
        lightBackground: "rgba(0, 0, 0, 0.05)",
        darkBackground: "rgba(255, 255, 255, 0.1)",
        lightHoverBackground: "#e5e5e5",
        darkHoverBackground: "#3f3f3f",
        lightTextColor: "#0f0f0f",
        darkTextColor: "#f1f1f1",

        lightEditorBackground: "#fff",
        darkEditorBackground: "#000"
    };

    initTruffleApp();
    getAccessToken().then((token) => {
        socket = new WebSocket(encodeURI((window.location.protocol === "http:" ? "ws:" : "wss:") + "//" + window.location.host + "/api?token=" + token));
        socket.addEventListener("message", function(event) {
            const msg: BillboardApiMessage = JSON.parse(event.data);
            if(msg.type == "update") {
                if(msg.admin === true) admin = true;    
                config = msg.config;
            } else if(msg.type == "link" && msg.link !== undefined && msg.timeout !== undefined && msg.link.startsWith("https://")) {
                let urlName = msg.link.replace("https://", "");
                if(urlName.endsWith("/")) urlName = urlName.substring(0, urlName.length - 1);
                if(urlName.length > 23) urlName = urlName.substring(0, 20) + "...";
                
                link = {
                    interval: 1,
                    groups: [[{
                        type: "both",
                        link: msg.link,
                        text: msg.text === undefined || msg.text === "" ? urlName : msg.text,
                        icon: "fa-solid fa-link"
                    }]]
                };
                setTimeout(() => link = undefined, msg.timeout)
            }
        });
    });

    let container: Element;
    const resizer = new ResizeObserver((_) => {
        if(editing || config === undefined) return;
        getEmbed().setStyles({"margin": "12px 0 0 8px"});
        getEmbed().setSize(container.scrollWidth + "px", theme.height);
    });
    onMount(async () => resizer.observe(container));

    function showEditor() {
        getEmbed().setStyles({
            "margin": "0px",
            "position": "fixed",
            "top": "0px",
            "left": "0px",
            "transition": "none",
            "background": "rgba(0, 0, 0, 0.5)",
            "z-index": "9999" // lol
        });
        getEmbed().setSize("100%", "100%");
        editing = true;
    }

    function hideEditor() {
        getEmbed().setSize("0px", "0px");
        getEmbed().setStyles({
            "position": "static",
            "background": "none",
            "z-index": "auto", 
            "transition": "width 1s"
        });
        editing = false;
    }
</script>

<main style="--height: {theme.height}; --item-height: {theme.itemHeight}; --item-spacing: {theme.itemSpacing}; --item-inner-spacing: {theme.itemInnerSpacing}; --border-radius: {theme.borderRadius}; --light-background: {theme.lightBackground}; --dark-background: {theme.darkBackground}; --light-hover-background: {theme.lightHoverBackground}; --dark-hover-background: {theme.darkHoverBackground}; --light-text-color: {theme.lightTextColor}; --dark-text-color: {theme.darkTextColor}; --light-editor-background: {theme.lightEditorBackground}; --dark-editor-background: {theme.darkEditorBackground};">
    <div class={editing ? "editing" : "viewing"} bind:this={container}>
        {#if editing}
            <div>
                <button on:click={hideEditor}><i class="fa-solid fa-xmark"></i></button>
                <p>Truffle Billboard Settings</p>
            </div>
            <BillboardEditor config={JSON.parse(JSON.stringify(config))} {socket} />
        {:else}
            {#if admin}
                <span style="margin-right: var(--item-spacing);">
                    <BillboardGroup group={[{
                        type: "icon",
                        icon: "fa-solid fa-gear"
                    }]} {theme} click={showEditor} />
                </span>
            {/if}

            {#if config !== undefined}
                <Billboard config={link === undefined ? config : link} />
            {/if}
        {/if}
    </div>
</main>

<style>
    main {
        --background: var(--light-background);
        --hover-background: var(--light-hover-background);
        --text-color: var(--light-text-color);
        --editor-background: var(--light-editor-background);

        font-family: "YouTube Sans", "Roboto", sans-serif;
    }

    @media (prefers-color-scheme: dark) {
        main {
            --background: var(--dark-background);
            --hover-background: var(--dark-hover-background);
            --text-color: var(--dark-text-color);
            --editor-background: var(--dark-editor-background);
        }
    }

    div.viewing {
        display: inline-flex;
    }

    div.editing {
        display: flex;
        flex-direction: column;
        overflow: scroll;

        padding: 0.5em;
        margin: 10vh 10vw;
        height: 80vh;
        border-radius: 0.2em;

        background: var(--editor-background);
        color: var(--text-color);
    }

    div.editing > div {
        text-align: center;
        line-height: 0;
    }

    div.editing > div > button {
        float: right;
        background: var(--editor-background);
        color: var(--text-color);
        cursor: pointer;
    }
</style>
