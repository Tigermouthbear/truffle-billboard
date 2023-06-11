# Truffle Billboard Embed
![](https://i.tigr.dev/lg4nsy72.png)

Truffle Billboard is an interactive and customizable "billboard" or menu for youtube livestreams. The motivation behind this is to allow streamers to easily and elegantly share links to their social media, sponsors, or other resources in a way thats more presentable than a youtube description. The billboard is located just under the viewing area and cycles through frames at a configurable interval. Each frame is very customizable, with support for links with images, font-awesome icons, text, or a mix of each. It has with a WYSIWYG editor built into the settings panel and even supports easy sharing links in real time, for a configurable length of time. I have linked the code + a video of functionality.

- https://github.com/Tigermouthbear/truffle-billboard
- https://www.youtube.com/watch?v=jC-79QAyCFc

## TODOs before production
- Reconnect embed after ws disconnect?
- Figure out if admin check is actually working
- Make billboard editor have of minimum height of 100% (i hate css)
- Configure SSL

## Embed Config
```
{
  "url": "http://localhost:8000/embed.html",
  "authToken": "[REDACTED]",
  "contentPageType": "youtube",
  "parentQuerySelector": "#above-the-fold #title",
  "defaultStyles": {
    "width": "0px",
    "height": "36px",
    "float": "right",
    "transition": "width 1s"
  }
}
```
