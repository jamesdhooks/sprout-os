# Sprout Platform
## Product Design and Technical Architecture Blueprint

**Status:** Foundational design document  
**Audience:** Product owner, technical lead, engine developers, OS/frontend developers, backend developers, game developers, contributors  
**Primary target:** Miyoo Mini Plus running an OnionOS-derived distribution  
**Additional targets:** Raspberry Pi, Windows, Linux, and browser/WebAssembly  
**Core concept:** A modern, family-oriented game platform that combines retro emulation, native microgames, profiles, parental controls, recommendations, media, and optional home-server integration.

---

# 1. Executive Summary

Sprout is a family-focused gaming platform built initially for the Miyoo Mini Plus, but designed from the outset as a portable ecosystem rather than a single-device firmware modification.

The initial implementation should be based on OnionOS infrastructure because Onion already solves several difficult device-specific problems:

- emulator configuration;
- RetroArch integration;
- save-state lifecycle;
- GameSwitcher behavior;
- power and sleep handling;
- button shortcuts;
- system applications;
- network services;
- package management;
- activity tracking.

Sprout should replace the user-facing experience while preserving Onion's stable lower-level capabilities wherever practical.

Sprout is not merely a launcher skin. It introduces foundational concepts that existing retro handheld firmware generally lacks:

- named user profiles;
- child and parent roles;
- profile-specific saves, favorites, recents, and recommendations;
- parental controls and daily screen-time limits;
- persistent parent unlock grants;
- remote time extensions;
- a unified catalogue containing emulated games, native games, media, and applications;
- server-backed statistics and recommendations;
- a portable native microgame platform;
- professional development, testing, packaging, publishing, and update workflows.

The platform should be split into independently versioned components:

1. **SproutOS** — the handheld distribution and family-first frontend.
2. **Sprout Runtime** — the cross-platform runtime for native Sprout games.
3. **Sprout Arcade** — the package catalogue and distribution channel for native games.
4. **Sprout Studio** — desktop/browser tooling for developing, testing, packaging, and publishing games.
5. **Sprout Server** — optional self-hosted services for synchronization, analytics, recommendations, media integration, and remote parental controls.
6. **Sprout SDK** — manifests, schemas, APIs, templates, validation tools, and documentation.

This separation is critical. SproutOS must be able to install new games without rebuilding the OS, while developers must be able to create and test games on Windows or in a browser before deploying them to a Miyoo or Raspberry Pi.

---

# 2. Product Vision

## 2.1 Product statement

> Sprout is a modern family console experience for retro handhelds and small computers.

It should make a device such as the Miyoo Mini Plus feel less like a folder browser full of ROMs and more like a cohesive family console.

## 2.2 Core product principles

### Family-first

Every person should have a profile with:

- a name;
- portrait or avatar;
- role;
- save namespace;
- favorites;
- recents;
- recommendations;
- play history;
- accessibility settings;
- content permissions;
- screen-time rules.

### Child-appropriate by design

Child mode should:

- hide all settings;
- avoid exposing platform complexity;
- avoid long text lists;
- emphasize artwork and visual recognition;
- provide simple controls;
- make failure gentle;
- support instant resume;
- show remaining screen time;
- prevent accidental access to parent or system tools.

### Parent-controlled but not server-dependent

Parent access and screen-time rules must work offline.

The server should enhance the experience, not be required for basic operation.

### One catalogue, many content types

Sprout should treat content as launchable library items rather than assuming everything is a ROM.

A unified library may contain:

- emulated games;
- native Sprout games;
- media applications;
- photo viewers;
- audiobooks;
- Jellyfin collections;
- utilities;
- future community applications.

### Portable content

Sprout native games should run unchanged, or with minimal platform-specific adaptation, across:

- Miyoo Mini Plus;
- Raspberry Pi;
- Windows;
- Linux;
- browser/WebAssembly.

### Professional engineering

The platform should use:

- reproducible builds;
- versioned schemas;
- package signing;
- compatibility checks;
- automated tests;
- release channels;
- telemetry opt-in;
- CI/CD;
- crash recovery;
- migration tooling;
- rollback support;
- clear security boundaries.

---

### User-configurable without file editing

Core household setup must be achievable from the Sprout UI or a companion management interface. Users should not need to edit JSON, rename internal directories, resize profile images, or discover system paths for ordinary setup and administration.

Advanced users may edit exported configuration files or manage packages directly, but manual file editing must remain optional.

### Provider-neutral integrations

Sprout must not assume a particular media server, photo library, home automation system, calendar, cloud provider, or storage product. External systems should be integrated through explicit connector interfaces with:

- capability discovery;
- provider-specific configuration forms;
- per-profile permissions;
- secure credential storage;
- offline and cached behavior;
- clear unsupported-feature handling;
- removable connectors that cannot destabilize core OS functions.


# 3. Naming and Product Structure

The core brand should be **Sprout**.

Recommended product names:

- **SproutOS** — the handheld operating environment.
- **Sprout Runtime** — the portable game runtime.
- **Sprout Arcade** — the native game catalogue.
- **Sprout Studio** — the developer environment.
- **Sprout Sync** — device/server synchronization.
- **Sprout Family** — parental controls, statistics, recommendations, and dashboard.
- **Sprout SDK** — developer-facing APIs and tooling.

The name carries several useful meanings:

- a subtle connection to Onion through the plant/growth theme;
- children as “sprouts”;
- learning and development;
- games and recommendations growing over time;
- an ecosystem that can extend beyond the Miyoo.

Before public release, perform a formal naming and trademark review. Existing unrelated products using “SproutOS” may not create practical conflict for an open-source gaming platform, but search discoverability and future commercialization should still be considered.

---

# 4. Scope and Non-Goals

## 4.1 Initial scope

Sprout 1.0 should focus on:

- replacement family-first frontend;
- real profiles;
- child and parent roles;
- profile-specific favorites and recents;
- profile-specific saves where feasible;
- Onion-compatible game launching;
- preservation of GameSwitcher;
- parental controls;
- screen-time enforcement;
- persistent parent unlock modes;
- native Sprout Arcade games;
- package installation and updates;
- recommendation feeds;
- statistics and basic server sync;
- Raspberry Pi and desktop runtime support.

## 4.2 Explicit non-goals for initial releases

Do not initially attempt to build:

