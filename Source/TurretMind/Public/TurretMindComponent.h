// Copyright 2026 Simulated Flow. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "TurretMindTypes.h"
#include "TurretMindComponent.generated.h"

class UTurretMindProfile;
class UTurretMindSubsystem;
class UTurretMindTargetComponent;

/** Fired when a turret goes from holding nothing to holding a target. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTurretMindTargetAcquired, AActor*, Target);

/** Fired when a turret loses its target and has nothing to replace it with. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTurretMindTargetLost, AActor*, PreviousTarget);

/** Fired when a turret trades one target for another in the same re-evaluation. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FTurretMindTargetSwitched, AActor*, PreviousTarget, AActor*, NewTarget);

/**
 * Put this on a turret. It is the whole client-side API.
 *
 * The component does no work. It has no tick — bCanEverTick is false and stays false — because the
 * point of this plugin is that forty turrets do not each run their own query. It registers with the
 * world's service at BeginPlay, unregisters at EndPlay, and in between the service writes the answers
 * into it: what to shoot at, where to lead, whether the line is clear.
 *
 * A turret Blueprint's tick then does exactly one thing: read Get Aim Point (or Get Aim Rotation) and
 * turn the barrel towards it. Firing, damage and hit detection are yours — TurretMind never pulls a
 * trigger.
 */
