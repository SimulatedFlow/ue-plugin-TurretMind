// Copyright 2026 Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "TurretMindTypes.h"
#include "TurretMindProfile.generated.h"

/**
 * Everything that makes one kind of turret behave differently from another, in one data asset.
 *
 * Ranges, the cone it can traverse, how it ranks targets, how sticky it is, how fast its projectile
 * flies, whether it needs to see what it shoots at, and how often it is allowed to think again. Share
 * one asset across forty turrets and they all change together; give the missile battery its own and it
 * stops fighting the machineguns for the same target.
 *
 * The plugin ships with three built-in profiles — Cannon, Machinegun and Missile — created in code, so
 * a turret component with an empty Profile still behaves sensibly before a single asset has been
 * authored. Content/TurretMind/Profiles has the same three as editable assets.
 */
UCLASS(BlueprintType, meta = (DisplayName = "TurretMind Profile"))
class TURRETMIND_API UTurretMindProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UTurretMindProfile();

	// ----------------------------------------------------------------------------------------------
	// Reach
	// ----------------------------------------------------------------------------------------------

	/**
	 * Nothing further away than this is even looked at, in cm. It is also what sets how many grid cells
	 * a re-evaluation opens, so it is the single biggest lever on the cost of this plugin.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Range", meta = (ClampMin = "0.0", UIMax = "50000.0"))
	float MaxRange = 4000.0f;

	/** Dead zone around the turret, in cm. A mortar cannot depress far enough to hit its own feet. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Range", meta = (ClampMin = "0.0", UIMax = "10000.0"))
	float MinRange = 0.0f;

	/**
	 * Full cone the turret can cover, in degrees, measured against its actor forward vector.
	 * 360 means a full traverse and skips the check entirely — which is what a ring-mounted turret wants
	 * and what the default is. Set it to 90 for something bolted to a wall.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Range", meta = (ClampMin = "1.0", ClampMax = "360.0"))
	float FieldOfViewDegrees = 360.0f;

	// ----------------------------------------------------------------------------------------------
	// Choosing
	// ----------------------------------------------------------------------------------------------

	/** How this turret ranks the targets that survived range, team and cone filtering. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy")
	ETurretMindPolicy Policy = ETurretMindPolicy::Nearest;

	/**
	 * Hysteresis. A new target has to score this fraction better than the one currently held before the
	 * turret will switch: 0.2 means twenty per cent better.
	 *
	 * Without it, two targets a hair apart in score make the barrel flick back and forth every
	 * re-evaluation, which reads as broken no matter how correct the maths is. Zero disables it and is
	 * only sensible when the score can never be close, e.g. Furthest Along Path on a single-file queue.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy", meta = (ClampMin = "0.0", UIMax = "1.0"))
	float TargetStickiness = 0.2f;

	/**
	 * Divisor that turns raw Threat into a 0..1 fitness for the Highest Threat policy: a target at this
	 * threat scores 1. Set it to the largest threat value you actually hand out.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy", meta = (ClampMin = "0.001", UIMax = "1000.0"))
	float ThreatScale = 100.0f;

	/**
	 * Seconds between two re-evaluations of the same turret. This is a floor, not a promise: the
	 * service's per-frame budget can push it out further when a lot of turrets come due at once, and
	 * that is the intended behaviour, not a failure.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy", meta = (ClampMin = "0.0", UIMax = "5.0"))
	float ReacquireInterval = 0.25f;

	/**
	 * Safety valve: at most this many candidates are scored in one re-evaluation. Reached only when a
	 * very large range meets a very dense crowd; the ones dropped are simply the later entries in the
	 * cells, so it degrades into "a good target" rather than "the best target".
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Policy", meta = (ClampMin = "1", UIMax = "512"))
	int32 MaxTargetsConsidered = 64;

	// ----------------------------------------------------------------------------------------------
	// Teams
	// ----------------------------------------------------------------------------------------------

	/** Which teams this turret is allowed to pick from. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Teams")
	ETurretMindTeamFilter TeamFilter = ETurretMindTeamFilter::DifferentTeam;

	/** Team ids this turret shoots at when Team Filter is Listed Teams. Ignored otherwise. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Teams", meta = (EditCondition = "TeamFilter == ETurretMindTeamFilter::ListedTeams", EditConditionHides))
	TArray<int32> TargetTeamIds;

	// ----------------------------------------------------------------------------------------------
	// Line of sight
	// ----------------------------------------------------------------------------------------------

	/**
	 * Require a clear line from the muzzle to the target before it can be picked.
	 *
	 * Traces are the expensive part, so they are rationed: each answer is cached for Los Cache Seconds
	 * and the service fires at most Max Line Traces Per Frame of them. Off means a turret will happily
	 * lock onto something behind a wall — which is the right answer for an artillery piece.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Line Of Sight")
	bool bRequireLineOfSight = true;

	/** Collision channel the visibility trace runs on. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Line Of Sight", meta = (EditCondition = "bRequireLineOfSight"))
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	// ----------------------------------------------------------------------------------------------
	// Leading
	// ----------------------------------------------------------------------------------------------

	/** Aim where the target will be rather than where it is. Off makes the aim point the target itself. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leading")
	bool bPredictLead = true;

	/**
	 * Speed of whatever you fire, in cm/s. Zero or less means hitscan — no travel time, so the aim point
	 * is the target's position and lead prediction is skipped.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leading", meta = (ClampMin = "0.0", UIMax = "100000.0", EditCondition = "bPredictLead"))
	float ProjectileSpeed = 0.0f;

	/**
	 * Never lead further ahead than this many seconds. Guards the case where a target sprints away
	 * almost as fast as the projectile flies and the intercept point runs off to the horizon.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leading", meta = (ClampMin = "0.0", UIMax = "20.0", EditCondition = "bPredictLead"))
	float MaxLeadSeconds = 3.0f;

	/** Clamped, sanity-checked copy of MaxRange squared. Cheap enough to just recompute on demand. */
	float GetMaxRangeSquared() const { return MaxRange * MaxRange; }

	/** Cosine of the half angle of the traverse cone, or -1 when the cone is a full 360. */
	float GetConeCosine() const;

	/** True when the traverse cone is wide enough that testing it would be a waste of instructions. */
	bool HasFullTraverse() const { return FieldOfViewDegrees >= 359.9f; }

	/** Does a target on this team pass the profile's team filter, given the turret's own team? */
	bool PassesTeamFilter(int32 TurretTeamId, int32 TargetTeamId) const;
};