- a full general-purpose Linux desktop;
- a modern 3D engine;
- a Steam competitor;
- a commercial app marketplace;
- a large-scale cloud service;
- a custom emulator stack;
- an advanced visual scripting editor;
- a complete replacement for every Onion system application;
- a complex AI model running directly on the handheld.

Sprout should reuse proven infrastructure wherever possible and add value at the user experience, profile, content, policy, and distribution layers.

---

# 5. High-Level Architecture

```text
+--------------------------------------------------+
|                  Sprout Ecosystem                |
+--------------------------------------------------+

  SproutOS
  ├── Sprout Launcher
  ├── Profile Service
  ├── Policy Agent
  ├── Catalogue Service
  ├── Recommendation Client
  ├── Package Manager
  ├── Media Applications
  ├── Onion Compatibility Layer
  └── Recovery / Safe Mode

  Sprout Runtime
  ├── Graphics
  ├── Input
  ├── Audio
  ├── Storage
  ├── Animation
  ├── Tile/Grid Systems
  ├── Achievements
  ├── Analytics Events
  └── Platform Adapters

  Sprout Arcade
  ├── Package Index
  ├── Signed Game Packages
  ├── Metadata
  ├── Screenshots / Artwork
  ├── Compatibility Data
  └── Release Channels

  Sprout Studio
  ├── Desktop Player
  ├── Browser Player
  ├── Tile Editor
  ├── Package Builder
  ├── Validator
  ├── Profiler
  ├── Replay Testing
  └── Publishing Client

  Sprout Server (optional)
  ├── Sync API
  ├── Family API
  ├── Usage Ingestor
  ├── Recommendation Engine
  ├── Package Mirror
  ├── Jellyfin Integration
  ├── Notifications
  └── Web Dashboard
```

---

# 6. SproutOS Architecture

## 6.1 Relationship with OnionOS

SproutOS should initially be implemented as an Onion-derived distribution with a replacement frontend and additional background services.

Reuse Onion for:

- emulator configurations;
- RetroArch builds and cores;
- GameSwitcher;
- auto-save and auto-resume;
- package definitions;
- system shortcuts;
- power and sleep handling;
- Wi-Fi services;
- Tweaks;
- Package Manager;
- Theme Switcher;
- Activity Tracker;
- native utilities;
- recovery access to stock MainUI.

Replace or extend:

- profile selection;
- home screen;
- game browser;
- favorites;
- recents presentation;
- recommendations;
- parental controls;
- content permissions;
- native game support;
- media browsing;
- server synchronization;
- package repository behavior.

## 6.2 SproutOS process model

Recommended long-running or core components:

```text
sprout-launcher
sprout-profile-service
sprout-policy-agent
sprout-package-agent
sprout-sync-agent
sprout-event-journal
sprout-onion-adapter
```

Not all need to be independent daemons in the first implementation. Early versions may combine them into one launcher process plus one policy supervisor.

## 6.3 Frontend ownership

Sprout Launcher should be the default visible frontend.

Expected flow:

```text
Power on
  ↓
Sprout splash
  ↓
Profile selector
  ↓
Profile home
  ↓
Launch game or application
  ↓
Onion/RetroArch or Sprout Runtime
  ↓
Return to GameSwitcher or Sprout home
```

A safe-mode boot gesture must bypass Sprout and open stock Onion.

Recommended default:

```text
Hold B during boot → Onion compatibility mode
```

## 6.4 Screen architecture

Suggested screen set:

- ProfileSelect
- ChildHome
- AdultHome
- RecentAndFavorites
- GameBrowser
- PlatformBrowser
- Recommendations
- GameDetails
- MediaHome
- JellyfinLibrary
- PhotoLibrary
- ScreenTimeStatus
- AskForTime
- ParentUnlock
- ParentDashboard
- PackageBrowser
- DeviceSettingsGateway
- RecoveryScreen

Each screen should be implemented through a shared navigation and rendering layer rather than ad hoc logic.

---

# 7. User Configuration and First-Run Experience

Sprout must treat configuration as a first-class product surface rather than a collection of files users are expected to understand.

The system should support three configuration surfaces:

1. **On-device setup** for essential tasks.
2. **Sprout Manager** on desktop or the local web interface for richer administration.
3. **Portable configuration archives** for backup, migration, automation, and advanced users.

All three surfaces should use the same underlying configuration APIs, schemas, validation rules, and permission model.

## 7.1 First-run setup

On first boot, Sprout should launch a guided and resumable setup flow:

```text
Welcome to Sprout
  ↓
Language, region, and time zone
  ↓
Connect to Wi-Fi or continue offline
  ↓
Create first parent administrator
  ↓
Set parent PIN
  ↓
Create household profiles
  ↓
Choose or import profile images
  ↓
Discover/import game libraries
  ↓
Select child-safe defaults
  ↓
Optionally add external-service connectors
  ↓
Review and finish
```

Requirements:

- Every step except creation of at least one parent administrator must be skippable.
- Offline setup must be fully supported.
- The wizard must survive reboot or power loss and continue from the last completed step.
- Child profiles should receive conservative defaults.
- Technical details such as ROM paths, API tokens, save namespaces, transcoding profiles, and connector capabilities should remain behind an Advanced option.
- The completed wizard must produce a valid local-only configuration even when no external services are connected.

## 7.2 Profile creation UI

Normal profile creation:

```text
Parent Dashboard
  → Profiles
  → Add Profile
```

Profile fields:

- display name;
- role: child, teen, adult, parent, or guest;
- optional birthday or age band;
- profile image or built-in avatar;
- theme/accent;
- content permissions;
- screen-time preset;
- library visibility;
- save-data isolation mode;
- optional synchronization.

Recommended presets:

- Preschool child;
- School-age child;
- Teen;
- Adult;
- Parent administrator;
- Temporary guest.

Presets should provide reasonable defaults without preventing later customization.

## 7.3 Profile images and avatars

Adding a custom profile picture must not require manual resizing or copying into hidden folders.

Supported flows:

### Built-in avatars

Provide a neutral built-in collection:

- animals;
- plants;
- simple characters;
- initials;
- abstract shapes;
- color-coded symbols.

### Sprout Manager upload

The desktop/local-web manager should support:

- drag-and-drop upload;
- selecting a phone or computer photo;
- crop and zoom;
- optional background removal;
- preview at actual handheld dimensions;
- assignment to one or more profiles.