UCLASS(ClassGroup = (TurretMind), meta = (BlueprintSpawnableComponent, DisplayName = "TurretMind Turret"))
class TURRETMIND_API UTurretMindComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTurretMindComponent();

	// UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ----------------------------------------------------------------------------------------------
	// Setup
	// ----------------------------------------------------------------------------------------------

	/**
	 * Ranges, policy, stickiness, lead speed — everything that makes this turret behave the way it
	 * does. Empty falls back to the project's default profile, and failing that to a built-in one, so a
	 * freshly added component already works.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TurretMind", meta = (ExposeOnSpawn = "true"))
	TObjectPtr<UTurretMindProfile> Profile;

	/** Whose side this turret is on. Compared against each target's Team Id by the profile's filter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TurretMind", meta = (ExposeOnSpawn = "true"))
	int32 TeamId = 0;

	/**
	 * Where the shot leaves the turret, in the actor's own space. Range, the traverse cone and the
	 * line-of-sight trace all start here, not at the actor origin — which matters for a turret whose
	 * barrel sits three metres above its base.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TurretMind")
	FVector MuzzleOffset = FVector(0.0f, 0.0f, 150.0f);

	/**
	 * Off takes this turret out of the round-robin without unregistering it: it keeps no target, fires
	 * no events, and costs nothing. Use it for a turret that is powered down, building, or destroyed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TurretMind")
	bool bEnabled = true;

	// ----------------------------------------------------------------------------------------------
	// Answers — written by the service, read by your turret
	// ----------------------------------------------------------------------------------------------

	/** The actor this turret is on, or null. */
	UFUNCTION(BlueprintPure, Category = "TurretMind")
	AActor* GetCurrentTarget() const;

	/** The target component this turret is on, or null. */
	UFUNCTION(BlueprintPure, Category = "TurretMind")
	UTurretMindTargetComponent* GetCurrentTargetComponent() const;

	/**
	 * Where to point the barrel, in world space.
	 *
	 * Updated every frame, for every turret, whatever the acquisition budget is doing — aiming is cheap,
	 * choosing is not. With a projectile speed set this is the intercept point, ahead of the target;
	 * with hitscan it is the target's aim location itself. Meaningless while Has Target is false.
	 */
	UFUNCTION(BlueprintPure, Category = "TurretMind")
	FVector GetAimPoint() const { return AimPoint; }

	/** Get Aim Point as a rotation from the muzzle. Feed it straight into a Set Relative Rotation. */
	UFUNCTION(BlueprintPure, Category = "TurretMind")
	FRotator GetAimRotation() const;

	/** True while this turret holds a target. */
	UFUNCTION(BlueprintPure, Category = "TurretMind")
	bool HasTarget() const;

	/**
	 * Whether the last line-of-sight answer for the current target was clear. Always true when the
	 * profile does not require line of sight. Cached, so reading it is free.
	 */
	UFUNCTION(BlueprintPure, Category = "TurretMind")
	bool HasLineOfSight() const { return bHasLineOfSight; }

	/** Seconds this turret has been holding its current target. Zero when it has none. */
	UFUNCTION(BlueprintPure, Category = "TurretMind")
	float GetTimeOnTarget() const;

	/** Distance from the muzzle to the current aim point, in cm. Zero when there is no target. */
	UFUNCTION(BlueprintPure, Category = "TurretMind")
	float GetDistanceToTarget() const;

	/** World position of the muzzle: the actor transform applied to Muzzle Offset. */
	UFUNCTION(BlueprintPure, Category = "TurretMind")
	FVector GetMuzzleLocation() const;

	/** The profile actually in use, after the fallbacks. Never null once BeginPlay has run. */
	UFUNCTION(BlueprintPure, Category = "TurretMind")
	UTurretMindProfile* GetEffectiveProfile() const;

	// ----------------------------------------------------------------------------------------------
	// Control
	// ----------------------------------------------------------------------------------------------

	/** Swap the profile at runtime. Takes effect on the next re-evaluation. */
	UFUNCTION(BlueprintCallable, Category = "TurretMind")
	void SetProfile(UTurretMindProfile* InProfile);

	/** Power the turret up or down. Powering down drops the current target and fires On Target Lost. */
	UFUNCTION(BlueprintCallable, Category = "TurretMind")
	void SetEnabled(bool bInEnabled);

	/**
	 * Put this turret at the front of the queue: it becomes due immediately instead of waiting out its
	 * Reacquire Interval. It still costs one slot of the per-frame budget, so calling it on forty
	 * turrets at once does not make forty re-evaluations happen this frame.
	 */
	UFUNCTION(BlueprintCallable, Category = "TurretMind")
	void ForceReacquire();

	/** Drop the current target without picking a new one. The next re-evaluation starts from nothing. */
	UFUNCTION(BlueprintCallable, Category = "TurretMind")
	void ClearTarget();

	/**
	 * Bind your own ranking function, used when the profile's policy is Custom. Higher is better; keep
	 * it in 0..1 so the profile's Target Stickiness keeps meaning "twenty per cent better".
	 */
	UFUNCTION(BlueprintCallable, Category = "TurretMind")
	void SetCustomScorer(const FTurretMindScoreDelegate& InScorer);

	/** Forget the custom scorer. A Custom-policy turret then falls back to Nearest. */
	UFUNCTION(BlueprintCallable, Category = "TurretMind")
	void ClearCustomScorer();

	/** True while this component is in the service's turret list. Not UActorComponent::IsRegistered. */
	UFUNCTION(BlueprintPure, Category = "TurretMind")
	bool IsRegisteredWithService() const { return TurretSlot != INDEX_NONE; }

	// ----------------------------------------------------------------------------------------------
	// Events
	// ----------------------------------------------------------------------------------------------

	/** This turret went from nothing to something. */
	UPROPERTY(BlueprintAssignable, Category = "TurretMind")
	FTurretMindTargetAcquired OnTargetAcquired;

	/** This turret lost its target and found nothing to replace it with. */
	UPROPERTY(BlueprintAssignable, Category = "TurretMind")
	FTurretMindTargetLost OnTargetLost;

	/** This turret traded one target for another. On Target Acquired does not also fire for this. */
	UPROPERTY(BlueprintAssignable, Category = "TurretMind")
	FTurretMindTargetSwitched OnTargetSwitched;

	/** Put this component into the service by hand. BeginPlay already does it. */
	UFUNCTION(BlueprintCallable, Category = "TurretMind")
	void RegisterWithService();

	/** Take this component out of the service by hand. EndPlay already does it. */
	UFUNCTION(BlueprintCallable, Category = "TurretMind")
	void UnregisterFromService();

private:
	friend class UTurretMindSubsystem;

	/** Slot in the subsystem's turret array, or INDEX_NONE while unregistered. */
	int32 TurretSlot = INDEX_NONE;

	/** Generation of that slot, so a stale handle to a reused slot is caught instead of followed. */
	uint32 TurretSerial = 0;

	/** Where the barrel should point. Refreshed every frame by the service's cheap aim pass. */
	UPROPERTY(Transient)
	FVector AimPoint = FVector::ZeroVector;

	/** The actor currently held. Kept here so Get Current Target does not have to enter the subsystem. */
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CurrentTarget;

	/** The target component currently held. */
	UPROPERTY(Transient)
	TWeakObjectPtr<UTurretMindTargetComponent> CurrentTargetComponent;

	/** Last line-of-sight verdict for the current target. */
	UPROPERTY(Transient)
	bool bHasLineOfSight = true;

	/** World time the current target was picked up, for Get Time On Target. */
	UPROPERTY(Transient)
	float TargetAcquiredTime = 0.0f;

	/** Optional Blueprint ranking function for the Custom policy. */
	UPROPERTY(Transient)
	FTurretMindScoreDelegate CustomScorer;

	/** Resolve this world's service, or null when there is none. */
	UTurretMindSubsystem* GetService() const;
};
