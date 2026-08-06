# Evidence matrix

| Surface | Required evidence |
| --- | --- |
| Rules | Automated tests for objective, collision, scoring, terminal transitions |
| Determinism | Seed replay, golden vectors, stable snapshots/hashes |
| Input | Tap, hold, direction changes, simultaneous actions, blocked-action leakage |
| Lifecycle | Start, pause, resume, back, exit, relaunch |
| Persistence | Fresh start, save, restore, reset, incompatible schema |
| Presentation | Title, gameplay, success, each real fail/recovery state |
| Assets | 1x readability, frame pivots, tile seams, draw ordering, palette/alpha |
| Content | Start/end of every difficulty band and mechanic transition |
| Platform | Windows, desktop PICO-8 when applicable, fake-08, physical Miyoo |

A release report must distinguish automated, visually reviewed, manually played, device-verified, and unverified rows.