### SD-card import

On-device flow:

```text
Profiles
  → Edit Profile
  → Change Picture
  → Import from SD Card
```

Sprout should scan friendly import locations such as:

```text
/Sprout/Imports/ProfileImages/
/Pictures/
/DCIM/
```

### Automatic image processing

Sprout should:

- accept PNG, JPEG, and WebP where supported;
- respect orientation metadata;
- crop to the configured frame;
- generate transparent and opaque variants where useful;
- generate all required thumbnails;
- preserve the original imported image;
- avoid repeatedly decoding full-resolution sources.

Recommended generated variants:

```text
avatar-original.*
avatar-square-256.png
avatar-device-128.png
avatar-cutout-128.png
avatar-list-64.png
```

Background removal should be optional and may be performed locally, by a user-selected connector, or not at all.

## 7.4 Profile-image privacy

Profile images may be personal data. Therefore:

- store locally by default;
- never upload externally without explicit consent;
- identify which connector will receive the image;
- allow users to delete originals;
- allow built-in avatars instead;
- include images in backups only according to backup settings;
- encrypt backups containing personal images when configured.

## 7.5 Profile editing and lifecycle

Parent UI should support:

- rename;
- replace avatar;
- change role;
- change content access;
- change time policy;
- change save isolation;
- archive;
- export;
- merge guest progress;
- delete with confirmation;
- restore recently deleted profiles.

Deletion should be staged:

```text
active → archived → deleted after retention window
```

Saves, history, and images should not be permanently erased without a second explicit confirmation.

## 7.6 Configuration precedence

Configuration should have explicit inheritance:

```text
platform defaults
  ↓
household settings
  ↓
device settings
  ↓
profile settings
  ↓
library-item overrides
  ↓
temporary session grants
```

The UI should show whether a value is inherited or overridden.

Example:

- Household default: 45 minutes daily.
- Device override: living-room device allows multiplayer until 8:00 p.m.
- Profile override: one child receives 30 minutes.
- Game override: an approved learning game uses a separate budget.
- Session grant: parent adds 15 minutes.

## 7.7 Configuration storage model

Use SQLite for transactional state and versioned JSON for portable definitions.

Recommended layout:

```text
/Sprout/
├── config/
│   ├── device.json
│   ├── household.json
│   ├── connectors.json
│   ├── libraries.json
│   └── schema-version.json
├── data/
│   ├── sprout.db
│   ├── profiles/
│   ├── packages/
│   ├── artwork-cache/
│   ├── media-cache/
│   └── connector-cache/
├── imports/
│   ├── profile-images/
│   ├── packages/
│   └── library-metadata/
├── exports/
├── backups/
└── logs/
```

User-facing file management should use concepts such as “Profile backup,” “Imported pictures,” and “Game library,” not internal database paths.

## 7.8 Configuration schemas

All configuration must use:

- explicit schema versions;
- stable IDs;
- migration rules;
- validation;
- descriptive errors;
- documented defaults;
- preservation of unknown fields where practical.

Example connector definition:

```json
{
  "id": "media-main",
  "type": "media-server",
  "provider": "example-provider",
  "displayName": "Family Media",
  "enabled": true,
  "endpoint": "https://media.example.local",
  "credentialRef": "secret://media-main",
  "capabilities": [
    "browse-video",
    "resume-playback",
    "transcode-video"
  ]
}
```

Core Sprout code must depend on connector capabilities, not provider names.

## 7.9 Secrets and credentials

Secrets must not be embedded directly in ordinary JSON.

Use a local secret store with:

- encryption where feasible;
- references from normal configuration;
- connector-specific revocation;
- health/status reporting;
- import/export controls;
- support for short-lived server tokens.

Connection states should be explicit:

- connected;
- authentication expired;
- unreachable;
- unsupported configuration;
- offline with cached data.

## 7.10 Configuration import and export

### Household export

May contain:

- profiles;
- permissions;
- screen-time rules;
- library metadata;
- connector definitions without secrets by default;
- themes;
- favorites;
- recommendation preferences.

### Full encrypted backup

May additionally contain:

- saves;
- save states;
- profile images;
- credentials;
- usage history;
- native-game data.

### Profile-only export

Used for:

- moving a profile between devices;
- sharing progress between Miyoo and Raspberry Pi;
- restoring one person;
- creating a travel device.

Exports should be versioned archives with manifests and checksums.

## 7.11 Backup and restore UI

Parent path:

```text
System
  → Backup and Restore
```

Actions:

- Back up now;
- schedule backup;
- export to SD card;
- send to a configured storage connector;
- restore household;
- restore one profile;
- restore saves only;
- verify backup;
- view last successful backup.

A local no-service-required backup path must always exist.

## 7.12 Library setup and file management

Library management should support both automatic and guided workflows.

### Automatic discovery

Sprout scans configured roots for:

- Onion ROM folders;
- BIOS folders;
- Sprout packages;
- ports;
- artwork;
- registered applications;
- supported media sources.

### Guided library import

```text
Libraries
  → Add Library
  → Games / Native Games / Media / Photos / Applications
```

For an emulated-game library:

- choose root folder;
- detect platform structure;
- map folders to systems;
- preview detected titles;
- resolve duplicates;
- assign default profile visibility;
- import metadata and artwork;
- test-launch a sample.

### Parent-only file manager

May provide:

- copy;
- move;
- rename;
- delete to recycle bin;
- checksums;
- free-space inspection;
- package import;
- cache rebuild;
- index repair.

Child mode must never expose filesystem access.

## 7.13 Sprout Manager

Sprout should provide a generic management interface available as:

- Windows desktop application;
- Linux desktop application;
- local-network browser interface;
- mobile-friendly web view.

Core functions:

- create and edit profiles;
- upload/crop profile pictures;
- manage libraries;
- approve or hide content;
- configure screen time;
- grant extra time;
- install and update packages;
- configure connectors;
- monitor storage;
- back up and restore;
- inspect logs;
- test integrations;
- preview the handheld UI.

The companion interface must remain optional.

## 7.14 Generic connector model

External services must use connector contracts.

Connector categories:

- media server;
- photo library;
- music or audiobook server;
- home automation;
- calendar;
- notification service;
- backup or storage provider;
- identity provider;
- recommendation metadata provider;
- custom HTTP/local service.

