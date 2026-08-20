// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TurretMindTypes.h"
#include "TurretMindStatics.generated.h"

class AActor;
class UObject;
class UTurretMindComponent;
class UTurretMindProfile;
class UTurretMindSubsystem;
class UTurretMindTargetComponent;

/**
 * The short way into TurretMind from a Blueprint that does not want to hold a reference to anything.
 *
 * Every function here is a thin wrapper over the world's service. Nothing in this library does work of
 * its own, and nothing here shoots.
 */
UCLASS(meta = (DisplayName = "TurretMind"))
class TURRETMIND_API UTurretMindStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The acquisition service for this world, or null outside a game world. */
	UFUNCTION(BlueprintPure, Category = "TurretMind", meta = (WorldContext = "WorldContextObject"))
	static UTurretMindSubsystem* GetTurretMindSubsystem(const UObject* WorldContextObject);

	/**
	 * Make an actor shootable without opening its Blueprint: adds a TurretMind Target component,
	 * fills it in and registers it.
	 *
	 * Meant for actors you do not own the class of — a third-party enemy, a spawned prop, anything
	 * coming out of a system that does not know TurretMind exists. Calling it twice on the same actor
	 * returns the component that is already there rather than stacking a second one.
	 */
	UFUNCTION(BlueprintCallable, Category = "TurretMind", meta = (AdvancedDisplay = "2"))
	static UTurretMindTargetComponent* RegisterTarget(AActor* Actor, int32 TeamId = 1, float Threat = 10.0f, float Radius = 50.0f);

	/** Remove the TurretMind Target component this library added, and with it the actor's registration. */
	UFUNCTION(BlueprintCallable, Category = "TurretMind")
	static void UnregisterTarget(AActor* Actor);

	/** The TurretMind Target component on this actor, or null. */
	UFUNCTION(BlueprintPure, Category = "TurretMind")
	static UTurretMindTargetComponent* GetTargetComponent(AActor* Actor);

	/** The TurretMind Turret component on this actor, or null. */
	UFUNCTION(BlueprintPure, Category = "TurretMind")
	static UTurretMindComponent* GetTurretComponent(AActor* Actor);

	/**
	 * What the service did on the most recent frame: budgets, cells opened, targets considered, time
	 * spent. The same numbers the on-screen box draws.
	 */
	UFUNCTION(BlueprintPure, Category = "TurretMind", meta = (WorldContext = "WorldContextObject"))
	static FTurretMindStats GetTurretMindStats(const UObject* WorldContextObject);

	/** Change the hard cap on turret re-evaluations per frame. Same as TurretMind.Budget. */
	UFUNCTION(BlueprintCallable, Category = "TurretMind", meta = (WorldContext = "WorldContextObject"))
	static void SetAcquisitionBudget(const UObject* WorldContextObject, int32 MaxAcquisitionsPerFrame);

	/** Change the hard cap on line-of-sight traces per frame. Same as TurretMind.TraceBudget. */
	UFUNCTION(BlueprintCallable, Category = "TurretMind", meta = (WorldContext = "WorldContextObject"))
	static void SetTraceBudget(const UObject* WorldContextObject, int32 MaxLineTracesPerFrame);

	/** Force one policy on every turret in this world. Clear Policy Override hands it back. */
	UFUNCTION(BlueprintCallable, Category = "TurretMind", meta = (WorldContext = "WorldContextObject"))
	static void SetPolicyOverride(const UObject* WorldContextObject, ETurretMindPolicy Policy);

	/** Hand the policy decision back to each turret's own profile. */
	UFUNCTION(BlueprintCallable, Category = "TurretMind", meta = (WorldContext = "WorldContextObject"))
	static void ClearPolicyOverride(const UObject* WorldContextObject);

	/** Force the line-of-sight requirement on or off for every turret in this world. */
	UFUNCTION(BlueprintCallable, Category = "TurretMind", meta = (WorldContext = "WorldContextObject"))
	static void SetLineOfSightOverride(const UObject* WorldContextObject, bool bRequireLineOfSight);

	/** Hand the line-of-sight decision back to each turret's own profile. */
	UFUNCTION(BlueprintCallable, Category = "TurretMind", meta = (WorldContext = "WorldContextObject"))
	static void ClearLineOfSightOverride(const UObject* WorldContextObject);

	/** Show or hide the on-screen statistics box. Same as TurretMind.Stats. */
	UFUNCTION(BlueprintCallable, Category = "TurretMind|Debug", meta = (WorldContext = "WorldContextObject"))
	static void SetShowStats(const UObject* WorldContextObject, bool bShowStats);

	/** Master switch for the world-space debug lines. Same as TurretMind.Draw. */
	UFUNCTION(BlueprintCallable, Category = "TurretMind|Debug", meta = (WorldContext = "WorldContextObject"))
	static void SetDrawDebug(const UObject* WorldContextObject, bool bDrawDebug);

	/** Include the range circles in the world-space debug draw. Same as TurretMind.DrawRanges. */
	UFUNCTION(BlueprintCallable, Category = "TurretMind|Debug", meta = (WorldContext = "WorldContextObject"))
	static void SetDrawRanges(const UObject* WorldContextObject, bool bDrawRanges);

	/**
	 * One-off query for something that is not a registered turret: the best target around Origin under
	 * this profile, plus the point to lead to.
	 *
	 * Unbudgeted and uncached — it does the whole scoring pass, traces included, right now. Fine for a
	 * missile re-targeting mid-flight or a placement preview; not fine in a loop over a hundred actors,
	 * which is what the turret component is for.
	 */
	UFUNCTION(BlueprintCallable, Category = "TurretMind", meta = (WorldContext = "WorldContextObject", AdvancedDisplay = "6"))
	static bool FindBestTarget(const UObject* WorldContextObject, FVector Origin, FVector Forward, UTurretMindProfile* Profile,
		int32 TeamId, AActor*& OutTarget, FVector& OutAimPoint, AActor* IgnoreActor = nullptr);

	/**
	 * Where to aim so a projectile of this speed meets a target at this position and velocity.
	 * Speed at or below zero means hitscan and hands back the target position unchanged.
	 */
	UFUNCTION(BlueprintPure, Category = "TurretMind")
	static FVector PredictAimPoint(FVector MuzzleLocation, FVector TargetLocation, FVector TargetVelocity,
		float ProjectileSpeed, float MaxLeadSeconds = 3.0f);
};
