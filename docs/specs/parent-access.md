# Parent Access

Status: **implemented for the Windows desktop preview; Onion timing and storage validation pending**.

## Stored boundary

Ordinary configuration holds only an opaque `secret:` credential reference. A separate schema-v1 SQLite database stores the self-describing Argon2id hash and at most one device-scoped active grant. A separate 32-byte random device key authenticates grant records. Plain PINs, PIN digits, source input buffers, and the device key are never stored in configuration or profile rows.

PINs contain four to eight decimal digits. Setting or replacing a PIN atomically revokes the active grant. Verification clears the function-owned PIN buffer after hashing. UI-owned input buffers must also be cleared immediately after use.

## End-of-day grant

An authenticated parent may create one grant for the current local calendar date. The record contains:

| Field | Purpose |
| --- | --- |
| `credential_ref` | Identifies the parent credential used to authenticate |
| `issued_at` | Rejects a clock earlier than grant creation |
| `last_observed` | Detects clock movement backward after a successful check |
| `local_date` | Expires the grant when the device's local date changes |
| `nonce` | Makes separately issued grants distinct |
| `mac` | Authenticates every preceding field with the device-local key |

Each successful access check advances and re-authenticates `last_observed`. A malformed record, invalid authenticator, earlier clock, or different date fails closed and removes the grant. Manual lock removes it immediately. Reboot persistence follows from the database and key files; no server or network time is required.

This policy cannot prove that the wall clock is correct after a powered-off device starts with a plausible later time. The launcher must avoid presenting stronger clock guarantees until hardware exposes a trustworthy monotonic or RTC source.

## Sensitive actions

An active grant permits ordinary parent-mode navigation. The current launcher requires fresh PIN verification before Family Dashboard, Profile Settings, Onion Tools, and Backup & Restore. Profile Settings can assign built-in or imported portraits to any active household profile. A dedicated manual-lock action revokes the grant before returning to profile selection. Parent-profile selection alone never authorizes these targets.

Changing the PIN, disabling parental controls, deleting profiles, clearing usage history, exporting secrets, or changing recovery policy must use the same reauthentication boundary when those actions gain concrete implementations.

See [ADR 0008](../decisions/0008-parent-credential-cryptography.md) for the cryptographic dependency and work-factor decision.