Example connector declaration:

```json
{
  "connectorType": "media-server",
  "provider": "example-provider",
  "capabilities": [
    "browse-video",
    "stream-video",
    "resume-playback",
    "transcode-video"
  ],
  "configurationSchema": {},
  "healthCheck": {}
}
```

Provider-specific connectors are allowed, but provider names must not leak into core domain models.

## 7.15 Capability-based behavior

Core capabilities may include:

```text
browse-video
stream-video
transcode-video
browse-photos
browse-audio
control-approved-scenes
read-calendar
send-notification
store-backup
```

Graceful degradation examples:

- A connector with video browsing but no transcoding can expose only compatible streams.
- A read-only photo connector cannot offer upload.
- A home automation connector can expose approved scenes without unrestricted device control.

## 7.16 Connector setup UI

```text
Parent Dashboard
  → Connections
  → Add Connection
  → Choose category
  → Choose provider
  → Enter connection details
  → Test connection
  → Select capabilities/libraries
  → Assign profile access
  → Save
```

Third-party connector packages may be supported later.

## 7.17 Recovery and last-known-good configuration

Sprout must remain recoverable from corrupt or incompatible configuration.

Requirements:

- validate before activation;
- atomic writes;
- automatic snapshots;
- last-known-good rollback;
- connector-by-connector disable;
- launcher-only reset;
- recovery UI after repeated boot failures;
- no requirement to erase saves to repair configuration.

# 8. User Experience Design

## 7.1 Profile selector

The boot landing screen should show cutout family portraits or avatars.

Controls:

- D-pad: move focus;
- A: select;
- B: back where applicable;
- hidden gesture: open parent authentication.

Child profiles should be visible.

Parent mode should not necessarily appear as a normal selectable profile. A better model is:

```text
Visible:
- Child 1
- Child 2

Hidden:
Hold Menu + Select → Parent unlock
```

Adult profiles may optionally be shown in households where this is preferred.

## 7.2 Child home

The child home should be one simple vertical page:

```text
Continue
Favorites
Recommended
See All
Media
Ask for More Time
```

Recent and favorite games should appear in one continuous list or a small number of horizontal rows.

No settings should be visible.

## 7.3 Adult home

Adult mode may expose:

- Continue;
- Favorites;
- Recommendations;
- Platforms;
- All Games;
- Sprout Arcade;
- Media;
- Family Dashboard;
- Onion Tools;
- Device Settings;
- Package Manager.

## 7.4 “See All” behavior

For adults:

```text
See All
  ↓
Platform selection
  ↓
Game browser
```

For children:

```text
See All
  ↓
Combined visual catalogue
```

Default child ordering should be a stable randomized order rather than alphabetical or platform-based.

Recommended approach:

```text
shuffle seed = profile ID + calendar date
```

This gives daily variety while keeping the order stable during the session.

## 7.5 Filters

Child filters should be visual and optional:

- puzzle icon;
- racing icon;
- easy icon;
- two-player icon;
- favorites star;
- random dice;
- Turnip/Sprout Arcade icon.

Adult filters can include:

- platform;
- genre;
- year;
- players;
- favorites;
- recently added;
- completion;
- playtime;
- native/emulated;
- kid-safe.

---

# 9. Profile System

## 8.1 Profile model

Example:

```json
{
  "id": "child-1",
  "displayName": "Alex",
  "role": "child",
  "avatar": "profiles/alex.png",
  "theme": "forest",
  "saveNamespace": "child-1",
  "libraryPolicyId": "kids-default",
  "timePolicyId": "weekday-child",
  "recommendationProfileId": "child-1",
  "pinRequired": false
}
```

Parent profile:

```json
{
  "id": "james",
  "displayName": "James",
  "role": "parent",
  "avatar": "profiles/james.png",
  "saveNamespace": "james",
  "libraryPolicyId": "unrestricted",
  "pinRequired": true
}
```

## 8.2 Profile-scoped data

Each profile should have independent:

- favorites;
- recents;
- play activity;
- recommendation history;
- native-game saves;
- content permissions;
- time limits;
- achievements;
- difficulty preferences;
- accessibility settings.

Where practical, also isolate:

- emulator save files;
- save states;
- remappings;
- GameSwitcher history.

## 8.3 Profile storage layout

```text
Sprout/
├── profiles/
│   ├── child-1/
│   │   ├── profile.json
│   │   ├── favorites.json
│   │   ├── recents.json
│   │   ├── settings.json
│   │   ├── saves/
│   │   ├── states/
│   │   └── activity/
│   └── james/
└── shared/
```

Prefer SQLite for transactional state and JSON for human-readable manifests and configuration.

---

# 10. Parent Authentication and Permissions

## 9.1 Access model

Child mode should use a strict allowlist.

Do not simply hide settings. Prevent child mode from launching unauthorized tools.

Blocked in child mode:

- Onion Settings;
- Tweaks;
- Package Manager;
- RetroArch menus;
- File Explorer;
- Terminal;
- Theme settings;
- Wi-Fi settings;
- clock/date settings;
- parent dashboard;
- unrestricted library;
- administrative package operations.

## 9.2 Parent unlock methods

Support:

- local PIN;
- phone approval through Sprout Family;
- optional one-time QR approval;
- server-issued temporary grants.

Offline fallback must always exist through the local PIN.

## 9.3 Persistent unlock grants

After entering the PIN, offer:

- 15 minutes;
- 1 hour;
- until bedtime;
- until end of day;
- until manually locked.

“Until end of day” must survive reboot.

Example grant:

```json
{
  "scope": "parent-ui",
  "deviceId": "miyoo-01",
  "validThroughLocalDate": "2026-08-02",
  "issuedAt": 1785695400,
  "manuallyRevoked": false,
  "signature": "..."
}
```

Manual lock should revoke the grant immediately and return to the profile selector.

## 9.4 Reauthentication for sensitive actions

Even during an unlocked parent session, re-prompt for:

- changing the PIN;
- disabling parental controls;
- deleting profiles;
- clearing usage history;
- enabling terminal access;
- entering unrestricted RetroArch settings;
- modifying trusted server keys;
- removing policy enforcement.

---

# 11. Parental Controls

## 10.1 Policy model

Per profile:

- daily minutes;
- session maximum;
- mandatory breaks;
- weekday/weekend rules;
- allowed hours;
- per-category budgets;
- blocked content;
- bedtime;
- bonus time;
- offline grace behavior.

Example:

```json
{
  "profileId": "child-1",
  "dailyMinutes": 45,
  "sessionMinutes": 25,
  "breakMinutes": 20,
  "allowedWindow": {
    "start": "08:00",
    "end": "19:30"
  },
  "categories": {
    "games": 45,
    "video": 20,
    "photos": null
  }
}
```

## 10.2 Enforcement design

Enforcement must not exist only in the launcher.

The policy agent should monitor:

- active emulator process;
- active Sprout Runtime game;
- active media player;
- GameSwitcher resumes;
- active profile;
- screen/suspend state;
- current allowance.

Any unauthorized launch should be blocked or immediately exited through the normal save-and-exit path.

## 10.3 Timer behavior

Show:

- remaining daily time on the child home;
- optional in-game overlay;
- 10-minute warning;
- 5-minute warning;
- 1-minute warning;
- end-of-session screen.

At zero:

```text
pause
→ save normally
→ exit through Onion-compatible path
→ return to locked child home
```

## 10.4 Remote time extension

Sprout Family should allow:

```text
+5 minutes
+15 minutes
+30 minutes
pause limit
end session
```

The handheld may poll every 15–30 seconds or use WebSockets if reliable.

## 10.5 Offline operation

The device should cache:

- policy;
- remaining time;
- last trusted server time;
- local usage journal;
- valid extension grants.

Use monotonic time for active-session accounting.

Do not trust wall-clock changes blindly.

---

# 12. Unified Library Model

## 11.1 Library item abstraction

```text
LibraryItem
├── EmulatedGame
├── NativeGame
├── MediaApplication
├── UtilityApplication
└── ContentCollection
```

Example emulated game:

```json
{
  "id": "kirby-dream-land",
  "type": "emulated-game",
  "title": "Kirby's Dream Land",
  "platform": "GB",
  "rom": "/Roms/GB/Kirby.gb",
  "artwork": "kirby.png",
  "tags": ["kids", "easy", "platformer"]
}
```

Example native game:

```json
{
  "id": "sprout.push-box",
  "type": "native-game",
  "title": "Push the Box",
  "platform": "sprout-arcade",
  "package": "sprout.push-box@1.0.0",
  "artwork": "push-box.png",
  "tags": ["kids", "puzzle", "grid"]
}
```

Example media app:

```json
{
  "id": "jellyfin-kids",
  "type": "media-application",
  "title": "Cartoons",
  "executable": "/Apps/SproutJellyfin/launch.sh",
  "tags": ["kids", "video"]
}
```

## 11.2 Shared behavior

All games should support:

- favorites;
- recents;
- recommendations;
- playtime tracking;
- profile permissions;
- screen-time policy;
- artwork;
- search;
- filtering;
- daily picks.

Native games should additionally support:

- achievements;
- structured analytics;
- adaptive difficulty;
- portable saves;
- package updates.

---

# 13. Onion Integration

## 12.1 Launch adapter

Sprout must launch emulated games through Onion-compatible mechanisms, not raw RetroArch commands wherever possible.

The adapter must preserve:

- auto-save;
- auto-resume;
- GameSwitcher;
- screenshots;
- activity tracking;
- power safety;
- configured cores;
- overrides.

## 12.2 Return behavior

Desired flow:

```text
Sprout Launcher
  ↓
Onion launch adapter
  ↓
Game
  ↓
Menu → GameSwitcher
  ↓
Exit → Sprout Launcher
```

During early development, returning briefly through stock MainUI is acceptable if necessary.

## 12.3 GameSwitcher roadmap

Phase 1:

- keep GameSwitcher global and unchanged.

Phase 2:

- wrap GameSwitcher data per active profile.

Phase 3:

- fork GameSwitcher to support:
  - profile-aware recents;
  - native games;
  - generic launchable items;
  - policy checks;
  - profile-specific resume states.

---

# 14. Sprout Runtime

## 13.1 Purpose

Sprout Runtime is a constrained, portable game runtime for small native games.

It should prioritize:

- deterministic behavior;
- fast startup;
- tiny packages;
- simple input;
- one-screen or small-grid gameplay;
- portability;
- offline operation;
- strong tooling;
- safe package execution.

## 13.2 Target platforms

Required:

- Windows;
- Linux;
- Raspberry Pi;
- Miyoo Mini Plus.

Later:

- browser/WebAssembly;
- additional SDL-capable handhelds.

## 13.3 Runtime architecture

```text
Game package
  ↓
Sprout API
  ↓
Runtime core
  ↓
Platform adapter
```

Runtime core:

- timing;
- deterministic RNG;
- input actions;
- scenes;
- animation;
- tilemaps;
- grids;
- collision;
- particles;
- saves;
- achievements;
- analytics events.

Platform adapter:

- display;
- audio;
- controller mapping;
- filesystem;
- networking;
- power integration;
- platform-specific launch/exit behavior.

## 13.4 Logical resolution

Recommended baseline:

```text
320 × 240 logical canvas
```

Miyoo:

```text
320 × 240 → 640 × 480 at exact 2× scaling
```

Other platforms may use integer scaling with letterboxing.

## 13.5 Input abstraction

```text
Up
Down
Left
Right
Primary
Secondary
Pause
Back
```

Games must never depend directly on SDL scancodes, browser events, or device-specific buttons.

## 13.6 Language and scripting

Recommended long-term model:

- native runtime in C or C++;
- Lua scripting for game packages;
- optional native modules for advanced games.

Benefits:

- one package runs everywhere;
- no recompilation per game;
- easy hot reload;
- small packages;
- safer community publishing;
- faster iteration.

## 13.7 Core API

Suggested namespaces:

```text
sprout.graphics
sprout.input
sprout.audio
sprout.storage
sprout.time
sprout.random
sprout.grid
sprout.collision
sprout.animation
sprout.particles
sprout.profile
sprout.achievements
sprout.events
sprout.analytics
```

---

# 15. Graphics Architecture

## 14.1 Baseline renderer

Always available:

- sprites;
- tilemaps;
- text;
- primitives;
- alpha blending;
- rotation;
- scaling;
- color tint;
- sprite animation;
- particles;
- camera shake;
- transitions;
- layered composition.

