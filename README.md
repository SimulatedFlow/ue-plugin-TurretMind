# TurretMind — Target Acquisition on a Budget

One targeting service for hundreds of turrets: spatial queries instead of actor loops, line-of-sight
spread over frames, lead prediction, and a hard per-frame budget.

**TurretMind does not shoot.** No projectiles, no damage, no hit detection. It decides what a turret
aims at and where to lead — the rest stays yours.

## Quick start

1. Add a **TurretMind Turret** component to your turret actor.
2. Add a **TurretMind Target** component to everything shootable.
3. Pick a profile (`Cannon`, `Machinegun`, `Missile` ship with the plugin, or make your own).
4. In your turret's tick, read `Get Aim Rotation` and turn the barrel.
5. `TurretMind.Stats 1` in the console to see what the service is doing.

Neither component ticks. The service ticks once per world and writes the answers into them.

## Why

Forty turrets and two hundred attackers, each turret running `GetAllActorsOfClass` plus a sort plus a
trace, is eight thousand distance checks and forty traces every frame. TurretMind puts targets in a
spatial grid, rations re-evaluation round-robin under a hard budget, caches and spreads line-of-sight
traces, and keeps aiming unbudgeted so nothing ever stands still.

## Documentation

**Online, free, no account needed:**
<https://wiki.teufel-engineering.com/en/TurretMind/documentation> — install, policies, budgets,
custom scoring, console commands, limits. The same manual ships with the plugin as
[`Docs/DOCUMENTATION.md`](Docs/DOCUMENTATION.md), so it is also available offline.

[`Docs/Fab-Store-Description.md`](Docs/Fab-Store-Description.md) describes what the plugin is and
what it explicitly is not.

## Requirements

Unreal Engine 5.8 · Windows · one runtime module, no third-party libraries.

---

Copyright 2026 Silvan Teufel. All Rights Reserved.

<!-- SF-STORE-BLOCK:BEGIN -->
## 🛒 Source-available — see before you buy

This repository contains the **full source** of a commercial Unreal Engine plugin. It is **source-available, not open source**: read it, evaluate it, then buy a license to use it. See **the Fab Content License Agreement / Unreal Engine EULA (purchase required)**.

**Get it / Buy:**
- Fab store — all our UE5 plugins: https://www.fab.com/sellers/Silvan%20Teufel

_This plugin does not have its own Fab listing yet — the store link above is where everything we currently sell lives._

### 📬 **Free UE5 Snippet-Pack**

10 ready-to-use C++/Blueprint building blocks (subsystems, versioned saves, async nodes, editor tooling) — MIT licensed. Get it by joining the newsletter — plus a heads-up when something new ships. Double opt-in, unsubscribe in one click, no address sharing.

👉 **[Get the free pack](https://silvan.teufel-engineering.com/newsletter/plugins/?q=gh)**

_© 2026 Silvan Teufel. All rights reserved._
<!-- SF-STORE-BLOCK:END -->
