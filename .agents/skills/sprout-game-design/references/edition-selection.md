# Edition selection

| Concern | Native Sprout edition | PICO-8 edition |
| --- | --- | --- |
| Runtime | Sprout Runtime and package Lua | Standalone `.p8` cart |
| Display | Package-declared logical resolution; 640x480 is the Miyoo display target, not an engine-wide canvas | Fixed 128x128 |
| Artwork | Highest practical masters and target derivatives | Dedicated tiny palette-index art |
| Shared code | Versioned engine modules after real reuse | Small injected cart source only when multiple carts consume it |
| Persistence | Runtime-owned profile/package storage | `cartdata()` plus Sprout profile-specific IDs |
| Content | Resolved campaign data or bounded runtime generation | Compact approved seeds/data and PICO-safe generation |
| Release target | Windows preview, then SproutOS/Miyoo | Desktop PICO-8, fake-08, and optional licensed wrapper |

Choose paired editions only when the same game rules benefit both platforms. A paired edition shares design intent and campaign provenance, but each edition must remain independently playable and maintainable.
