# TurretMind — Target Acquisition on a Budget

**One targeting service for hundreds of turrets: spatial queries instead of actor loops, line-of-sight
spread over frames, lead prediction, and a hard per-frame budget.**

---

## The problem

The naive turret asks `GetAllActorsOfClass`, sorts the result and pulls a trace — per turret, per
frame.

Forty turrets and two hundred attackers is **eight thousand distance checks and forty line traces every
frame**, before a single shot is fired. It is exactly where tower defence prototypes fall over once the
wave gets big, and it is the same eight thousand checks whether the attackers are next to the turret or
on the other side of the map.

## What TurretMind does instead

**Targets live in a grid, not a list.** A uniform spatial index over the XY plane. A turret with 4000 cm
of range opens the cells its range actually covers — nearly all of them empty — and looks at the dozen
or so targets inside. The statistics box puts that number on screen next to what the naive loop would
have cost.

**Re-evaluation is rationed.** A cursor walks the turret list under a hard per-frame budget. A turret
that does not get a turn keeps the target it already has. Nothing stalls.

**Aiming is not rationed.** Every barrel updates every frame, whatever the budget is doing. Drop the
budget to four and the turrets keep tracking smoothly — they just change their minds less often. That
separation is the whole design.

**Line-of-sight traces are spread over frames and cached.** Each turret/target verdict stays good for a
configurable window; traces have their own budget on top of the acquisition budget, because a trace
costs an order of magnitude more than a distance comparison.

**Hysteresis stops the flicker.** A new target has to score a configurable percentage better than the
held one. Without it, two equally good attackers make the barrel snap back and forth every
re-evaluation — the failure everyone hits and nobody expects.

**Lead prediction.** Hand it a projectile speed and it gives you the intercept point instead of the
point the target is standing on. Zero speed means hitscan and the lead is skipped.

## Six ways to rank a target

`Nearest` · `Lowest Health` · `Highest Threat` · `Furthest Along Path` · `First Seen` · `Custom`

The policy lives on the turret's profile, not on the service, so the machinegun can shoot whatever is
closest while the missile battery next to it goes for the biggest threat and the mortar at the back
picks whoever has come furthest along the path. Every policy produces a fitness in 0..1, so one
stickiness setting means the same thing under all of them.

`Custom` is a Blueprint delegate that receives a flat snapshot of the turret and the candidate and
returns a number.

## Measure it, do not take my word for it

An on-screen statistics box drawn on `UCanvas` from `AHUD` — so it works in a cooked Shipping build,
not just in the editor:

```
Turrets            40  (38 on target)
Targets            200
Acquisitions       16 / 16
Line traces        8 / 8
Cells visited      212
Targets considered 147  (naive 3200)
Switches           2
Acquire            0.184 ms
```

Plus world-space debug drawing — range circles, muzzle-to-target lines, the lead point as a cross — and
console commands for every budget and override, so you can turn the knobs while the wave is running.

## Install in five minutes

1. **TurretMind Turret** component on the turret. It does not tick.
2. **TurretMind Target** component on everything shootable. It does not tick either.
3. Pick a profile — `Cannon`, `Machinegun` or `Missile` ship with the plugin.
4. Read `Get Aim Rotation` in your turret's tick and turn the barrel.

Both components register themselves. Velocity is measured, not read off a movement component, so a
target is allowed to be anything that moves — including an actor that just sets its own transform.

## What it does NOT do — read this part

* **It does not shoot.** No projectiles, no damage, no hit detection. TurretMind decides *what* is aimed
  at and *where to lead*; what happens after the trigger is yours. If you want the other half, Terminal
  Ballistics, XProjectile and Miss No Hit sit next to this one very comfortably.
* **No turret meshes and no animations.** The demo map uses primitive shapes so you can see the
  targeting rather than the art.
* **No turret building or placement, no wave system, no economy, no tower defence template.** This is a
  service you drop into your game, not a game.
* **No network replication.** The service runs where the targets are known — the server. Each client
  displays what its own code tells it.
* **No Behavior Tree nodes and no perception system.** Deliberately independent of `AIModule`, so it
  works in a project that never touches either.
* **The grid is 2D (XY).** The cell lookup is flat; Z is part of the range test but not of the cell
  index. In a level with heavily stacked floors — a space station, a high-rise — one cell holds every
  floor at once and a turret will consider targets it cannot reach. Raise the cell size or shorten the
  ranges. That is the honest shape of the trade, and it is in the documentation too.
* **No editor utility widgets.** Everything is runtime.

## Technical

* One runtime module. No third-party libraries, no editor module.
* Dependencies: `Core`, `CoreUObject`, `Engine`, `DeveloperSettings`, `RenderCore`. No `UMG`, no
  `AIModule`, no `UnrealEd`.
* Blueprint and C++, fully documented headers.
* Win64 — built and verified with `RunUAT BuildPlugin` for this release, and the only entry in
  the `.uplugin`'s `PlatformAllowList`. The code contains nothing platform-specific, so adding
  Mac, Linux or a console to that list and building it yourself is a one-line change — but those
  platforms were not built here and are therefore not claimed as supported.
* Demo map with a tower defence arena: forty turrets, two hundred moving targets, sight-blocking walls,
  and a click-driven HUD for the budgets and policies.
* Unreal Engine 5.8.
