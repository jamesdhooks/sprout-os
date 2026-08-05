# Mouse & Cheese shared game design

Mouse & Cheese has native Sprout Runtime and PICO-8 editions. They share the
player goal, campaign progression intent, accessibility rules, seed provenance,
and warm forest storybook language. They do not share rendering code, runtime
assets, display resolution, or save implementation.

The PICO-8 edition is a 128x128, 16-colour adaptation. It has a 100-level
campaign and automatic cheese-to-next-level flow. Four maze bands grow from
7x7 to 13x13 while retaining a minimum 9-pixel cell size; later difficulty is
driven by route length, loops, branches, and omitted rooms rather than smaller
rendering. Before maze carving, each level receives a deterministic connected
shape mask with edge-attached chunks removed; these cutouts expose the forest
background and prevent the arena silhouette from remaining a plain rectangle.
Chunk count and maximum depth increase by campaign band. Offline acceptance thresholds live in
`pico8/mouse-cheese/campaign-spec.json`.
