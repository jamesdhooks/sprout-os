# PICO-8 development

For the contributor workflow and copyable starter cartridge, see
[Writing, testing, and deploying PICO-8 games](pico8-game-development.md).

Sprout ships original `.p8` cartridges for fake-08 through Onion. It does not ship PICO-8 itself, `pico8.dat`, BBS carts, or user save data.

## Mouse & Cheese

`pico8/mouse-cheese/mouse-cheese.p8` is directly loadable and self-contained. Its 1,000 campaign layouts are generated offline from `campaign-spec.json`; `campaign.json` records the accepted seed, metrics, and layout hash for each level. The cartridge receives only the approved seed payload.

Regenerate and check it from the repository root:

```powershell
python tools/pico8_mouse_cheese_campaign.py generate --spec pico8/mouse-cheese/campaign-spec.json --campaign pico8/mouse-cheese/campaign.json
python tools/pico8_mouse_cheese_campaign.py inject --campaign pico8/mouse-cheese/campaign.json --cart pico8/mouse-cheese/mouse-cheese.p8
python tools/pico8_mouse_cheese_assets.py --sprites pico8/mouse-cheese/sprites.json --cart pico8/mouse-cheese/mouse-cheese.p8
python tools/pico8_mouse_cheese_campaign.py validate --spec pico8/mouse-cheese/campaign-spec.json --campaign pico8/mouse-cheese/campaign.json --cart pico8/mouse-cheese/mouse-cheese.p8
```

`sprites.json` is the original 16-colour sprite source. The compiler produces the cart's single PICO sprite bank; it is not required on the device.

## Deployment and saves

`tools/pico8-deploy.ps1 -SdRoot <card> -ProfileId <profile>` validates the campaign, installs the public cart to `Roms/PICO/Sprout/MouseCheese/`, writes Sprout-owned metadata under `Sprout/catalogue/`, and optionally makes a hidden per-profile cart under `Roms/PICO/.sprout-profiles/`. The managed copy changes only the cart's `cartdata()` identifier. Its identifier is a lowercase, 64-character-safe profile hash, so PICO persistence is isolated by Sprout profile.

Do not add `.sprout-profiles` to normal library discovery. The public cart remains directly launchable from Onion. The currently supported release runtime is Onion's fake-08; a purchased native PICO-8 wrapper may be used separately for compatibility checks but is not bundled.

## Validation boundary

The Python validator checks reproducibility, hashes, metrics, and cart payload integrity. A configured desktop PICO-8 executable can additionally check token and compressed-cart limits. Fake-08/Miyoo input, storage, GameSwitcher return, and frame pacing require device validation and are not established by host checks.
