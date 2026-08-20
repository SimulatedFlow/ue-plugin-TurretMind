// Copyright 2026 Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TurretMindTypes.generated.h"

class AActor;

/**
 * How a turret ranks the targets it can see.
 *
 * The policy lives on the profile, which lives on the turret — not on the service. Two turrets standing
 * next to each other in the same level are allowed to think differently: the machinegun shoots whatever
 * is closest, the missile battery goes for the biggest threat, and the mortar at the back picks whoever
 * has come furthest along the path.
 *
 * Every policy is turned into a fitness in the range 0..1, where 1 is the best possible target. That is
 * what makes stickiness a plain, comparable number: "twenty per cent better" means the same thing under
 * every policy.
 */
UENUM(BlueprintType)
enum class ETurretMindPolicy : uint8
{
	/** Closest to the muzzle wins. The default, and the one that needs no tuning. */
	Nearest UMETA(DisplayName = "Nearest"),

	/** Lowest Health on the target component wins — finish the wounded one instead of spreading damage. */
	LowestHealth UMETA(DisplayName = "Lowest Health"),

	/** Highest Threat wins, normalised against the profile's Threat Scale. */
	HighestThreat UMETA(DisplayName = "Highest Threat"),

	/** Highest Path Progress wins. The tower defence answer: whoever is about to reach the end. */
	FurthestAlongPath UMETA(DisplayName = "Furthest Along Path"),

	/** The target that has been registered the longest wins. Stable, boring, and never flickers. */
	FirstSeen UMETA(DisplayName = "First Seen"),

	/** Your own scoring function, bound per turret with Set Custom Scorer. */
	Custom UMETA(DisplayName = "Custom (delegate)")
};

/** Which teams a turret is allowed to shoot at. */
UENUM(BlueprintType)
enum class ETurretMindTeamFilter : uint8
{
	/** Anything whose Team Id differs from the turret's. The usual case. */
	DifferentTeam UMETA(DisplayName = "Different Team"),

	/** No filtering at all — friendly fire included. Useful for a repair drone or a test map. */
	AnyTeam UMETA(DisplayName = "Any Team"),

	/** Only the turret's own team, e.g. a healing or buff emitter that has to pick a friend. */
	SameTeam UMETA(DisplayName = "Same Team"),

	/** Only the team ids listed in Target Team Ids on the profile. */
	ListedTeams UMETA(DisplayName = "Listed Teams")
};

/**
 * One target as the scoring pass sees it: a flat snapshot out of the spatial index, never a live actor
 * lookup. This is what a Custom scorer receives, so a Blueprint scoring function costs one delegate call
 * and no component reads at all.
 */
USTRUCT(BlueprintType)
struct TURRETMIND_API FTurretMindTargetInfo
{
	GENERATED_BODY()

	/** The actor the target component sits on. Can be null if it died between the sample and the score. */
	UPROPERTY(BlueprintReadOnly, Category = "TurretMind")
	TObjectPtr<AActor> Actor = nullptr;

	/** Last sampled world location of the target actor. */
	UPROPERTY(BlueprintReadOnly, Category = "TurretMind")
	FVector Location = FVector::ZeroVector;

	/** Where the turret would actually aim: Location plus the target's Aim Offset. */
	UPROPERTY(BlueprintReadOnly, Category = "TurretMind")
	FVector AimLocation = FVector::ZeroVector;

	/** Measured velocity, in cm/s. Measured between index samples, not read off a movement component. */
	UPROPERTY(BlueprintReadOnly, Category = "TurretMind")
	FVector Velocity = FVector::ZeroVector;

	/** Distance from the turret muzzle to Location, in cm. */
	UPROPERTY(BlueprintReadOnly, Category = "TurretMind")
	float Distance = 0.0f;

	/** Threat as set on the target component. Meaning is yours; TurretMind only compares it. */
	UPROPERTY(BlueprintReadOnly, Category = "TurretMind")
	float Threat = 0.0f;

	/** Health as pushed in by your own health system, 0..1. Only read by the Lowest Health policy. */
	UPROPERTY(BlueprintReadOnly, Category = "TurretMind")
	float Health = 1.0f;

	/** How far along its path this target has come, 0..1. Only read by the Furthest Along Path policy. */
	UPROPERTY(BlueprintReadOnly, Category = "TurretMind")
	float PathProgress = 0.0f;

	/** Rough radius of the target in cm, used to keep the line-of-sight trace off the ground. */
	UPROPERTY(BlueprintReadOnly, Category = "TurretMind")
	float Radius = 50.0f;

	/** Seconds this target has been registered with the service. */
	UPROPERTY(BlueprintReadOnly, Category = "TurretMind")
	float TimeRegistered = 0.0f;

