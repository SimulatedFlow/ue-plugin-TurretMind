# TurretMind — Target Acquisition on a Budget

One targeting service for hundreds of turrets. Targets live in a spatial grid instead of a list every
turret walks, re-evaluation runs round-robin under a hard per-frame budget, line-of-sight traces are
spread over frames and cached, and lead prediction hands you the point to aim at.

**TurretMind does not shoot.** No projectiles, no damage, no hit detection. It answers two questions —
*what should this turret aim at* and *where should it lead* — and stops there.

---

## Install in five minutes

### 1. Put a component on the turret

Add a **TurretMind Turret** component to your turret actor (Blueprint or C++).

| Property | What it does |
|---|---|
| `Profile` | Ranges, policy, stickiness, projectile speed. Leave it empty to use the built-in fallback. |
| `Team Id` | Whose side this turret is on. Default `0`. |
| `Muzzle Offset` | Where the shot leaves the turret, in the actor's own space. Range, cone and traces start here. |
| `b Enabled` | Off takes the turret out of the round-robin without unregistering it. |

The component has **no tick**. That is deliberate: a turret that ticks is a turret that runs its own
query, and forty of those is the problem this plugin removes.

### 2. Put a component on everything shootable

Add a **TurretMind Target** component to each attacker.

| Property | What it does |
|---|---|
| `Team Id` | Whose side the target is on. Default `1`. |
| `Threat` | Only read by the `Highest Threat` policy. The unit is yours. |
| `Health` | 0..1, only read by `Lowest Health`. Push your own number in with `Set Health`. |
| `Path Progress` | 0..1, only read by `Furthest Along Path`. |
| `Radius` | Rough size in cm; keeps the line-of-sight trace out of the target's feet. |
| `Aim Offset` | Where on the actor a turret aims, in the actor's own space. |
| `b Targetable` | Off makes the actor invisible to turrets without unregistering it. |

Targets do not tick either. Velocity is **measured** between index samples, so an actor that is moved by
setting its transform still produces a usable lead — asking a movement component would hand back zero.

Have an actor whose class you do not own? `TurretMind → Register Target (Actor, Team Id, Threat,
Radius)` adds and registers the component for you.

### 3. Pick a profile

Create one under **Add → Miscellaneous → Data Asset → TurretMind Profile**, or use one of the three
that ship in `Content/TurretMind/Profiles`:

| Profile | Range | Policy | Projectile | Character |
|---|---|---|---|---|
| `DA_TurretMind_Machinegun` | 2500 cm | Nearest | hitscan | Short reach, re-checks often. |
| `DA_TurretMind_Cannon` | 7000 cm | Furthest Along Path | 6000 cm/s | Long reach, leads hard, rarely changes its mind. |
| `DA_TurretMind_Missile` | 9000 cm | Highest Threat | 3500 cm/s | Long reach, goes for the dangerous one. |

The same three exist as transient built-ins created in code, so a turret with an empty `Profile` works
before a single asset has been authored.

### 4. Use the answer

In your turret's own tick:

```
Get TurretMind Turret Component  →  Has Target?
                                 →  Get Aim Rotation  →  Set World Rotation (barrel)
```

`Get Aim Point` gives you the world position; `Get Aim Rotation` gives you the rotation from the muzzle
to it, ready for a `Set World Rotation` on the barrel component.

Then fire with whatever you already have — a projectile, a hitscan trace, a beam. That half is yours.

### 5. See the numbers

Set your game mode's **HUD Class** to `TurretMind HUD` and type `TurretMind.Stats 1`. Already have a
HUD class? Add a **TurretMind HUD** component to it and call `Draw Stats (Canvas)` from your own
`Draw HUD` — both paths end in the same call.

---

## How it works

The naive turret asks `GetAllActorsOfClass`, sorts the result and pulls a trace — per turret, per
frame. Forty turrets and two hundred attackers is **eight thousand distance checks and forty traces
every frame**, before a single shot is fired.

TurretMind replaces that with four things.

**A spatial index.** Targets sit in a uniform grid over the XY plane (`Cell Size`, 1000 cm by default),
not in a list. A turret with 4000 cm of range opens about eighty-one cells, nearly all empty, and looks
at the dozen or so targets inside them. `Targets Considered` in the statistics box is that number.

