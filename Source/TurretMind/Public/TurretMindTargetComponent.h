// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "TurretMindTargetComponent.generated.h"

class UTurretMindSubsystem;

/**
 * Put this on anything that is allowed to be shot at.
 *
 * It carries no logic of its own. It is a label plus a handful of numbers, and it puts the actor into
 * the service's spatial index at BeginPlay and takes it out again at EndPlay. That is the entire deal:
 * a target does not tick, does not trace, and does not know that turrets exist.
 *
 * Velocity is measured, not read. Asking a movement component would tie this to characters and pawns,
 * and a target is allowed to be anything — a moving platform, a projectile, an actor being dragged
 * around by a spline, a Blueprint that just sets its own location every frame. The service measures the
 * distance travelled between two index samples and smooths it, which works for all of those and needs
 * nothing from you. If the owner does keep a proper velocity, tick Use Owner Velocity and it is used
 * verbatim instead.
 */
UCLASS(ClassGroup = (TurretMind), meta = (BlueprintSpawnableComponent, DisplayName = "TurretMind Target"))
class TURRETMIND_API UTurretMindTargetComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTurretMindTargetComponent();

	// UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ----------------------------------------------------------------------------------------------
	// What a turret sees
	// ----------------------------------------------------------------------------------------------

	/** Whose side this target is on. The turret's profile decides what to do with the difference. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TurretMind", meta = (ExposeOnSpawn = "true"))
	int32 TeamId = 1;

	/**
	 * How dangerous this target is. Only read by the Highest Threat policy, and normalised against the
	 * profile's Threat Scale. The unit is yours — TurretMind never invents a number for it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TurretMind", meta = (ExposeOnSpawn = "true"))
	float Threat = 10.0f;

	/**
	 * Health as a fraction, 0..1. Only read by the Lowest Health policy.
	 *
	 * TurretMind has no health system and will never have one: push your own number in with Set Health
	 * whenever it changes. Left at 1 it simply makes Lowest Health behave like a tie between everyone.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TurretMind", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Health = 1.0f;

	/**
	 * How far along its path this target has come, 0..1. Only read by the Furthest Along Path policy —
	 * the tower defence answer, where the leaker about to reach the base outranks the fat one at the
	 * spawn. Your path logic sets it; a spline follower usually already has this number.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TurretMind", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PathProgress = 0.0f;

	/** Rough radius in cm. Keeps the line-of-sight trace off the ground and out of the target's feet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TurretMind", meta = (ClampMin = "0.0", UIMax = "1000.0"))
	float Radius = 50.0f;

	/**
	 * Where on the actor a turret aims, in the actor's own space. Default is roughly chest height on a
	 * standard character; a tank wants something lower, a flyer something higher.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TurretMind")
	FVector AimOffset = FVector(0.0f, 0.0f, 60.0f);

	/**
	 * Off makes this actor invisible to every turret without unregistering it — cloaked, phased,
	 * downed, in the spawn shield. Turrets already locked on drop it on their next re-evaluation.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TurretMind")
	bool bTargetable = true;

	/**
	 * Take the owner's own velocity instead of measuring the distance between index samples. Correct
	 * and free when the owner is a character or has physics; wrong (always zero) when the actor is moved
	 * by setting its transform, which is why measuring is the default.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TurretMind")
	bool bUseOwnerVelocity = false;

	// ----------------------------------------------------------------------------------------------
	// Setters — these exist so the value reaches the index without waiting for the next slice
	// ----------------------------------------------------------------------------------------------

	/** Change the team. Pushed into the index immediately. */
	UFUNCTION(BlueprintCallable, Category = "TurretMind")
	void SetTeamId(int32 InTeamId);

	/** Change the threat value. Pushed into the index immediately. */
	UFUNCTION(BlueprintCallable, Category = "TurretMind")
	void SetThreat(float InThreat);

	/** Push in your own health fraction, 0..1. Pushed into the index immediately. */
	UFUNCTION(BlueprintCallable, Category = "TurretMind")
	void SetHealth(float InHealth);

	/** Report how far along the path this target has come, 0..1. Pushed into the index immediately. */
	UFUNCTION(BlueprintCallable, Category = "TurretMind")
	void SetPathProgress(float InPathProgress);

	/** Make this target visible or invisible to turrets without unregistering it. */
	UFUNCTION(BlueprintCallable, Category = "TurretMind")
	void SetTargetable(bool bInTargetable);

	// ----------------------------------------------------------------------------------------------
	// Queries
	// ----------------------------------------------------------------------------------------------

	/** Velocity the service is currently leading against, in cm/s. Measured unless Use Owner Velocity. */
	UFUNCTION(BlueprintPure, Category = "TurretMind")
	FVector GetMeasuredVelocity() const;

	/** True while this component is in the service's index. Not UActorComponent::IsRegistered. */
	UFUNCTION(BlueprintPure, Category = "TurretMind")
	bool IsRegisteredWithService() const { return IndexSlot != INDEX_NONE; }

	/** Where turrets aim at this actor: actor transform applied to Aim Offset. */
	UFUNCTION(BlueprintPure, Category = "TurretMind")
	FVector GetAimLocation() const;

	/** Put this component into the index by hand. BeginPlay already does it; this is for odd lifetimes. */
	UFUNCTION(BlueprintCallable, Category = "TurretMind")
	void RegisterWithService();

	/** Take this component out of the index by hand. EndPlay already does it. */
	UFUNCTION(BlueprintCallable, Category = "TurretMind")
	void UnregisterFromService();

private:
	friend class UTurretMindSubsystem;

	/** Slot in the subsystem's target array, or INDEX_NONE while unregistered. Never a UPROPERTY. */
	int32 IndexSlot = INDEX_NONE;

	/** Generation of that slot, so a stale handle to a reused slot is caught instead of followed. */
	uint32 IndexSerial = 0;

	/** Copy of the last velocity the index measured, kept here so the Blueprint getter costs nothing. */
	FVector CachedVelocity = FVector::ZeroVector;

	/** Resolve this world's service, or null when there is none (CDO, cooked-out world, shutdown). */
	UTurretMindSubsystem* GetService() const;
};
