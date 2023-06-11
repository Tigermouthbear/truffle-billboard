function createBillboardGroup(items) {
    let group = document.createElement("div");
    group.classList.add("bb-group");
    group.classList.add("bb-parent");
    
    items.forEach(function(item) {
        let hasLink = item.link !== undefined && item.link !== "";
        let container = hasLink ? document.createElement("a") : group;
        if(hasLink) {
            container.href = item.link;
            container.target = "_blank";
            container.classList.add("bb-group");
        }

        createBillboardItems(item).forEach(function(elem) {
            container.appendChild(elem);
        });

        if(hasLink) group.appendChild(container);
    });

    return group;
}

function createBillboardItems(item) {
    let elems;

    if(item.type == "text") elems = [_createBillboardText(item.text)];
    else if(item.type == "image") elems = [_createBillboardImage(item.image)];
    else if(item.type == "icon") elems = [_createBillboardIcon(item.icon)];
    else if(item.type == "both") {
        let text = _createBillboardText(item.text);
        let imgIcon = item.icon !== undefined ? _createBillboardIcon(item.icon) : _createBillboardImage(item.image);

        if(item.first == "text") elems = [text, imgIcon];
        else elems = [imgIcon, text];
    }

    return elems;
}

function _createBillboardText(text) {
    let p = document.createElement("p");
    if(text !== undefined) p.innerText = text;
    return p;
}

function _createBillboardImage(src) {
    let img = document.createElement("img");
    if(src !== undefined && src !== "") img.src = src;
    return img;
}

function _createBillboardIcon(icon) {
    let i = document.createElement("i");
    if(icon !== undefined && icon !== "") icon.split(" ").forEach(function(clazz) {
        if(clazz != "") i.classList.add(clazz);
    });
    return i;
}
