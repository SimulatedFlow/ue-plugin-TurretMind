# TurretMind — Target Acquisition on a Budget

**User documentation · Plugin version 1.0.0 · Unreal Engine 5.8**

One targeting service for hundreds of turrets. Targets live in a spatial grid instead of a list every
turret walks, re-evaluation runs round-robin under a hard per-frame budget, line-of-sight traces are
spread over frames and cached, and lead prediction hands you the point to aim at.

**TurretMind does not shoot.** No projectiles, no damage, no hit detection. It answers two questions —
*what should this turret aim at* and *where should it lead* — and stops there. See
[Limits](#limits) before you buy or before you build on it.

---

## Contents

1. [Supported engine and platforms](#supported-engine-and-platforms)
2. [Installation](#installation)
3. [Quick start — five minutes](#quick-start--five-minutes)
4. [How it works](#how-it-works)
5. [Class and API overview](#class-and-api-overview)
6. [Code examples](#code-examples)
7. [Policies](#policies)
8. [Budgets and tuning](#budgets-and-tuning)
9. [Console commands](#console-commands)
10. [Lead prediction](#lead-prediction)
11. [Troubleshooting](#troubleshooting)
12. [Limits](#limits)
13. [Support](#support)

---

## Supported engine and platforms

| | |
|---|---|
| **Engine** | Unreal Engine **5.8** (`"EngineVersion": "5.8.0"`). Not tested on 5.7 or earlier. |
| **Platforms** | **Win64** — the module's `PlatformAllowList`. |
| **Module** | One module: `TurretMind`, `Type: Runtime`, `LoadingPhase: PreDefault`. No editor module. |
| **Dependencies** | `Core`, `CoreUObject`, `Engine`, `DeveloperSettings` (public), `RenderCore` (private). |
| **Not required** | `AIModule`, `UMG`, `UnrealEd`, Behavior Trees, the perception system, Navigation. |
| **Third-party code** | None. No external libraries, no binaries beyond the engine's own build output. |
| **Project type** | Works in a **Blueprint-only project** (the plugin ships its own source and builds itself) and in a C++ project. |
| **Build configs** | Verified building in Development Editor, Development and Shipping. |
| **Content** | `CanContainContent: true`. Everything the plugin ships lives under `Content/TurretMind/`. |

The plugin is **runtime code**: the statistics box is drawn on `UCanvas` from `AHUD`, not with UMG, so
it survives a cooked Shipping build. World-space debug drawing is compiled behind `ENABLE_DRAW_DEBUG`
and the editor-viewport overlay behind `WITH_EDITOR`, so neither costs anything in Shipping.

### Mac, Linux and consoles

Not listed. Nothing in the plugin is platform-specific — it is plain gameplay C++ with no third-party
code — but the `PlatformAllowList` names only the platform that was actually built and tested, and
that is Win64. Add a platform to the list in `TurretMind.uplugin` if you build for it yourself; the
source ships with the plugin, so nothing stops you.

---

## Installation

### From Fab (recommended)

1. In the **Epic Games Launcher → Library → Fab Library**, find *TurretMind* and press
   **Install to Engine**, choosing your 5.8 installation.
2. Open your project, then **Edit → Plugins → AI → TurretMind**, tick **Enabled** and restart the
   editor when asked.

### Into a single project

1. Copy the `TurretMind` folder into `<YourProject>/Plugins/` so you end up with
   `<YourProject>/Plugins/TurretMind/TurretMind.uplugin`.
2. **Blueprint-only project:** right-click your `.uproject` → **Generate Visual Studio project files**,
   then open the `.uproject`. The editor offers to rebuild the missing module — accept. (On Mac/Linux
   use the equivalent `GenerateProjectFiles` script.)
3. **C++ project:** regenerate project files and build your editor target as usual.
4. **Edit → Plugins → AI → TurretMind → Enabled**, restart.

### Verifying the install

Open the console (`~`) in Play-In-Editor and type:

```
TurretMind.Stats 1
```

If the statistics box appears, the runtime module loaded and this world's service is alive. If nothing
happens, your game mode's HUD class is not drawing it — see [step 5](#5-see-the-numbers) below.

### Packaging

Nothing to do. The module is `Runtime`, so it is cooked into your game automatically. If you do not want
the demo content in your shipped build, exclude `Content/TurretMind/` from the packaged directories — the
three built-in profiles are created in code and do not depend on any asset.

---

## The demo map

Everything below lives under `Content/TurretMind/` (mount path `/TurretMind/TurretMind/`) and is there to
be read, not just watched. It uses nothing but engine primitives — no meshes, no animations, no
third-party content.

| Asset | What it is |
|---|---|
| `Maps/L_TurretMindDemo` | A tower-defence arena: a serpentine path, 40 turrets in four rows either side of it, six walls as sight blockers, top-down camera. |
| `Profiles/DA_TurretMind_Cannon` | Long reach (5000 cm), slow re-evaluation, leads a 4000 cm/s shell, **Furthest Along Path**. |
| `Profiles/DA_TurretMind_Machinegun` | Short reach (2500 cm), fast re-evaluation, hitscan, **Nearest**. |
| `Profiles/DA_TurretMind_Missile` | Longest reach (6500 cm) with a 800 cm dead zone, leads a 2200 cm/s missile, **Highest Threat**, line of sight off — it arcs over cover. |
| `Blueprints/BP_TurretMindTurret` | Base + a **rotating barrel on its own component**. Its whole tick is `Get Aim Rotation` → `RInterp To` → `Set World Rotation`. That is the entire client-side API in three nodes. |
| `Blueprints/BP_TurretMindWalker` | An attacker. Walks a fixed path and pushes `Set Path Progress` into its target component; nothing else. |
| `Blueprints/BP_TurretMindDemoDirector` | Holds the wave at a target size. It spawns and retires attackers — TurretMind itself never creates or destroys anything. |
| `Blueprints/BP_TurretMindDemoGameMode` | Sets `ATurretMindHUD` as the HUD class, which is what draws the statistics box. |
| `Blueprints/BP_TurretMindDemoController` | Shows the cursor and puts the overlay on screen. |
| `UI/WBP_TurretMindDemoHUD` | The button panel. Every button is a single call into `UTurretMindStatics` — wave size, policy override, acquisition budget, line-of-sight override, debug draw, statistics box. |

Open the map, press Play, and watch the `Targets considered` line against `Targets`: that gap is the
spatial index. Press **Budget 4** and watch `Acquisitions` cap while the barrels keep tracking.

---

## Quick start — five minutes

### 1. Put a component on the turret

Add a **TurretMind Turret** component (`UTurretMindComponent`) to your turret actor, in Blueprint or
C++.

| Property | What it does |
|---|---|
| `Profile` | Ranges, policy, stickiness, projectile speed. Leave it empty to use the built-in fallback. |
| `Team Id` | Whose side this turret is on. Default `0`. |
| `Muzzle Offset` | Where the shot leaves the turret, in the actor's own space. Range, cone and traces start here. |
| `b Enabled` | Off takes the turret out of the round-robin without unregistering it. |

The component has **no tick**. That is deliberate: a turret that ticks is a turret that runs its own
query, and forty of those is the problem this plugin removes.

### 2. Put a component on everything shootable

Add a **TurretMind Target** component (`UTurretMindTargetComponent`) to each attacker.

| Property | What it does |
|---|---|
| `Team Id` | Whose side the target is on. Default `1`. |
| `Threat` | Only read by the `Highest Threat` policy. The unit is yours. |
| `Health` | 0..1, only read by `Lowest Health`. Push your own number in with `Set Health`. |
| `Path Progress` | 0..1, only read by `Furthest Along Path`. |
| `Radius` | Rough size in cm; keeps the line-of-sight trace out of the target's feet. |
| `Aim Offset` | Where on the actor a turret aims, in the actor's own space. Default 60 cm up. |
| `b Targetable` | Off makes the actor invisible to turrets without unregistering it. |
| `b Use Owner Velocity` | Read the owner's `GetVelocity()` instead of measuring. Off by default. |

Targets do not tick either. Velocity is **measured** between index samples by default, so an actor that
is moved by setting its transform still produces a usable lead — asking a movement component would hand
back zero.

Have an actor whose class you do not own? `TurretMind → Register Target (Actor, Team Id, Threat,
Radius)` adds and registers the component for you, and is safe to call twice.

### 3. Pick a profile

Three profiles are **created in code** and always available, so a turret with an empty `Profile` works
before a single asset has been authored:

| Built-in | Max range | Policy | Projectile | Character |
|---|---|---|---|---|
| `Machinegun` *(default)* | 2500 cm | Nearest | hitscan | Short reach, re-checks often, low stickiness. |
| `Cannon` | 7000 cm | Furthest Along Path | 6000 cm/s | Long reach, leads hard, rarely changes its mind. |
| `Missile` | 9000 cm | Highest Threat | 3500 cm/s | Long reach, goes for the dangerous one. |

Which one an empty `Profile` falls back to is **Project Settings → Plugins → TurretMind → Default
Builtin Profile**.

For your own: **Content Browser → Add → Miscellaneous → Data Asset → TurretMind Profile**. Every field
is documented in the Details panel tooltip.

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

Set your game mode's **HUD Class** to `TurretMind HUD` (`ATurretMindHUD`) and type `TurretMind.Stats 1`.

Already have a HUD class? Add a **TurretMind HUD** component (`UTurretMindHUDComponent`) to it and call
`Draw Stats (Canvas)` from your own `Draw HUD` — both paths end in the same call.

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

### Lifetime

The service is a `UTickableWorldSubsystem`: one per world, created with the world, gone with it. There
is nothing to spawn, nothing to place in the level and nothing to initialise. Settings are read once
when a world's service comes up; budgets, overrides and debug switches can be moved at runtime
afterwards without touching the config.

---

## Class and API overview

| Class | Base | Role |
|---|---|---|
| `UTurretMindSubsystem` | `UTickableWorldSubsystem` | The service. Spatial index, budgeted acquisition round, line-of-sight cache, aim pass, statistics, debug draw. One per world, automatic. |
| `UTurretMindComponent` | `UActorComponent` | Goes on a turret. No tick. The entire client-side API for reading what to aim at. |
| `UTurretMindTargetComponent` | `UActorComponent` | Goes on anything shootable. No tick. Feeds position, measured velocity, team, threat, health and path progress into the index. |
| `UTurretMindProfile` | `UPrimaryDataAsset` | One turret *kind*: ranges, cone, policy, stickiness, team filter, line-of-sight, projectile speed. |
| `UTurretMindSettings` | `UDeveloperSettings` | Project Settings → Plugins → TurretMind. Cell size, budgets, cache times, debug defaults. |
| `UTurretMindStatics` | `UBlueprintFunctionLibrary` | Static entry points for Blueprints that hold no reference: register targets, read stats, move budgets, one-off queries. |
| `ATurretMindHUD` | `AHUD` | Drop-in HUD class that draws the statistics box. |
| `UTurretMindHUDComponent` | `UActorComponent` | The same box for a HUD class you already own. |

### Types

| Type | Contents |
|---|---|
| `ETurretMindPolicy` | `Nearest`, `LowestHealth`, `HighestThreat`, `FurthestAlongPath`, `FirstSeen`, `Custom` |
| `ETurretMindTeamFilter` | `DifferentTeam`, `AnyTeam`, `SameTeam`, `ListedTeams` |
| `FTurretMindTargetInfo` | Flat snapshot of a candidate: `Actor`, `Location`, `AimLocation`, `Velocity`, `Distance`, `Threat`, `Health`, `PathProgress`, `Radius`, `TimeRegistered`, `TeamId` |
| `FTurretMindTurretInfo` | Who is asking: `Actor`, `MuzzleLocation`, `Forward`, `MaxRange`, `TeamId` |
| `FTurretMindStats` | `Turrets`, `Targets`, `AcquisitionsThisFrame`, `TracesThisFrame`, `CellsVisited`, `TargetsConsidered`, `AcquireMs`, `SwitchesThisFrame`, `TurretsOnTarget`, `IndexRefreshedThisFrame`, `LosCacheHitsThisFrame` |
| `FTurretMindScoreDelegate` | `float (const FTurretMindTurretInfo&, const FTurretMindTargetInfo&)` — your `Custom` ranking function |

### `UTurretMindComponent` — the turret side

**Read (BlueprintPure):** `GetCurrentTarget`, `GetCurrentTargetComponent`, `GetAimPoint`,
`GetAimRotation`, `HasTarget`, `HasLineOfSight`, `GetTimeOnTarget`, `GetDistanceToTarget`,
`GetMuzzleLocation`, `GetEffectiveProfile`, `IsRegisteredWithService`.

**Control (BlueprintCallable):** `SetProfile`, `SetEnabled`, `ForceReacquire`, `ClearTarget`,
`SetCustomScorer`, `ClearCustomScorer`, `RegisterWithService`, `UnregisterFromService`.

**Events (BlueprintAssignable):** `OnTargetAcquired(AActor* Target)`,
`OnTargetLost(AActor* PreviousTarget)`, `OnTargetSwitched(AActor* Previous, AActor* New)`.

> `IsRegisteredWithService()` is **not** `UActorComponent::IsRegistered()`. It asks whether the
> component is in the service's turret list.

### `UTurretMindTargetComponent` — the target side

**Setters:** `SetTeamId`, `SetThreat`, `SetHealth`, `SetPathProgress`, `SetTargetable`.
Each pushes the value straight into the index instead of waiting for the target's turn in the staggered
refresh — a unit that just became untargetable has to stop being shot at *now*, not in three frames.

**Read:** `GetMeasuredVelocity`, `GetAimLocation`, `IsRegisteredWithService`.

**Control:** `RegisterWithService`, `UnregisterFromService`.

### `UTurretMindStatics` — the library (Blueprint category `TurretMind`)

| Function | Notes |
|---|---|
| `GetTurretMindSubsystem(WorldContext)` | The world's service, or null outside a game world. |
| `RegisterTarget(Actor, TeamId, Threat, Radius)` | Adds + registers a target component. Idempotent. |
| `UnregisterTarget(Actor)` | Removes the component this library added. |
| `GetTargetComponent(Actor)` / `GetTurretComponent(Actor)` | Lookups. |
| `GetTurretMindStats(WorldContext)` | The same numbers the box draws. |
| `SetAcquisitionBudget(WorldContext, N)` / `SetTraceBudget(WorldContext, N)` | Runtime budget changes. |
| `SetPolicyOverride` / `ClearPolicyOverride` | Force one policy on every turret in the world. |
| `SetLineOfSightOverride` / `ClearLineOfSightOverride` | Force the LoS requirement on or off, world-wide. |
| `SetShowStats` / `SetDrawDebug` / `SetDrawRanges` | Category `TurretMind\|Debug`. |
| `FindBestTarget(...)` | One-off, unbudgeted, uncached query — see below. |
| `PredictAimPoint(...)` | Pure intercept solve, no registration needed. |

#### `FindBestTarget`

The escape hatch for things that are not turrets — a homing missile picking a new target mid-flight, a
UI showing what a turret *would* pick if you built one here. It runs the full scoring pass, traces
included, **right now**: unbudgeted and uncached. Fine for one caller; not fine in a loop over a hundred
actors, which is what the turret component is for.

---

## Code examples

### C++ — a turret actor

```cpp
// MyTurret.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MyTurret.generated.h"

class UTurretMindComponent;

UCLASS()
class MYGAME_API AMyTurret : public AActor
{
    GENERATED_BODY()

public:
    AMyTurret();

    virtual void Tick(float DeltaSeconds) override;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, Category = "Turret")
    TObjectPtr<UStaticMeshComponent> Barrel;

    /** The only targeting code in this actor. */
    UPROPERTY(VisibleAnywhere, Category = "Turret")
    TObjectPtr<UTurretMindComponent> Targeting;

    UFUNCTION()
    void HandleTargetAcquired(AActor* Target);

    UFUNCTION()
    void HandleTargetLost(AActor* PreviousTarget);
};
```

```cpp
// MyTurret.cpp
#include "MyTurret.h"

#include "Components/StaticMeshComponent.h"
#include "TurretMindComponent.h"

AMyTurret::AMyTurret()
{
    // Your actor still ticks - to swing the barrel. TurretMind's component does not.
    PrimaryActorTick.bCanEverTick = true;

    Barrel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Barrel"));
    SetRootComponent(Barrel);

    Targeting = CreateDefaultSubobject<UTurretMindComponent>(TEXT("Targeting"));
    Targeting->TeamId = 0;
    Targeting->MuzzleOffset = FVector(0.0f, 0.0f, 150.0f);
}

void AMyTurret::BeginPlay()
{
    Super::BeginPlay();

    // The component registered itself in its own BeginPlay; just listen.
    Targeting->OnTargetAcquired.AddDynamic(this, &AMyTurret::HandleTargetAcquired);
    Targeting->OnTargetLost.AddDynamic(this, &AMyTurret::HandleTargetLost);
}

void AMyTurret::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!Targeting->HasTarget())
    {
        return;
    }

    // GetAimRotation() is already the muzzle-to-intercept-point rotation, refreshed this frame
    // regardless of what the acquisition budget did.
    const FRotator Desired = Targeting->GetAimRotation();
    const FRotator Smoothed = FMath::RInterpTo(Barrel->GetComponentRotation(), Desired, DeltaSeconds, 6.0f);
    Barrel->SetWorldRotation(Smoothed);

    // Fire only when the barrel has actually caught up and the line is clear.
    if (Targeting->HasLineOfSight() && Desired.Equals(Smoothed, 2.0f))
    {
        // Your weapon code. TurretMind never pulls a trigger.
        // FireProjectileTowards(Targeting->GetAimPoint());
    }
}

void AMyTurret::HandleTargetAcquired(AActor* Target)
{
    UE_LOG(LogTemp, Verbose, TEXT("%s locked on to %s"), *GetName(), *GetNameSafe(Target));
}

void AMyTurret::HandleTargetLost(AActor* PreviousTarget)
{
    // Park the barrel, play a "searching" animation, whatever suits.
}
```

### C++ — making an actor shootable

Either add the component in the constructor:

```cpp
#include "TurretMindTargetComponent.h"

AMyEnemy::AMyEnemy()
{
    Targetable = CreateDefaultSubobject<UTurretMindTargetComponent>(TEXT("Targetable"));
    Targetable->TeamId = 1;
    Targetable->Threat = 25.0f;
    Targetable->Radius = 45.0f;
}
```

…or attach one to an actor whose class you do not own:

```cpp
#include "TurretMindStatics.h"
#include "TurretMindTargetComponent.h"

// Team 1, threat 25, radius 45 cm. Calling this twice returns the same component.
UTurretMindTargetComponent* Comp = UTurretMindStatics::RegisterTarget(SpawnedActor, 1, 25.0f, 45.0f);
```

Then keep the fields your policies read up to date. These setters push into the index immediately:

```cpp
void AMyEnemy::ApplyDamage(float Amount)
{
    CurrentHealth = FMath::Max(0.0f, CurrentHealth - Amount);

    // Health is normalised 0..1 - only the Lowest Health policy reads it.
    Targetable->SetHealth(CurrentHealth / MaxHealth);

    if (CurrentHealth <= 0.0f)
    {
        // Stop being shot at this frame, not in three.
        Targetable->SetTargetable(false);
    }
}

void AMyEnemy::UpdatePathProgress(float AlongSpline01)
{
    // The tower-defence input for the Furthest Along Path policy.
    Targetable->SetPathProgress(AlongSpline01);
}
```

### C++ — a custom ranking function

```cpp
#include "TurretMindComponent.h"
#include "TurretMindTypes.h"

void AMyTurret::InstallScorer()
{
    FTurretMindScoreDelegate Scorer;
    Scorer.BindDynamic(this, &AMyTurret::ScoreTarget);
    Targeting->SetCustomScorer(Scorer);
    // The profile's Policy must be Custom for this to be used.
}

// UFUNCTION() is required - it is a dynamic delegate.
UFUNCTION()
float AMyTurret::ScoreTarget(const FTurretMindTurretInfo& Turret, const FTurretMindTargetInfo& Target)
{
    // Keep the result inside 0..1 so the profile's Target Stickiness keeps meaning
    // "twenty per cent better".
    const float Closeness = 1.0f - FMath::Clamp(Target.Distance / FMath::Max(Turret.MaxRange, 1.0f), 0.0f, 1.0f);
    const float Wounded   = 1.0f - FMath::Clamp(Target.Health, 0.0f, 1.0f);
    const float Advanced  = FMath::Clamp(Target.PathProgress, 0.0f, 1.0f);

    return 0.20f * Closeness + 0.30f * Wounded + 0.50f * Advanced;
}
```

It is called once per candidate that survived range, team and cone filtering, so it runs a handful of
times per re-evaluation, not once per target in the level.

> **Do not spawn or destroy actors from a custom scorer.** It runs inside the service's acquisition
> pass, over an array the service is walking.

### C++ — one-off query and lead prediction

```cpp
#include "TurretMindStatics.h"

// What would a Missile-profile turret standing here pick right now?
AActor*  Best = nullptr;
FVector  AimPoint = FVector::ZeroVector;

const bool bFound = UTurretMindStatics::FindBestTarget(
    this,                       // world context
    GetActorLocation(),         // origin
    GetActorForwardVector(),    // forward, for the profile's cone
    MissileProfile,             // UTurretMindProfile*
    /*TeamId=*/0,
    Best,
    AimPoint,
    /*IgnoreActor=*/this);

// Pure maths, no registration required - useful for a homing missile in flight.
const FVector Lead = UTurretMindStatics::PredictAimPoint(
    GetActorLocation(),
    Target->GetActorLocation(),
    Target->GetVelocity(),
    /*ProjectileSpeed=*/3500.0f,
    /*MaxLeadSeconds=*/3.0f);
```

### C++ — moving the budgets at runtime

```cpp
#include "TurretMindSubsystem.h"

if (UTurretMindSubsystem* Service = GetWorld()->GetSubsystem<UTurretMindSubsystem>())
{
    // Big wave incoming: think harder.
    Service->SetAcquisitionBudget(48);
    Service->SetTraceBudget(24);

    const FTurretMindStats& Stats = Service->GetStats();
    UE_LOG(LogTemp, Display, TEXT("%d turrets, %d targets, %d considered, %.3f ms"),
        Stats.Turrets, Stats.Targets, Stats.TargetsConsidered, Stats.AcquireMs);
}
```

### Blueprint — the same four things

**Turret tick:**

```
Event Tick
  → Targeting (TurretMind Turret) → Has Target
  → Branch (True)
      → Targeting → Get Aim Rotation
      → RInterp To (Current = Barrel → Get World Rotation, Interp Speed = 6)
      → Barrel → Set World Rotation
```

**Custom scorer:**

```
Event BeginPlay
  → Create Event (MyScoreFunction)      // signature: TurretMindTurretInfo, TurretMindTargetInfo → float
  → TurretMind Turret Component → Set Custom Scorer
```

**Stats on a HUD you already own:**

```
Event Receive Draw HUD
  → TurretMind HUD (component) → Draw Stats (Canvas)
```

**Budget button in a debug menu:**

```
On Clicked → TurretMind → Set Acquisition Budget (Max Acquisitions Per Frame = 4)
```

---

## Policies

Set on the profile, so two turrets standing next to each other are allowed to think differently.

| Policy | Picks | Reads |
|---|---|---|
| `Nearest` | Closest to the muzzle. The default, and the one that needs no tuning. | distance |
| `Lowest Health` | Finish the wounded one instead of spreading damage. | `Health` |
| `Highest Threat` | The dangerous one, normalised against the profile's `Threat Scale`. | `Threat` |
| `Furthest Along Path` | The tower defence answer: whoever is about to reach the base. | `Path Progress` |
| `First Seen` | The target registered longest ago. Stable, boring, never flickers. | `Time Registered` |
| `Custom` | Your own function. Falls back to `Nearest` when nothing is bound. | whatever you read |

Every policy produces a fitness in **0..1**, where 1 is the best possible target. That is what makes
`Target Stickiness` mean the same thing under every policy: "twenty per cent better" is a real
percentage, not a magic number that has to be retuned per policy.

### Team filtering

| Filter | Effect |
|---|---|
| `Different Team` | Anything whose `Team Id` differs from the turret's. The usual case, and the default. |
| `Any Team` | No filtering at all — friendly fire included. Useful for a test map. |
| `Same Team` | Only the turret's own team, e.g. a repair or buff emitter. |
| `Listed Teams` | Only the ids in `Target Team Ids` on the profile. |

---

## Budgets and tuning

**Project Settings → Plugins → TurretMind.**

| Setting | Default | Notes |
|---|---|---|
| `Cell Size` | 1000 cm | A cell should hold a handful of targets; a turret's range should cover a handful of cells. |
| `Index Slice Fraction` | 0.25 | Fraction of the target index refreshed per frame. |
| `Velocity Smoothing` | 0.5 | How hard a new velocity sample pulls the stored one. |
| `Max Acquisitions Per Frame` | 16 | Turret re-evaluations per frame. |
| `Max Line Traces Per Frame` | 8 | Line-of-sight traces per frame. |
| `Los Cache Seconds` | 0.3 | How long a verdict stays good for one turret/target pair. |
| `Default Profile` / `Default Builtin Profile` | — / `Machinegun` | What an empty `Profile` falls back to. |
| `Max Debug Draw Turrets` | 64 | Cap on how many turrets the world-space debug draw covers. |
| `b Show Stats / Draw Debug / Draw Ranges By Default` | off / off / on | Starting state of the debug switches. |
| `b Enable Editor Viewport Stats` | on | The `WITH_EDITOR` viewport overlay. |

Per-profile, on the data asset: `Max Range`, `Min Range`, `Field Of View Degrees`, `Policy`,
`Target Stickiness`, `Threat Scale`, `Reacquire Interval`, `Max Targets Considered`, `Team Filter`,
`Target Team Ids`, `b Require Line Of Sight`, `Trace Channel`, `b Predict Lead`, `Projectile Speed`,
`Max Lead Seconds`.

**Rule of thumb for the budget:** `turrets ÷ reacquire interval ÷ frame rate` is the number of
re-evaluations a frame needs to give every turret its interval exactly. Forty turrets at a 0.25 s
interval and 60 fps is `40 / 0.25 / 60 ≈ 2.7` — so a budget of 16 has headroom to spare, and dropping
it to 4 still keeps everyone inside a fifth of a second.

`Reacquire Interval` is a **floor, not a promise**: the per-frame budget can push it out further when a
lot of turrets come due at once. That is the intended behaviour, not a failure.

---

## Console commands

| Command | Does |
|---|---|
| `TurretMind.Stats 0\|1` | The on-screen statistics box. |
| `TurretMind.Draw 0\|1` | World-space debug lines: muzzle-to-target, lead cross, range circles. |
| `TurretMind.DrawRanges 0\|1` | Include the range circles in the debug draw. |
| `TurretMind.Budget <n>` | Hard cap on re-evaluations per frame. No argument prints the current value. |
| `TurretMind.TraceBudget <n>` | Hard cap on line-of-sight traces per frame. No argument prints it. |
| `TurretMind.Policy <name>` | Force one policy on every turret. `Clear` hands it back to the profiles. |
| `TurretMind.LineOfSight 0\|1\|Clear` | Force the line-of-sight requirement on or off for every turret. |

`TurretMind.Policy` accepts `Nearest`, `LowestHealth`, `HighestThreat`, `FurthestAlongPath`,
`FirstSeen`, `Custom` and `Clear`.

Everything here also exists as a Blueprint node under `TurretMind` and `TurretMind|Debug`.

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

Gravity is **not** modelled. If you fire ballistic arcs, take `Get Aim Point` as the impact point and do
your own launch-angle solve from there — that is a projectile question, and TurretMind is not a
projectile plugin.

---

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| Turret never acquires anything | Targets have no **TurretMind Target** component, or both sides share a `Team Id` under the `Different Team` filter. |
| Turret sees nothing behind a wall | Working as intended. Turn off `b Require Line Of Sight` on the profile, or `TurretMind.LineOfSight 0`. |
| Turrets react sluggishly | `Max Acquisitions Per Frame` too low for the turret count, or `Reacquire Interval` too long. Check `Acquisitions` against its budget in the box. |
| Barrels jitter between two targets | `Target Stickiness` too low. Raise it towards 0.3–0.4. Watch `Switches`. |
| `Targets considered` is nearly `Targets` | `Cell Size` too large for the ranges in play, or one profile's `Max Range` covers the whole level. |
| Lead point sits on the target | `b Predict Lead` off, `Projectile Speed` at 0 (hitscan), or the target is not actually moving. |
| Aim point lags a moving target | `Index Slice Fraction` very low. Raise it, or set `b Use Owner Velocity` on targets that have a movement component. |
| Statistics box does not appear | HUD class is not `TurretMind HUD` and no `TurretMind HUD` component calls `Draw Stats`. |
| Nothing at all in a dedicated-server build | Expected — there is no `AHUD` and no debug draw. The service itself runs fine. |

---

## Limits

Read this section before you build on the plugin. None of it is a bug.

* **It does not shoot.** No projectiles, no damage, no hit detection, no turret-building, no waves, no
  tower defence template. TurretMind decides *what* is aimed at and *where to lead*; everything after
  the trigger is yours.
* **No network replication.** The service runs where the targets are known — the server. Each client
  shows what its own code tells it. Replicating "which actor is this turret on" is a one-`UPROPERTY`
  job in your own turret class, and deliberately not done for you.
* **No Behavior Tree nodes, no perception system.** Deliberately independent of `AIModule`, so it runs
  in a project that never touches either.
* **The grid is 2D (XY).** A turret's range query covers a square of cells in the XY plane; the Z
  distance is part of the range test but not of the cell lookup. In a level with heavily stacked floors
  — a space station, a high-rise — one cell holds every floor at once, and a turret opens targets it
  cannot reach. Raise `Cell Size` or shorten the ranges, and know that this is the honest shape of the
  trade.
* **Line of sight is one trace to one point.** Muzzle to aim location, offset by the target's `Radius`.
  There is no per-bone visibility and no partial-cover model.
* **No gravity or drag in the lead solve.** Constant-velocity intercept only.
* **No turret meshes, no animations, no sounds.** The demo uses primitive shapes so you can see the
  targeting instead of the art.
* **No editor utility widgets.** Everything is runtime.

---

## Support

* **Documentation / issues:** <https://github.com/SimulatedFlow/ue-plugin-TurretMind>
* **E-mail:** teufelsilvan@gmail.com

When reporting a problem, a screenshot of the statistics box (`TurretMind.Stats 1`) plus your profile's
`Max Range`, `Policy` and `Reacquire Interval` answers most questions on the spot.
