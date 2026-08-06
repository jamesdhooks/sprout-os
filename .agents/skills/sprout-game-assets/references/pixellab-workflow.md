# PixelLab workflow

The configured MCP server is pixellab at https://api.pixellab.ai/mcp, authenticated from PIXELLAB_API_TOKEN.

## Tool routing

| Need | Preferred tool |
| --- | --- |
| Coherent directional actor | create_character |
| Walk, idle, push, attack, or other actor motion | animate_character |
| Forced-palette transparent prop | create_image_pixflux |
| Larger clean pixel-art canvas | create_image_pixen |
| Highest-quality master with labelled references | create_image_pro |
| Context-matched map decoration | create_map_object |
| Seamless terrain transitions | create_topdown_tileset |
| Repair one bounded defect | inpaint_image |

Poll asynchronous jobs with the corresponding getter. Archive the completed provider output before selection, inpainting, downsampling, or atlas packing.

## Prompt contract

Specify:

- one asset identity and gameplay role;
- high, low, or side camera explicitly;
- physical form and material;
- palette or labelled style reference;
- transparent or scenic background;
- ground-contact anchor and intended runtime footprint;
- exclusions such as no text, cast shadow, detached pixels, diagonal isometric rotation, or extra objects.

Do not describe sheet coordinates in the asset concept prompt. Generate or select the identity first, then ask the animation/directional tool for consistent views.

## Small PICO sprites

PixelLab's raw-image tools have minimum canvas/area limits. Generate at a clean integer multiple of the final sprite, use low detail and flat/basic shading, archive the original, and derive the tiny sprite locally. Forced palette does not guarantee readable geometry; validate at 1x and redraw pixels where needed.

## History invariant

Use one directory per provider job ID. Preserve prompt, seed, operation, dimensions, palette/reference identifiers, download URL, source checksum, source PNG, and derived reviews. Rerolls and edits receive new job IDs and reference their parent; they never overwrite it.
