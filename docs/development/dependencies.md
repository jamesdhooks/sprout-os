# Dependency Decisions

Dependencies are pinned in `CMakeLists.txt`. This document records the review required when the project adds one; it is not a general package inventory.

## Lua 5.4.8

| Consideration | Decision |
| --- | --- |
| Purpose | Execute small native-game logic behind a constrained runtime API while keeping game packages portable and data-sized |
| Alternatives | A native plug-in ABI would expose platform toolchains and unsafe process access; a custom scripting language would create an unnecessary parser and toolchain; compiling each game into the runtime would prevent independent packages |
| Maintenance | Lua 5.4.8 is the final published 5.4 patch release and has stable official C API documentation; Sprout intentionally does not adopt a new language series before the preview contract is exercised |
| License | MIT-style Lua license; required notice is retained in `THIRD_PARTY_NOTICES.md` |
| Binary-size impact | The runtime statically links the language core and selected standard-library objects; exact optimized artifact size must be recorded before device targeting |
| Target compatibility | Lua is ISO C and builds on the current Windows toolchain. Linux and Miyoo compatibility remain unverified and are not implied by the Windows preview |

Sources: [official Lua 5.4.8 source and checksum](https://www.lua.org/ftp/) and [Lua 5.4 documentation](https://www.lua.org/manual/5.4/).
