<script lang="ts">
    import '$lib/Billboard';
    import Billboard from './Billboard.svelte';
	import BillboardEditorList from './BillboardEditorList.svelte';
	import BillboardGroup from './BillboardGroup.svelte';

    export let config: BillboardConfig;
    export let socket: WebSocket;

    let selectedGroup: number = 0;
    let selectedItem: number = 0;
    $: currentItem = config.groups.length > selectedGroup && config.groups.length !== 0
        && config.groups[selectedGroup].length > selectedItem && config.groups[selectedGroup].length !== 0 ?
        config.groups[selectedGroup][selectedItem] : undefined;
    $: {
        if(config.groups.length > selectedGroup && selectedItem >= config.groups[selectedGroup].length)
            selectedItem = config.groups[selectedGroup].length - 1; // fixes selection out of range on group switch

        if(currentItem !== undefined) {
            if(currentItem.type !== "text" && currentItem.type !== "both") currentItem.text = undefined;
            if(currentItem.type !== "image" && currentItem.type !== "both") currentItem.image = undefined;
            if(currentItem.type !== "icon" && currentItem.type !== "both") currentItem.icon = undefined;
            if(currentItem.type !== "both") currentItem.first = undefined;
            else if(currentItem.image === "" || currentItem.image === undefined) currentItem.image = undefined;
            else currentItem.icon = undefined;
        }
    }

    let shareUrl: string;
    let shareText: string;
    let shareTime: number = 1;
    function shareLink() {
        if(!/^https:\/\/.+$/.test(shareUrl)) {
            alert("Invalid link! Make sure it starts with https://");
            return;
        }

        socket.send(JSON.stringify({
            type: "link",
            link: shareUrl,
            text: shareText,
            timeout: shareTime * 60000
        }));

        shareUrl = shareText = "";
    } 

    function importConfig() {
        const file = this.files[0];
        if(file && file.type !== "application/json") return;

        const importReader: FileReader = new FileReader();
        importReader.addEventListener("load", () => {
            if(importReader.result !== null)
                config = JSON.parse(importReader.result.toString());
        });
        importReader.readAsText(file);
    }

    function exportConfig() {
        download(JSON.stringify(config, null, 2), "billboard.json", "application/json");
    }

    // Function to download data to a file
    // https://stackoverflow.com/a/30832210
    function download(data: string, filename: string, type: string) {
        const file = new Blob([data], {type: type});
        if(window.navigator.msSaveOrOpenBlob) // IE10+
            window.navigator.msSaveOrOpenBlob(file, filename);
        else { // Others
            var a = document.createElement("a"),
                url = URL.createObjectURL(file);
            a.href = url;
            a.download = filename;
            document.body.appendChild(a);
            a.click();
            setTimeout(function() {
                document.body.removeChild(a);
                window.URL.revokeObjectURL(url);  
            }, 0); 
        }
    }

    function saveConfig() {
        socket.send(JSON.stringify({
            type: "update",
            config: config
        }));
    }
</script>

