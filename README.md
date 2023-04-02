# Truffle Billboard Embed
Create a custom (and interactive) billboard underneath your youtube videos and livestreams!

## Todo List
- Billboard Editor
- Link share
- Add css options to groups
- Custom Background/Image instead of rounded youtube
- Change background color on link hover
- Click minigame?

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
}```

