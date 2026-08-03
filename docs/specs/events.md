# Event Vocabulary

Status: **provisional v0.1**. Events describe observable facts for local journaling and tests. This specification does not select a message bus, network transport, or server.

Every persisted event is expected to carry an event ID, schema version, event name, occurrence time, device ID, and relevant profile/item/session references. Payloads contain the minimum event-specific facts. Sensitive values, credentials, PIN material, and unrestricted paths are excluded.

| Event | Meaning | Minimum payload expectations |
| --- | --- | --- |
| `GameStarted` | A permitted game became active | Item, session, launch kind, fresh/resume |
| `GamePaused` | Active play stopped without ending the session | Item, session, reason |
| `GameResumed` | A paused game became active again | Item, session, resume source |
| `GameExited` | The active game ended or returned to the launcher/switcher | Item, session, reason, normal/abnormal result |
| `PlaytimeUpdated` | Accounted active time advanced | Item/session, monotonic duration delta, resulting daily total |
| `AchievementUnlocked` | A native game reported a validated achievement | Item, achievement ID, game/package version |
| `LevelCompleted` | A native game completed a defined level | Item, level ID, attempt/session reference |
| `RecommendationShown` | A recommendation became visible | Item, placement, recommendation ID |
| `RecommendationAccepted` | The user acted on a shown recommendation | Recommendation ID, item, action |
| `TimeExtensionRequested` | A child requested additional time | Profile, requested duration or preset, current allowance |
| `TimeExtensionGranted` | An authenticated authority issued additional access | Profile, grant scope, duration/expiry, issuer class, revocation state |

Events are appendable offline and may be synchronized later. Consumers must tolerate duplicates by event ID, unknown newer event types, delayed delivery, and device wall-clock uncertainty. Active-time calculations use monotonic time locally; wall-clock timestamps support ordering and display but are not trusted alone for policy enforcement.

The implemented [daily time-policy core](daily-time-policy.md) consumes equivalent local lifecycle calls but does not yet persist this event vocabulary. A verified Onion lifecycle adapter must establish when `GameStarted`, `GamePaused`, `GameResumed`, and `GameExited` are true before those events become an authoritative journal.

Payload schemas, retention periods, journal compaction, and synchronization conflict handling remain unresolved until an implemented local journal requires them.
