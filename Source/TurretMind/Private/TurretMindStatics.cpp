// Copyright 2026 Simulated Flow. All Rights Reserved.

#include "TurretMindStatics.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TurretMindComponent.h"
#include "TurretMindSubsystem.h"
#include "TurretMindTargetComponent.h"

namespace
{
	UTurretMindSubsystem* ResolveService(const UObject* WorldContextObject)
	{
		if (!GEngine || !WorldContextObject)
		{
			return nullptr;
		}
		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
		return World ? World->GetSubsystem<UTurretMindSubsystem>() : nullptr;
	}
}

UTurretMindSubsystem* UTurretMindStatics::GetTurretMindSubsystem(const UObject* WorldContextObject)
{
	return ResolveService(WorldContextObject);
}

UTurretMindTargetComponent* UTurretMindStatics::RegisterTarget(AActor* Actor, int32 TeamId, float Threat, float Radius)
{
	if (!IsValid(Actor))
	{
		return nullptr;
	}

	if (UTurretMindTargetComponent* Existing = Actor->FindComponentByClass<UTurretMindTargetComponent>())
	{
		// Already shootable. Update the numbers rather than stacking a second component on the actor.
		Existing->SetTeamId(TeamId);
		Existing->SetThreat(Threat);
		Existing->Radius = Radius;
		if (!Existing->IsRegisteredWithService())
		{
			Existing->RegisterWithService();
		}
		return Existing;
	}

	UTurretMindTargetComponent* Component = NewObject<UTurretMindTargetComponent>(Actor, UTurretMindTargetComponent::StaticClass());
	if (!Component)
	{
		return nullptr;
	}

	Component->TeamId = TeamId;
	Component->Threat = Threat;
	Component->Radius = Radius;
	Component->RegisterComponent();

	// RegisterComponent runs BeginPlay for an actor that has already begun play, and that is what puts
	// the component into the index. For one that has not, BeginPlay will come round on its own.
	if (!Component->IsRegisteredWithService())
	{
		Component->RegisterWithService();
	}

	return Component;
}

void UTurretMindStatics::UnregisterTarget(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return;
	}

	if (UTurretMindTargetComponent* Component = Actor->FindComponentByClass<UTurretMindTargetComponent>())
	{
		Component->UnregisterFromService();
		Component->DestroyComponent();
	}
}

UTurretMindTargetComponent* UTurretMindStatics::GetTargetComponent(AActor* Actor)
{
	return IsValid(Actor) ? Actor->FindComponentByClass<UTurretMindTargetComponent>() : nullptr;
}

UTurretMindComponent* UTurretMindStatics::GetTurretComponent(AActor* Actor)
{
	return IsValid(Actor) ? Actor->FindComponentByClass<UTurretMindComponent>() : nullptr;
}

FTurretMindStats UTurretMindStatics::GetTurretMindStats(const UObject* WorldContextObject)
{
	if (const UTurretMindSubsystem* Service = ResolveService(WorldContextObject))
	{
		return Service->GetStats();
	}
	return FTurretMindStats();
}

void UTurretMindStatics::SetAcquisitionBudget(const UObject* WorldContextObject, int32 MaxAcquisitionsPerFrame)
{
	if (UTurretMindSubsystem* Service = ResolveService(WorldContextObject))
	{
		Service->SetAcquisitionBudget(MaxAcquisitionsPerFrame);
	}
}

void UTurretMindStatics::SetTraceBudget(const UObject* WorldContextObject, int32 MaxLineTracesPerFrame)
{
	if (UTurretMindSubsystem* Service = ResolveService(WorldContextObject))
	{
		Service->SetTraceBudget(MaxLineTracesPerFrame);
	}
}

void UTurretMindStatics::SetPolicyOverride(const UObject* WorldContextObject, ETurretMindPolicy Policy)
{
	if (UTurretMindSubsystem* Service = ResolveService(WorldContextObject))
	{
		Service->SetPolicyOverride(Policy);
	}
}

void UTurretMindStatics::ClearPolicyOverride(const UObject* WorldContextObject)
{
	if (UTurretMindSubsystem* Service = ResolveService(WorldContextObject))
	{
		Service->ClearPolicyOverride();
	}
}

void UTurretMindStatics::SetLineOfSightOverride(const UObject* WorldContextObject, bool bRequireLineOfSight)
{
	if (UTurretMindSubsystem* Service = ResolveService(WorldContextObject))
	{
		Service->SetLineOfSightOverride(bRequireLineOfSight);
	}
}

void UTurretMindStatics::ClearLineOfSightOverride(const UObject* WorldContextObject)
{
	if (UTurretMindSubsystem* Service = ResolveService(WorldContextObject))
	{
		Service->ClearLineOfSightOverride();
	}
}

void UTurretMindStatics::SetShowStats(const UObject* WorldContextObject, bool bShowStats)
{
	if (UTurretMindSubsystem* Service = ResolveService(WorldContextObject))
	{
		Service->SetShowStats(bShowStats);
	}
}

void UTurretMindStatics::SetDrawDebug(const UObject* WorldContextObject, bool bDrawDebug)
{
	if (UTurretMindSubsystem* Service = ResolveService(WorldContextObject))
	{
		Service->SetDrawDebug(bDrawDebug);
	}
}

void UTurretMindStatics::SetDrawRanges(const UObject* WorldContextObject, bool bDrawRanges)
{
	if (UTurretMindSubsystem* Service = ResolveService(WorldContextObject))
	{
		Service->SetDrawRanges(bDrawRanges);
	}
}

bool UTurretMindStatics::FindBestTarget(const UObject* WorldContextObject, FVector Origin, FVector Forward, UTurretMindProfile* Profile,
	int32 TeamId, AActor*& OutTarget, FVector& OutAimPoint, AActor* IgnoreActor)
{
	OutTarget = nullptr;
	OutAimPoint = Origin;

	if (UTurretMindSubsystem* Service = ResolveService(WorldContextObject))
	{
		return Service->FindBestTarget(Origin, Forward, Profile, TeamId, OutTarget, OutAimPoint, IgnoreActor);
	}
	return false;
}

FVector UTurretMindStatics::PredictAimPoint(FVector MuzzleLocation, FVector TargetLocation, FVector TargetVelocity,
	float ProjectileSpeed, float MaxLeadSeconds)
{
	return UTurretMindSubsystem::PredictAimPoint(MuzzleLocation, TargetLocation, TargetVelocity, ProjectileSpeed, MaxLeadSeconds);
}
