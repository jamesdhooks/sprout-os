---
name: sprout-game-assets
description: Design, generate, archive, refine, pack, validate, or integrate visual assets for Sprout native games and PICO-8 editions. Use for PixelLab MCP generation, art direction, sprites, directional animation, tile sets, atlases, title screens, library covers, transparency, palettes, anchors, native-versus-PICO resolution choices, immutable generation history, or replacing placeholder game art.
---

# Sprout game assets

Produce replaceable assets that remain coherent at their actual runtime scale.

## Classify the target first

Read docs/arcade/assets.md, docs/architecture/visual-system.md, the target game's visual-language notes, and references/target-profiles.md.

- For native games, retain the highest practical reviewed master and derive package-declared runtime sizes. Never impose one global logical resolution.
- For PICO-8, design for the fixed 128x128 screen, exact palette indices, actual sprite dimensions, and cart memory. Do not downsample native art mechanically.
- Keep SproutOS startup art, per-game title art, library covers, gameplay sprites, UI, and profile art as distinct composition contracts.

## Define visual language before generation

Record the game's palette, camera, lighting, outline weight, material language, ground-contact anchor, scale family, animation timing, title composition, and readability rules in public game documentation. Keep provider names, private prompts, job logs, and rejected variants local.

Use readable silhouettes, stable pivots, restrained palettes, seamless tiles, and animation frames that change motion rather than identity. Review every sprite at 1x before approving an enlarged preview.

## Choose the PixelLab workflow

Read references/pixellab-workflow.md.

- Use character creation plus character animation for coherent directional actors.
- Use PixFlux for transparent props when a forced palette matters.
- Use Pixen or Pro for larger masters when structure matters more than exact palette.
- Use map-object or tileset tools when context/style matching or transitions are the actual requirement.
- Generate one asset concept at a time. Do not ask the model to invent an entire packed sheet unless the tool specifically owns directional consistency.

Describe the asset as a whole: form, camera, materials, anchor, palette, negative constraints, and runtime role. Let the character/animation tool own directions and frame layout.

## Preserve immutable history

Every generation must be archived before editing or integration:

~~~powershell
python .agents/skills/sprout-game-assets/scripts/archive_pixellab_generation.py --job-id <id> --download-url <url> --operation create_image_pixflux --prompt-file <prompt.txt> --source-size 64x80 --runtime-size 16x20 --seed 1234 --metadata-json '{"palette":["#1D2B53"]}'
~~~

The script writes an untouched source, deterministic review enlargement, checksum, prompt, and parameters beneath ignored .local-work/pixellab-history/YYYY-MM-DD/<job-id>/. It refuses to overwrite an existing job.

Never make a tracked atlas the only copy of a generated source. Never delete a rejected generation to make history look cleaner.

## Derive and integrate

1. Validate source dimensions, alpha, palette, silhouette, camera, and anchor.
2. Crop only by an explicit pivot/trim contract.
3. Produce target derivatives with the declared resampling mode.
4. Hand-refine PICO sprites after integer downsampling when necessary.
5. Pack deterministically; preserve individual reviewed sources.
6. Validate frame bounds, collisions, padding, animation coverage, and manifest provenance.
7. Run the game-specific builder with --check.
8. Capture actual gameplay, not only contact sheets.

Reject assets with malformed anatomy, stray matte pixels, inconsistent perspective, unstable baselines, unreadable 1x silhouettes, seams, or palette drift.

Read references/external-influences.md only when auditing the origins and licences of the adapted workflow principles.