**A staggered index refresh.** Targets move, so the grid has to be maintained — but only a slice of it
per frame (`Index Slice Fraction`, a quarter by default). Between refreshes a target's position is
extrapolated from its last sample and its measured velocity, so aiming stays smooth on the frames the
index skipped it.

**Round-robin re-evaluation under a hard budget.** A cursor walks the turret list. At most
`Max Acquisitions Per Frame` turrets get to think again; traces are rationed separately on top of that
with `Max Line Traces Per Frame`. A turret that does not get a turn **keeps the target it already
has**. Nothing stalls, nothing stands still — reaction time stretches, which is the trade a
hundred-turret level wants to make.

**Hysteresis.** A new target has to score `Target Stickiness` better than the held one before the turret
will switch. Without it, two attackers a hair apart in score make the barrel flick back and forth every
re-evaluation, which reads as broken however correct the ranking is.

### Aiming is not budgeted

That distinction is the whole trick. Every turret's aim point is recomputed **every frame**, for every
turret, whatever the acquisition budget is doing — it costs a subtraction and a normalise. *Choosing*
what to shoot at is the expensive part, and choosing is what gets rationed.

Drop the budget to four with `TurretMind.Budget 4` and the barrels keep tracking smoothly. They just
change their minds less often, and `Switches This Frame` falls.

---

## Policies

Set on the profile, so two turrets standing next to each other are allowed to think differently.

| Policy | Picks |
|---|---|
| `Nearest` | Closest to the muzzle. The default, and the one that needs no tuning. |
| `Lowest Health` | Finish the wounded one instead of spreading damage. Reads the target's `Health`. |
| `Highest Threat` | Reads `Threat`, normalised against the profile's `Threat Scale`. |
| `Furthest Along Path` | The tower defence answer: whoever is about to reach the base. Reads `Path Progress`. |
| `First Seen` | The target registered longest ago. Stable, boring, never flickers. |
| `Custom` | Your own function. |

Every policy produces a fitness in **0..1**, where 1 is the best possible target. That is what makes
`Target Stickiness` mean the same thing under every policy: "twenty per cent better" is a real
percentage, not a magic number that has to be retuned per policy.

### Custom scoring

```
Event BeginPlay
  → Create Event (MyScoreFunction)
  → TurretMind Turret Component → Set Custom Scorer
```

Your function receives a `TurretMind Turret Info` (who is asking: muzzle, forward, team, max range) and
a `TurretMind Target Info` (a flat snapshot: location, aim location, measured velocity, distance,
threat, health, path progress, radius, seconds registered, team). Return a fitness — higher is better,
and keep it inside 0..1 so stickiness keeps meaning what it says.

It is called once per candidate that survived range, team and cone filtering, so it runs a handful of
times per re-evaluation, not once per target in the level.

**Do not spawn or destroy actors from a custom scorer.** It runs inside the service's acquisition pass,
over an array the service is walking.

---

## Budgets and tuning

| Setting | Default | Notes |
|---|---|---|
| `Cell Size` | 1000 cm | A cell should hold a handful of targets; a turret's range should cover a handful of cells. |
| `Index Slice Fraction` | 0.25 | Fraction of the target index refreshed per frame. |
| `Velocity Smoothing` | 0.5 | How hard a new velocity sample pulls the stored one. |
| `Max Acquisitions Per Frame` | 16 | Turret re-evaluations per frame. |
| `Max Line Traces Per Frame` | 8 | Line-of-sight traces per frame. |
| `Los Cache Seconds` | 0.3 | How long a verdict stays good for one turret/target pair. |

**Project Settings → Plugins → TurretMind.** All of them are read once when a world's service comes up;
the budgets, overrides and debug switches can be moved at runtime afterwards without touching the
config.

Rule of thumb for the budget: `turrets ÷ reacquire interval ÷ frame rate` is the number of
re-evaluations a frame needs to give every turret its interval exactly. Forty turrets at a 0.25 s
interval and 60 fps is `40 / 0.25 / 60 ≈ 2.7` — so a budget of 16 has headroom to spare, and dropping
it to 4 still keeps everyone inside a fifth of a second.

---

## Console commands

| Command | Does |
|---|---|
| `TurretMind.Stats 0\|1` | The on-screen statistics box. |
| `TurretMind.Draw 0\|1` | World-space debug lines: muzzle-to-target, lead cross, range circles. |
| `TurretMind.DrawRanges 0\|1` | Include the range circles in the debug draw. |
| `TurretMind.Budget <n>` | Hard cap on re-evaluations per frame. No argument prints the current value. |
| `TurretMind.TraceBudget <n>` | Hard cap on line-of-sight traces per frame. |
| `TurretMind.Policy <name>` | Force one policy on every turret. `Clear` hands it back to the profiles. |
| `TurretMind.LineOfSight 0\|1\|Clear` | Force the line-of-sight requirement on or off for every turret. |

