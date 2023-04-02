function createBillboardGroup(items) {
    let group = document.createElement("div");
    group.classList.add("bb-group");
    group.classList.add("bb-parent");
    
    items.forEach(function(item) {
        let hasLink = item.link !== undefined;
        let container = hasLink ? document.createElement("a") : group;
        if(hasLink) {
            container.href = item.link;
            container.target = "_blank";
            container.classList.add("bb-group");
        }
        
        if(item.type == "text") container.appendChild(_createBillboardText(item.text));
        else if(item.type == "image") container.appendChild(_createBillboardImage(item.image));
        else if(item.type == "icon") container.appendChild(_createBillboardIcon(item.icon));
        else if(item.type == "both") {
            let text = _createBillboardText(item.text);
            let imgIcon = item.icon !== undefined ? _createBillboardIcon(item.icon) : _createBillboardImage(item.image);

            if(item.first == "text") {
                container.appendChild(text);
                container.appendChild(imgIcon);
            } else {
                container.appendChild(imgIcon);
                container.appendChild(text);
            }
        }

        if(hasLink) group.appendChild(container);
    });

    return group;
}

function _createBillboardText(text) {
    let p = document.createElement("p");
    p.innerText = text;
    return p;
}

function _createBillboardImage(src) {
    let img = document.createElement("img");
    img.src = src;
    return img;
}

function _createBillboardIcon(icon) {
    let i = document.createElement("i");
    icon.split(" ").forEach(function(clazz) {
        i.classList.add(clazz);
    });
    return i;
}
