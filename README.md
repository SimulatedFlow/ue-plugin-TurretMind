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

* [`Docs/DOCUMENTATION.md`](Docs/DOCUMENTATION.md) — install, policies, budgets, custom scoring,
  console commands, limits.
* [`Docs/Fab-Store-Description.md`](Docs/Fab-Store-Description.md) — what it is and what it explicitly
  is not.

## Requirements

Unreal Engine 5.8 · Windows, Mac, Linux · one runtime module, no third-party libraries.

---

Copyright 2026 Silvan Teufel. All Rights Reserved.
