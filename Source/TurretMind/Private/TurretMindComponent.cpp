// Copyright 2026 Simulated Flow. All Rights Reserved.

#include "TurretMindComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TurretMindProfile.h"
#include "TurretMindSubsystem.h"
#include "TurretMindTargetComponent.h"

UTurretMindComponent::UTurretMindComponent()
{
	// No tick, and this is not an oversight.
	//
	// A turret that ticks is a turret that runs its own query, and forty of those is the problem this
	// plugin exists to remove. The service ticks once per world and writes the answers in here. Reading
	// Get Aim Point from your own Blueprint's tick is free — it is a member read, not a query.
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	bWantsInitializeComponent = false;
}

void UTurretMindComponent::BeginPlay()
{
	Super::BeginPlay();
	RegisterWithService();
}

void UTurretMindComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromService();
	Super::EndPlay(EndPlayReason);
}

UTurretMindSubsystem* UTurretMindComponent::GetService() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UTurretMindSubsystem>() : nullptr;
}

void UTurretMindComponent::RegisterWithService()
{
	if (UTurretMindSubsystem* Service = GetService())
	{
		Service->RegisterTurret(this);
	}
}

void UTurretMindComponent::UnregisterFromService()
{
	if (UTurretMindSubsystem* Service = GetService())
	{
		Service->UnregisterTurret(this);
	}
	else
	{
		// World already gone. Nothing to unhook from, but leave the handle clean.
		TurretSlot = INDEX_NONE;
	}

	CurrentTarget = nullptr;
	CurrentTargetComponent = nullptr;
	bHasLineOfSight = false;
	TargetAcquiredTime = 0.0f;
}

// -------------------------------------------------------------------------------------------------------
// Answers
// -------------------------------------------------------------------------------------------------------

AActor* UTurretMindComponent::GetCurrentTarget() const
{
	return CurrentTarget.Get();
}

UTurretMindTargetComponent* UTurretMindComponent::GetCurrentTargetComponent() const
{
	return CurrentTargetComponent.Get();
}

bool UTurretMindComponent::HasTarget() const
{
	return CurrentTarget.IsValid();
}

FVector UTurretMindComponent::GetMuzzleLocation() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->GetActorTransform().TransformPosition(MuzzleOffset) : FVector::ZeroVector;
}

FRotator UTurretMindComponent::GetAimRotation() const
{
	if (!CurrentTarget.IsValid())
	{
		const AActor* Owner = GetOwner();
		return Owner ? Owner->GetActorRotation() : FRotator::ZeroRotator;
	}

	const FVector Direction = AimPoint - GetMuzzleLocation();
	return Direction.IsNearlyZero() ? FRotator::ZeroRotator : Direction.Rotation();
}

float UTurretMindComponent::GetTimeOnTarget() const
{
	if (!CurrentTarget.IsValid())
	{
		return 0.0f;
	}

	const UWorld* World = GetWorld();
	return World ? FMath::Max(0.0f, World->GetTimeSeconds() - TargetAcquiredTime) : 0.0f;
}

float UTurretMindComponent::GetDistanceToTarget() const
{
	if (!CurrentTarget.IsValid())
	{
		return 0.0f;
	}
	return static_cast<float>((AimPoint - GetMuzzleLocation()).Size());
}

UTurretMindProfile* UTurretMindComponent::GetEffectiveProfile() const
{
	if (Profile)
	{
		return Profile;
	}

	// Falls through to the project's default profile, and from there to a built-in one, so a component
	// dropped onto an actor with nothing filled in still behaves like a turret.
	if (UTurretMindSubsystem* Service = GetService())
	{
		return Service->GetDefaultProfile();
	}
	return nullptr;
}

// -------------------------------------------------------------------------------------------------------
// Control
// -------------------------------------------------------------------------------------------------------

void UTurretMindComponent::SetProfile(UTurretMindProfile* InProfile)
{
	if (Profile == InProfile)
	{
		return;
	}

	Profile = InProfile;

	// Ranges and policy just changed under the turret's feet; do not make it wait out the old interval.
	ForceReacquire();
}

void UTurretMindComponent::SetEnabled(bool bInEnabled)
{
	if (bEnabled == bInEnabled)
	{
		return;
	}

	bEnabled = bInEnabled;

	if (bEnabled)
	{
		ForceReacquire();
	}
	// Powering down is handled by the service's aim pass on the next frame, which drops the target and
	// fires On Target Lost. Doing it here would broadcast from inside a setter, which is a trap.
}

void UTurretMindComponent::ForceReacquire()
{
	if (UTurretMindSubsystem* Service = GetService())
	{
		// Due now, target kept. Re-registering would work too, and would throw away the held target
		// along the way — which is exactly what a caller asking for a re-think does not want.
		Service->MakeTurretDue(this);
	}
}

void UTurretMindComponent::ClearTarget()
{
	if (UTurretMindSubsystem* Service = GetService())
	{
		// The service owns the bookkeeping and fires On Target Lost, so there is one code path for
		// losing a target rather than two that have to agree with each other.
		Service->ClearTurretTarget(this);
		return;
	}

	CurrentTarget = nullptr;
	CurrentTargetComponent = nullptr;
	bHasLineOfSight = false;
	TargetAcquiredTime = 0.0f;
}

void UTurretMindComponent::SetCustomScorer(const FTurretMindScoreDelegate& InScorer)
{
	// The scorer runs inside the service's acquisition pass, over an array the service is walking.
	// Spawning or destroying actors from it would move that array underneath the loop, so do neither:
	// read the two structs, return a number, and leave the world alone.
	CustomScorer = InScorer;
}

void UTurretMindComponent::ClearCustomScorer()
{
	CustomScorer.Unbind();
}