<div class="editor">
    <fieldset style="grid-area: share;">
        <legend>Share Link</legend>
        <span>
            <input type="url" placeholder="https://example.com/" pattern="https://.*" bind:value={shareUrl} />
            <input type="text" placeholder="Optional Label" bind:value={shareText} />
            <span> for</span><input type="number" min="1" bind:value={shareTime} />
            <span> mins</span>
            <button on:click={shareLink}>Share</button>
        </span>
    </fieldset>

    <fieldset style="grid-area: top;">
        <legend>Actions</legend>
        <span>
            <button style="z-index: -1;">
                <label for="import-upload">Import</label>
            </button>
            <input style="display: none;" type="file" id="import-upload" name="import-upload" on:change={importConfig} />
            <button on:click={exportConfig}>Export</button>
            <button on:click={saveConfig}>Save</button>
        </span>
    </fieldset>

    <fieldset style="grid-area: frames;">
        <legend>Edit Frames</legend>
        <span>
            <BillboardEditorList bind:list={config.groups} component={BillboardGroup} bind:selected={selectedGroup}
                props={(item) => { return { group: item }; }} create={() => { return []; }} />
        </span>
    </fieldset>

    <fieldset style="grid-area: items;">
        <legend>Edit Items</legend>
        <span>
            {#if config.groups.length > selectedGroup && config.groups.length !== 0}
                <BillboardEditorList bind:list={config.groups[selectedGroup]} component={BillboardGroup} bind:selected={selectedItem}
                    props={(item) => { return { group: [item] }; }} create={() => {
                        return {
                            type: "text",
                            link: "",
                            text: ""
                        };
                    }} />
            {/if}
        </span>
    </fieldset>

    <fieldset style="grid-area: settings;">
        <legend>Item Options</legend>
        <span>
            {#if currentItem !== undefined }
                <div class="settings">
                    <label for="item-type">Type:</label>
                    <select name="item-type" id="item-type" bind:value={currentItem.type}>
                        <option value="text">Text</option>
                        <option value="image">Image</option>
                        <option value="icon">Icon</option>
                        <option value="both">Text and Image or Icon</option>
                    </select>
                </div>

                <div class="settings">
                    <label for="item-link">Link:</label>
                    <input type="text" name="item-link" id="item-link" placeholder="https://example.com/" bind:value={currentItem.link} />
                </div>

                {#if currentItem.type === "both"}
                    <div class="settings">
                        <label for="item-first">First:</label>
                        <select name="item-first" id="item-first" bind:value={currentItem.first}>
                            <option value="text">Text</option>
                            <option value="imgicon">Image/Icon</option>
                        </select>
                    </div>
                {/if}

                {#if currentItem.type === "text" || currentItem.type == "both"}
                    <div class="settings">
                        <label for="item-text">Text:</label>
                        <input type="text" name="item-text" id="item-text" placeholder="Example Text" bind:value={currentItem.text} />
                    </div>
                {/if}

                {#if currentItem.type === "image" || currentItem.type === "both"}
                    <div class="settings">
                        <label for="item-image">Image:</label>
                        <input type="text" name="item-image" id="item-image" placeholder="Image URL" bind:value={currentItem.image} />
                    </div>
                {/if}

                {#if currentItem.type === "icon" || currentItem.type === "both"}
                    <div class="settings">
                        <label for="item-icon">*Icon:</label>
                        <input type="text" name="item-icon" id="item-icon" placeholder="fa-style fa-class" bind:value={currentItem.icon} on:keydown={() => currentItem.image = undefined} />
                    </div> 
                {/if}                

                {#if currentItem.type === "icon" || currentItem.type === "both"}
                    <p style="font-size: 0.8em;">
                    *Search <a href="https://fontawesome.com/search?o=r&m=free" target="_blank">fontawesome</a> icons,
                    then enter the icon's class string.
                    Ex: "fa-solid fa-house" = <i class="fa-solid fa-house"></i>
                    </p>
                {/if}
            {/if}
        </span>
    </fieldset>

    <fieldset style="grid-area: preview;">
        <legend>Preview</legend>
        <span>
            <Billboard {config} />
        </span>
    </fieldset>

    <fieldset style="grid-area: other;">
        <legend>Other Settings</legend>
        <span>
            <label for="interval-input">Interval:</label>
            <input type="number" name="interval-input" id="interval-input" min="1" bind:value={config.interval}> seconds
        </span>
    </fieldset>
</div>

<style>
    div.editor {
        flex: 1 1 auto;
        background: var(--editor-background);
        color: var(--text-color);

        display: grid;
        grid-template-areas: "share share top" "frames items settings" "preview items other";
        grid-template-rows: auto 1fr auto;
        grid-template-columns: 30% 30% auto;
        gap: 0.5em;
    }

    fieldset {
        display: flex;
        padding: 0.5em;
        border-radius: 0.25em;
    }

    fieldset > span {
        flex: 1 1 auto;
    }

    input[type="text"] {
        flex: 1 0 auto;
    }

    input, select {
        margin-left: 0.5em;
    }

    input, select, button {
        color: var(--text-color);
        background: var(--editor-background);
    }

    select, button {
        cursor: pointer;
    }

    div.settings {
        display: flex;
        flex-flow: row;
        margin-bottom: 0.5em;
    }
</style>
