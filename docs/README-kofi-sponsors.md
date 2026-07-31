# Ko-fi supporters feed

Ko-fi does not provide a public API to list supporters (its API only
pushes webhook events on donations), so `kofi_sponsors.json` in this folder
is a small, manually maintained list that TsVitch fetches at runtime to show
Ko-fi supporters in **Settings > About**.

It is published to `https://giovannimirulla.github.io/TsVitch/kofi_sponsors.json`
by `.github/workflows/update-downloads-badge.yml`, which copies this file
into the `gh-pages` branch on every scheduled run.

## Format

An array of objects:

```json
[
  {
    "name": "Supporter name",
    "url": "https://ko-fi.com/supporter-profile",
    "avatar": "https://example.com/avatar.png"
  }
]
```

- `name` (required): display name.
- `url` (optional): link opened when the card is clicked. Defaults to the
  Ko-fi page (`https://ko-fi.com/giovannimirulla`) if omitted.
- `avatar` (optional): avatar image URL. If omitted, no avatar is shown.

If the feed is empty, unreachable, or malformed, the app falls back to a
simple link/QR pointing to the Ko-fi page.