Everything here also exists as a Blueprint node under `TurretMind` and `TurretMind\|Debug`.

### Reading the statistics box

```
TurretMind
Turrets            40  (38 on target)
Targets            200
Acquisitions       16 / 16
Line traces        8 / 8
LoS cache hits     41
Cells visited      212
Targets considered 147  (naive 3200)
Switches           2
Index refreshed    50 / 200
Acquire            0.184 ms
Policy             per profile
Line of sight      per profile
```

`Targets considered` against `naive` is the spatial index doing its job — `naive` is what
`acquisitions × targets` would have cost. `Acquisitions` and `Line traces` never exceed their budgets;
if they did, the budget would not be a budget.

---

## Blueprint API

**On the turret component:** `Get Current Target`, `Get Current Target Component`, `Get Aim Point`,
`Get Aim Rotation`, `Has Target`, `Has Line Of Sight`, `Get Time On Target`, `Get Distance To Target`,
`Get Muzzle Location`, `Get Effective Profile`, `Set Profile`, `Set Enabled`, `Force Reacquire`,
`Clear Target`, `Set Custom Scorer`, `Clear Custom Scorer`.

**Events:** `On Target Acquired (Target)`, `On Target Lost (Previous Target)`,
`On Target Switched (Previous Target, New Target)`.

**On the target component:** `Set Team Id`, `Set Threat`, `Set Health`, `Set Path Progress`,
`Set Targetable`, `Get Measured Velocity`, `Get Aim Location`. Each setter pushes the value into the
index immediately instead of waiting for the target's turn in the staggered refresh — a unit that just
became untargetable has to stop being shot at now, not in three frames.

**Library (`TurretMind`):** `Get TurretMind Subsystem`, `Register Target`, `Unregister Target`,
`Get Target Component`, `Get Turret Component`, `Get TurretMind Stats`, `Set Acquisition Budget`,
`Set Trace Budget`, `Set Policy Override`, `Clear Policy Override`, `Set Line Of Sight Override`,
`Clear Line Of Sight Override`, `Set Show Stats`, `Set Draw Debug`, `Set Draw Ranges`,
`Find Best Target`, `Predict Aim Point`.

### Find Best Target

The escape hatch for things that are not turrets — a homing missile picking a new target mid-flight, a
UI showing what a turret *would* pick if you built one here. It runs the full scoring pass, traces
included, **right now**: unbudgeted and uncached. Fine for one caller; not fine in a loop over a hundred
actors, which is what the turret component is for.

---

## Lead prediction

`Predict Aim Point (Muzzle, Target Location, Target Velocity, Projectile Speed, Max Lead Seconds)`
solves the intercept point by iteration — three passes, not the closed-form quadratic.

The quadratic is exact for the assumption it makes, *constant velocity forever*, and that assumption is
wrong by the next frame because the target turns, is pushed, or stops. Three iterations land within a
few centimetres for any sane speed ratio, cost three square roots, and cannot produce the negative
discriminant the closed form has to handle. It is re-solved every frame anyway.

`Projectile Speed` at or below zero means **hitscan**: no travel time, so the aim point is the target's
own position and the lead is skipped. `Max Lead Seconds` (3 by default) guards the case where a target
runs away almost as fast as the projectile flies and the intercept point heads for the horizon.

---

## Limits

* **It does not shoot.** No projectiles, no damage, no hit detection, no turret-building, no waves.
* **No replication.** The service runs where the targets are known — the server. Each client shows what
  its own code tells it.
* **No Behavior Tree, no perception system.** Deliberately independent of `AIModule`, so it runs in a
  project that never touches either.
* **The grid is 2D (XY).** A turret's range query covers a square of cells in the XY plane; the Z
  distance is part of the range test but not of the cell lookup. In a level with heavily stacked floors
  — a space station, a high-rise — one cell holds every floor at once, and a turret opens targets it
  cannot reach. Raise `Cell Size` or shorten the ranges, and know that this is the honest shape of the
  trade.
* **No turret meshes or animations.** The demo uses primitive shapes.