## 14.2 Enhanced 2D

Optional but broadly portable:

- dynamic blob shadows;
- low-resolution light maps;
- parallax layers;
- pre-rendered 3D sprites;
- fake height;
- 2.5D three-quarter views;
- water and heat distortion approximations;
- palette effects;
- additive glow.

## 14.3 Advanced optional path

Capability-gated:

- OpenGL ES 2.0;
- simple shaders;
- render-to-texture;
- low-resolution framebuffer effects;
- basic mesh rendering;
- software low-poly 3D.

No game should require advanced effects unless its manifest explicitly declares that requirement.

## 14.4 Design direction

The recommended visual style is:

> polished 2.5D toy-box graphics with simple mechanics.

Use:

- pre-rendered 3D assets;
- soft shadows;
- small particles;
- eased motion;
- bold silhouettes;
- limited text;
- large readable objects.

---

# 16. Sprout Arcade Game Design

## 15.1 Design constraints

Each game should:

- be understandable in under ten seconds;
- use D-pad plus at most A/B;
- avoid complex menus;
- start immediately;
- run for 20 seconds to 5 minutes per round;
- support instant replay;
- avoid harsh failure;
- be procedural or replayable where practical;
- support adaptive difficulty;
- work offline;
- emit structured play events.

## 15.2 Initial game set

Recommended first six:

1. Snake
2. Find the Exit
3. Catch the Fruit
4. Lights Out
5. Memory Sequence
6. Flood Fill

Recommended first top-down set:

1. Push the Box
2. Key and Door
3. Mow the Lawn
4. Feed the Animals

These validate most foundational runtime systems.

## 15.3 Shared grid-game module

Core entities:

```text
wall
floor
player
pushable block
goal
key
door
switch
collectible
hazard
ice
teleporter
```

Level format:

```text
########
#P..B.G#
#......#
########
```

Rules should be data-driven.

---

# 17. Package Format

## 16.1 Package extension

Recommended:

```text
.sprout
```

Example:

```text
push-box.sprout
```

## 16.2 Package contents

```text
manifest.json
game.lua
assets/
localization/
icon.png
cover.png
screenshots/
licenses/
```

## 16.3 Manifest

```json
{
  "id": "sprout.push-box",
  "title": "Push the Box",
  "version": "1.0.0",
  "runtimeVersion": "1",
  "players": {
    "minimum": 1,
    "maximum": 1
  },
  "logicalResolution": [320, 240],
  "categories": ["kids", "puzzle", "grid"],
  "minimumAge": 3,
  "capabilities": [
    "achievements",
    "adaptive-difficulty",
    "cloud-save"
  ],
  "requires": [],
  "optional": [
    "shaders"
  ]
}
```

## 16.4 Package guarantees

Packages should be:

- immutable by version;
- signed;
- hashed;
- reproducible;
- validated before publication;
- dependency-minimal;
- sandboxed where possible;
- compatible with declared runtime versions.

---

# 18. Sprout Arcade Distribution

## 17.1 Catalogue service

The catalogue should provide:

- package metadata;
- version information;
- compatibility;
- artwork;
- screenshots;
- changelogs;
- signatures;
- age and content metadata;
- recommendation tags.

## 17.2 Update behavior

SproutOS should:

- periodically fetch the index;
- support manual refresh;
- download packages in the background;
- verify signatures and hashes;
- stage updates;
- install atomically;
- retain previous versions for rollback;
- avoid updating active games.

## 17.3 Release channels

Support:

- stable;
- beta;
- development;
- local/private repository.

A family may point SproutOS at a private home-server package feed.

## 17.4 Publishing flow

```text
Develop
→ validate
→ test
→ package
→ sign
→ upload
→ catalogue review
→ publish
→ clients discover update
```

---

# 19. Sprout Studio

## 18.1 Desktop development environment

Primary development target should be Windows/Linux desktop.

Features:

- 640×480 or 320×240 preview;
- Miyoo input emulation;
- gamepad support;
- hot reload;
- asset reload;
- profiler;
- memory budget warnings;
- input visualization;
- save inspection;
- profile switching;
- offline simulation;
- slow-device simulation;
- screenshot/video capture.

## 18.2 Browser target

Long-term browser support should use:

- C++ runtime compiled to WebAssembly;
- the same Lua game packages;
- browser storage adapter;
- browser input adapter;
- Canvas/WebGL presentation.

Browser limitations must be abstracted away by the runtime.

## 18.3 Studio modules

- project templates;
- code editor integration;
- tilemap editor;
- sprite preview;
- animation timeline;
- package manifest editor;
- test runner;
- replay recorder;
- validator;
- publisher.

## 18.4 Headless testing

The runtime should support:

```text
load game
simulate N frames
inject input sequence
assert state
```

This allows deterministic regression tests.

---

# 20. Sprout Server

## 19.1 Server role

The server should be optional.

It enhances:

- synchronization;
- recommendations;
- statistics;
- remote controls;
- package hosting;
- media integration;
- notifications;
- family dashboards.

## 19.2 Recommended services

```text
sprout-api
sprout-sync
sprout-usage-worker
sprout-recommendation-worker
sprout-package-registry
sprout-notification-service
sprout-connector-host
sprout-web
```

Early versions can be one service plus PostgreSQL.

## 19.3 Event model

Examples:

```text
GameStarted
GamePaused
GameResumed
GameExited
PlaytimeUpdated
AchievementUnlocked
LevelCompleted
DifficultyChanged
RecommendationShown
RecommendationAccepted
TimeExtensionRequested
TimeExtensionGranted
```

## 19.4 Usage events

```json
{
  "deviceId": "miyoo-01",
  "profileId": "child-1",
  "itemId": "sprout.push-box",
  "event": "GameStarted",
  "timestamp": "2026-08-02T18:42:30-04:00"
}
```

Events should be journaled locally and uploaded later.

## 19.5 Recommendation engine

Start rule-based, not AI-first.

Inputs:

- age;
- profile role;
- play history;
- session length;
- completion;
- genre;
- difficulty;
- abandoned games;
- recent repetition;
- multiplayer context.

Later add LLM-assisted curation and explanation.

Never allow an LLM to directly grant access, alter parental policies, or install arbitrary packages without deterministic validation.

---