	/** Team id of the target. */
	UPROPERTY(BlueprintReadOnly, Category = "TurretMind")
	int32 TeamId = 0;
};

/** The turret side of a scoring call. Everything a Custom scorer needs about who is asking. */
USTRUCT(BlueprintType)
struct TURRETMIND_API FTurretMindTurretInfo
{
	GENERATED_BODY()

	/** The actor the turret component sits on. */
	UPROPERTY(BlueprintReadOnly, Category = "TurretMind")
	TObjectPtr<AActor> Actor = nullptr;

	/** World position of the muzzle: actor transform times the component's Muzzle Offset. */
	UPROPERTY(BlueprintReadOnly, Category = "TurretMind")
	FVector MuzzleLocation = FVector::ZeroVector;

	/** Forward vector of the turret actor, which is what the field-of-view cone is measured against. */
	UPROPERTY(BlueprintReadOnly, Category = "TurretMind")
	FVector Forward = FVector::ForwardVector;

	/** Maximum range from the profile, in cm. Handy for normalising your own distance term. */
	UPROPERTY(BlueprintReadOnly, Category = "TurretMind")
	float MaxRange = 0.0f;

	/** Team id of the turret. */
	UPROPERTY(BlueprintReadOnly, Category = "TurretMind")
	int32 TeamId = 0;
};

/**
 * Your own ranking function, bound per turret with UTurretMindComponent::SetCustomScorer and used when
 * the profile's policy is Custom.
 *
 * Return a fitness where higher is better. Keep it inside 0..1 if you want the profile's Target
 * Stickiness to keep meaning what it says on the tin — the hysteresis test is "new > current * (1 +
 * stickiness)", which only reads as a percentage when the scores are normalised.
 *
 * This is called once per candidate that survived range, team and cone filtering, so it runs a handful
 * of times per re-evaluation, not once per target in the level.
 */
DECLARE_DYNAMIC_DELEGATE_RetVal_TwoParams(float, FTurretMindScoreDelegate, const FTurretMindTurretInfo&, Turret, const FTurretMindTargetInfo&, Target);

/**
 * What the service did on the most recent frame.
 *
 * These numbers are the whole argument for the plugin, which is why they are on screen and not in a log
 * file: Acquisitions This Frame never exceeds the budget, Traces This Frame never exceeds the trace
 * budget, and Targets Considered stays far below Targets because the spatial index is doing its job.
 */
USTRUCT(BlueprintType)
struct TURRETMIND_API FTurretMindStats
{
	GENERATED_BODY()

	/** Turret components currently registered with this world's service. */
	UPROPERTY(BlueprintReadOnly, Category = "TurretMind")
	int32 Turrets = 0;

	/** Target components currently registered with this world's service. */
	UPROPERTY(BlueprintReadOnly, Category = "TurretMind")
	int32 Targets = 0;

	/** Turrets that were re-evaluated on the last frame. Never above Max Acquisitions Per Frame. */
	UPROPERTY(BlueprintReadOnly, Category = "TurretMind")
	int32 AcquisitionsThisFrame = 0;

	/** Line-of-sight traces fired on the last frame. Never above Max Line Traces Per Frame. */
	UPROPERTY(BlueprintReadOnly, Category = "TurretMind")
	int32 TracesThisFrame = 0;

	/** Grid cells opened by all of the last frame's re-evaluations together. */
	UPROPERTY(BlueprintReadOnly, Category = "TurretMind")
	int32 CellsVisited = 0;

	/** Targets that were actually looked at last frame. Compare this against Targets — that is the point. */
	UPROPERTY(BlueprintReadOnly, Category = "TurretMind")
	int32 TargetsConsidered = 0;

	/** Wall-clock cost of the whole budgeted acquisition round, in milliseconds. */
	UPROPERTY(BlueprintReadOnly, Category = "TurretMind")
	float AcquireMs = 0.0f;

	/** Turrets that changed target last frame. Low is good: high means the hysteresis is too weak. */
	UPROPERTY(BlueprintReadOnly, Category = "TurretMind")
	int32 SwitchesThisFrame = 0;

	/** Turrets that hold a target right now. */
	UPROPERTY(BlueprintReadOnly, Category = "TurretMind")
	int32 TurretsOnTarget = 0;

	/** Index entries refreshed last frame — one slice of the staggered index update. */
	UPROPERTY(BlueprintReadOnly, Category = "TurretMind")
	int32 IndexRefreshedThisFrame = 0;

	/** Line-of-sight answers served from the cache instead of a trace, last frame. */
	UPROPERTY(BlueprintReadOnly, Category = "TurretMind")
	int32 LosCacheHitsThisFrame = 0;
};
