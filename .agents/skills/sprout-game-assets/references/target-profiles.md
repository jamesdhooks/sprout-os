# Asset target profiles

## Native Sprout

- Display target: Miyoo Mini Plus 640x480, while each package declares its own logical surface.
- Masters: highest practical reviewed resolution.
- Filtering: declared by art profile; storybook raster art may use smooth resampling.
- Atlases: named frames, stable pivots, explicit target sizes, padding, and batching metadata.
- Raised grid objects: visual art may extend above the occupied cell; ground contact stays bottom-center.

## PICO-8

- Display: fixed 128x128.
- Palette: PICO-8's 16 indices.
- Gameplay art: dedicated tiny sprites reviewed at 1x.
- Source generation: use an integer multiple such as 2x or 4x when the provider cannot produce a reliable 16x20 asset; preserve the larger original, downsample with nearest neighbour, then hand-refine.
- Titles: dedicated 128x128 composition, separate from the gameplay sprite bank and launcher cover.
- Library cover: separate 16:9 Sprout launcher artwork.

## Shared rules

- Collision size and rendered size are independent.
- Art direction may be shared; raster assets are not.
- Every animation frame shares an intentional pivot and footprint.
- Important states differ by shape or motion, not colour alone.
