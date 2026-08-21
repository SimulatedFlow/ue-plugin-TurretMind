// Copyright 2026 Simulated Flow. All Rights Reserved.

#include "TurretMindTargetComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TurretMindSubsystem.h"

UTurretMindTargetComponent::UTurretMindTargetComponent()
{
	// A target never ticks. The service samples it a slice at a time, which is a fraction of the cost
	// of two hundred components each doing their own bookkeeping every frame.
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	bWantsInitializeComponent = false;
}

void UTurretMindTargetComponent::BeginPlay()
{
	Super::BeginPlay();
	RegisterWithService();
}

void UTurretMindTargetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromService();
	Super::EndPlay(EndPlayReason);
}

UTurretMindSubsystem* UTurretMindTargetComponent::GetService() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UTurretMindSubsystem>() : nullptr;
}

void UTurretMindTargetComponent::RegisterWithService()
{
	if (UTurretMindSubsystem* Service = GetService())
	{
		Service->RegisterTarget(this);
	}
}

void UTurretMindTargetComponent::UnregisterFromService()
{
	if (UTurretMindSubsystem* Service = GetService())
	{
		Service->UnregisterTarget(this);
	}
	else
	{
		IndexSlot = INDEX_NONE;
	}
}

FVector UTurretMindTargetComponent::GetMeasuredVelocity() const
{
	return CachedVelocity;
}

FVector UTurretMindTargetComponent::GetAimLocation() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->GetActorTransform().TransformPosition(AimOffset) : FVector::ZeroVector;
}

// -------------------------------------------------------------------------------------------------------
// Setters
//
// Each of these pushes the new value into the index straight away instead of waiting for this target's
// turn in the staggered refresh. That matters for the values a game changes in response to an event: a
// unit that just became untargetable has to stop being shot at now, not in three frames.
// -------------------------------------------------------------------------------------------------------

void UTurretMindTargetComponent::SetTeamId(int32 InTeamId)
{
	if (TeamId == InTeamId)
	{
		return;
	}
	TeamId = InTeamId;
	if (UTurretMindSubsystem* Service = GetService())
	{
		Service->RefreshTargetNow(this);
	}
}

void UTurretMindTargetComponent::SetThreat(float InThreat)
{
	if (FMath::IsNearlyEqual(Threat, InThreat))
	{
		return;
	}
	Threat = InThreat;
	if (UTurretMindSubsystem* Service = GetService())
	{
		Service->RefreshTargetNow(this);
	}
}

void UTurretMindTargetComponent::SetHealth(float InHealth)
{
	const float Clamped = FMath::Clamp(InHealth, 0.0f, 1.0f);
	if (FMath::IsNearlyEqual(Health, Clamped))
	{
		return;
	}
	Health = Clamped;
	if (UTurretMindSubsystem* Service = GetService())
	{
		Service->RefreshTargetNow(this);
	}
}

void UTurretMindTargetComponent::SetPathProgress(float InPathProgress)
{
	const float Clamped = FMath::Clamp(InPathProgress, 0.0f, 1.0f);
	if (FMath::IsNearlyEqual(PathProgress, Clamped))
	{
		return;
	}
	PathProgress = Clamped;
	if (UTurretMindSubsystem* Service = GetService())
	{
		Service->RefreshTargetNow(this);
	}
}

void UTurretMindTargetComponent::SetTargetable(bool bInTargetable)
{
	if (bTargetable == bInTargetable)
	{
		return;
	}
	bTargetable = bInTargetable;
	if (UTurretMindSubsystem* Service = GetService())
	{
		Service->RefreshTargetNow(this);
	}
}
