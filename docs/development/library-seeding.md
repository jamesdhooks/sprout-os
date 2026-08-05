# Local library and household seeding

Sprout never distributes ROMs, BIOS files, or PICO-8 runtime files. It can
index locally supplied content and seed an editable household around it.

## Household seed

[`config/household-seed.example.json`](../../config/household-seed.example.json)
is a schema-v1 example with two parents and two children. Copy it to the
ignored `config/household-seed.json`, then replace identities, avatar choices,
and curation before seeding.

Seed only a fresh data directory:

```powershell
tools/dev.ps1 -Action run -SdRoot <extracted-Onion-card> `
  -HouseholdSeed config/household-seed.json
```

For direct launcher use, add `--household-seed config/household-seed.json` and
`--data-dir <new-data-directory>`. The launcher creates missing profiles but
never overwrites an existing profile. A seed completes first-run setup without
creating a parent PIN; create one in the normal parental setup before handing a
child profile to a child.

Parents see the complete locally discovered library. A child sees only curated
items plus favorites. This makes a favorite an implicit allowlist addition even
when it was not in the starter set. The checked-in seed includes all three
Sprout Arcade games for both child profiles.

The child "Browse More" action is intentionally deferred until it can invoke
the existing parent-PIN boundary and persist a precise approval grant. It must
not silently expose a full library.

## Tiny Best Set input

`Q:\miyoo\tiny` currently contains Tiny Best Set download archives rather than
an extracted Onion SD-card tree. The games archive contains a `Roms/` hierarchy
and should be extracted to a separately prepared development card before
launching. The current scanner discovers only the verified GB and SFC Onion
launch paths; other Tiny Best Set systems remain catalogue/research work until
their system launcher contracts are verified.

Use the supplied personal-curation list to populate each parent’s
`favoriteItems`, and the supplied age-four list for each child’s
`curatedItems`. Each item uses an explicit `platform` (for example `GB`,
`SFC`, `NES`, or `ARCADE`) and `title`. The repository deliberately keeps
those household selections out of version control. Matching is
case-insensitive, platform-scoped, and strips ROM region/revision suffixes
such as `(USA)`.

## PICO-8

Onion documents `Roms/PICO` with `.p8` and `.png` carts for the fake-08
emulator. It also documents an optional purchased PICO-8 native wrapper with
different save and compatibility behavior. Sprout should catalogue PICO-8 only
after the selected Onion package and exact launch script are present on the
development card; its adapter must declare whether it is targeting fake-08 or
the licensed native wrapper. See [PICO-8 investigation](../research/pico-8.md).