# 21. Media and External-System Integration

## 21.1 Media-server connectors

Sprout may ship official connectors for common media servers, but the platform must use the generic connector and capability model defined in the configuration section.

A media connector may support:

- Continue Watching;
- child-safe libraries;
- movies;
- series;
- recently added;
- resume position;
- watched state;
- server-side transcoding;
- profile-restricted accounts;
- offline metadata caching.

For the Miyoo display profile, playback should generally prefer:

- 640×480 or lower;
- H.264 where available;
- lightweight audio such as AAC;
- modest bitrate;
- low-resolution thumbnails;
- aggressive caching.

A Jellyfin connector may be provided as a reference implementation, but Jellyfin must never be a required dependency or an assumption embedded in Sprout's core domain model.

## 21.2 Photo-library connectors

Photo support should use the same provider-neutral connector approach and may offer:

- local albums;
- network or cloud-backed albums;
- cached thumbnails;
- daily memories;
- slideshows;
- profile-scoped collections;
- offline favorites;
- read-only or read/write capabilities according to the connector.

No specific photo platform should be required.

## 21.3 Other connector-backed applications

Potential optional modules include:

- music;
- audiobooks;
- illustrated stories;
- weather;
- family calendars;
- approved home-automation scenes;
- read-only chore lists;
- digital photo-frame mode;
- notification inboxes;
- custom household services.

These should be optional packages backed by declared connector capabilities rather than hard dependencies.


---

# 22. Repository Strategy

Recommended repositories:

```text
sprout-os
sprout-runtime
sprout-sdk
sprout-studio
sprout-arcade
sprout-server
sprout-docs
```

Alternative monorepo for early development:

```text
sprout/
├── os/
├── runtime/
├── sdk/
├── studio/
├── arcade/
├── server/
└── docs/
```

Recommended early choice: monorepo until APIs stabilize, then split only if release cadence and contribution patterns justify it.

## 21.1 Onion upstream strategy

Keep Onion-derived code isolated.

Recommended structure:

```text
third_party/onion
sprout/integration/onion
sprout/frontend
sprout/profile
sprout/policy
```

Avoid scattered modifications to upstream code.

Prefer:

1. reuse unchanged;
2. wrap;
3. patch minimally;
4. fork only where necessary.

---

# 23. Development Environment

## 22.1 Dev SD card workflow

Maintain two cards:

### Stable card

- normal Onion/Sprout stable installation;
- full library;
- real saves;
- daily use.

### Development card

- cloned Onion version;
- representative test ROMs;
- debug builds;
- SSH/SFTP;
- logs;
- experimental startup hooks.

Never hot-swap while powered or suspended.

## 22.2 Deployment loop

```text
develop on Windows/WSL
→ build desktop runtime
→ run automated tests
→ cross-compile ARM build
→ deploy over SFTP/rsync
→ restart Sprout component
→ collect logs
```

## 22.3 Data separation

```text
Apps/Sprout/
├── bin/
├── assets/
└── defaults/

Saves/Sprout/
├── sprout.db
├── profiles/
├── cache/
└── logs/
```

Application updates must never overwrite user data.

---

# 24. Build and Release Engineering

## 23.1 Reproducible builds

Every release should record:

- source commit;
- compiler version;
- toolchain version;
- dependency lockfiles;
- target architecture;
- build flags;
- package hashes.

## 23.2 CI pipeline

On every pull request:

- formatting;
- linting;
- unit tests;
- schema validation;
- package validation;
- desktop build;
- headless runtime tests;
- sample game tests;
- security scan;
- license scan.

On release:

- Windows runtime;
- Linux runtime;
- Raspberry Pi ARM64 runtime;
- Miyoo ARM runtime;
- browser WASM build;
- package index generation;
- signed artifacts;
- changelog;
- release notes.

## 23.3 Compatibility matrix

Maintain:

```text
SproutOS version
Sprout Runtime version
Onion base version
GameSwitcher version
Package schema version
Game API version
Server API version
```

Use semantic versioning.

---

# 25. Testing Strategy

## 24.1 Unit tests

- grid movement;
- push logic;
- flood fill;
- maze generation;
- save/load;
- profile isolation;
- policy calculations;
- package validation;
- signature verification.

## 24.2 Integration tests

- launch emulated game;
- return through GameSwitcher;
- save state preservation;
- profile switch;
- screen-time expiration;
- offline usage sync;
- package update rollback;
- parent unlock persistence;
- safe-mode boot.

## 24.3 Hardware tests

Test on:

- Miyoo Mini Plus revisions;
- multiple SD cards;
- weak Wi-Fi;
- no Wi-Fi;
- low battery;
- abrupt power loss;
- sleep/resume;
- repeated game switching;
- long-running sessions.

## 24.4 Replay tests

Record deterministic input sequences for native games and compare:

- resulting game state;
- score;
- completion;
- emitted events;
- save files.

---

# 26. Security Model

## 25.1 Threat model

Sprout parental controls are intended to prevent accidental or casual bypass by young children.

They are not intended to resist a technically skilled attacker with:

- physical access;
- SD-card access;
- source code access;
- custom firmware knowledge.

## 25.2 Security requirements

- hashed local PIN;
- signed persistent grants;
- package signatures;
- HTTPS where supported;
- restricted Jellyfin accounts;
- child-mode allowlists;
- no admin tokens in game packages;
- no arbitrary shell access from Lua;
- sandboxed package APIs;
- atomic state writes;
- tamper detection for critical policy files.

## 25.3 Package sandbox

Lua games should not receive:

- unrestricted filesystem access;
- shell execution;
- raw sockets;
- arbitrary native library loading;
- parent profile data;
- PIN or policy secrets.

Expose only capability-based runtime APIs.

---

# 27. Performance Guidelines

## 26.1 Miyoo baseline

Design to the weakest supported device.

Guidelines:

- 320×240 logical canvas;
- 30 or 60 FPS depending on game;
- minimal overdraw;
- limited alpha layers;
- pre-sized thumbnails;
- capped particle counts;
- deterministic fixed update;
- no mandatory network;
- no mandatory shader path;
- small decoded texture budget;
- avoid large JSON parsing during interaction.

## 26.2 Suggested budgets

Initial conservative budgets:

- memory per native game: 16–32 MB target;
- visible particles: under 100;
- active textures: small atlas-based sets;
- startup time: under 1 second where practical;
- save write: under 100 ms;
- game package: typically under 5 MB;
- microgame code: preferably tens to hundreds of KB.

---

# 28. Roadmap

## Phase 0 — Discovery and verification

- inspect Onion source and filesystem layout;
- identify exact launch paths;
- inspect GameSwitcher data;
- validate SDL/GLES options;
- build a minimal native app;
- validate desktop-to-Miyoo cross-compilation;
- document supported Onion version.

Deliverable: technical spike report.

## Phase 1 — Sprout Launcher and configuration prototype

- desktop 640×480 launcher;
- resumable first-run wizard;
- profile creation and editing;
- built-in avatars;
- imported image crop/thumbnail pipeline;
- profile selector;
- child/adult modes;
- recent/favorites mock data;
- visual game catalogue;
- Onion launch adapter prototype;
- safe-mode boot.

Deliverable: launchable dev-card prototype.

## Phase 2 — Profiles, configuration, and policy

- SQLite profile storage;
- versioned configuration schemas and migrations;
- household/device/profile inheritance;
- profile import/export;
- backup and restore;
- parent-only library/file management;
- generic connector framework with one reference connector;
- per-profile favorites and recents;
- local PIN;
- persistent parent grants;
- daily screen-time rules;
- usage journal;
- end-of-time save-and-exit.

Deliverable: usable family-controlled handheld.

## Phase 3 — Sprout Runtime MVP

- C/C++ runtime;
- Lua embedding;
- SDL rendering;
- input abstraction;
- audio;
- storage;
- grid engine;
- animation;
- package loader;
- Windows/Linux/Miyoo builds.

Deliverable: portable microgame runtime.

## Phase 4 — First Arcade games

Build:

- Snake;
- Find the Exit;
- Catch the Fruit;
- Lights Out;
- Memory Sequence;
- Flood Fill;
- Push the Box;
- Key and Door.

Deliverable: first Sprout Arcade collection.

## Phase 5 — Package repository and updates

- package schema;
- signatures;
- package index;
- download/install;
- rollback;
- private feed support;
- publishing CLI.

Deliverable: independent game distribution.

## Phase 6 — Server and recommendations

- usage ingestion;
- family dashboard;
- remote time grants;
- rule-based recommendations;
- sync;
- notification requests.

Deliverable: connected family experience.

## Phase 7 — Studio

- desktop player;
- browser preview;
- hot reload;
- tile editor;
- package validator;
- profiler;
- publisher.

Deliverable: professional game-development workflow.

## Phase 8 — Deeper Onion fork

- profile-aware GameSwitcher;
- profile-scoped save states;
- unified native/emulated recents;
- native games in GameSwitcher;
- Turnip/Onion MainUI dependency reduction;
- custom updater.

Deliverable: cohesive SproutOS distribution.

## Phase 9 — Raspberry Pi living-room edition

- controller mapping;
- multi-controller support;
- HDMI layouts;
- shared family profiles;
- enhanced presentation;
- multiplayer native games.

Deliverable: Sprout TV/Console mode.

---

# 29. Recommended First Milestone

The first serious milestone should be deliberately narrow:

## “Sprout Family Launcher MVP”

Features:

- boots from a dev SD card;
- runs a resumable first-run wizard;
- creates at least one parent and one child profile;
- supports built-in avatars and imported/cropped profile images;
- allows profile editing without manual file changes;
- shows family portraits;
- child profile;
- parent unlock;
- recent and favorites;
- one platform browser;
- discovers and configures a small test library;
- provides parent-only library and file-management entry points;
- exports and restores a profile configuration;
- launches one Game Boy and one SNES game through Onion;
- preserves GameSwitcher;
- returns to Sprout;
- local daily time limit;
- manual parent lock;
- persistent “unlock until end of day” grant;
- safe-mode boot into Onion.

Do not begin with Jellyfin, AI, browser Studio, or a full package store.

After this milestone proves the architecture, build Sprout Runtime and the first native games.

---

# 30. Technical Decision Summary

Recommended decisions:

- **Base distribution:** Onion-derived.
- **Frontend:** custom native Sprout launcher.
- **Rendering:** SDL-based, optional GLES path.
- **Game scripting:** Lua.
- **Runtime core:** C/C++.
- **Logical resolution:** 320×240.
- **State storage:** SQLite plus versioned JSON manifests.
- **Configuration UX:** first-run wizard, parent dashboard, optional Sprout Manager, import/export, backup/restore, and last-known-good recovery.
- **Profile images:** built-in avatars plus user-friendly import, crop, thumbnail generation, and local-by-default privacy.
- **External systems:** provider-neutral, capability-based connector contracts.
- **Server:** optional REST API with PostgreSQL.
- **Sync:** event journal plus periodic reconciliation.
- **Package format:** signed `.sprout` archive.
- **Package delivery:** versioned index with stable/beta/dev channels.
- **Desktop development:** first-class Windows/Linux runtime.
- **Browser:** later WebAssembly target.
- **Game design:** D-pad + A/B, short loops, deterministic, child-safe.
- **Onion strategy:** reuse, wrap, minimally patch, fork only where required.
- **Security:** practical child protection, not hardened anti-tamper DRM.
- **Recovery:** hold-button safe mode to stock Onion.
- **Roadmap:** launcher first, runtime second, packages third, server fourth, deep fork later.

---

# 31. Final Recommendation

Proceed with Sprout as a layered ecosystem, not as a single firmware repository.

The most important architectural boundary is:

```text
SproutOS provides the family console experience.
Sprout Runtime executes portable native games.
Sprout Arcade distributes content.
Sprout Studio creates and validates content.
Sprout Server enhances synchronization and intelligence.
Sprout Connectors integrate optional user-selected systems without coupling the platform to one vendor or household stack.
Onion remains the initial low-level emulation foundation.
```

The project should begin with the smallest complete vertical slice:

```text
profile
→ library
→ launch
→ GameSwitcher
→ return
→ time accounting
→ parent control
```

Only after that works reliably on physical hardware should the project expand into native games, package publishing, recommendations, media, and broader platform support.

This approach preserves the best parts of Onion, avoids rewriting stable emulation infrastructure prematurely, allows professional independent development of games, and creates a credible path from a personal family project into a reusable open-source gaming platform.
