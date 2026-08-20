// Copyright 2026 Simulated Flow. All Rights Reserved.

#include "TurretMindHUDComponent.h"

#include "Engine/World.h"
#include "TurretMindSubsystem.h"

UTurretMindHUDComponent::UTurretMindHUDComponent()
{
	// Nothing to tick: the service owns the numbers, this component only forwards a canvas.
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	bWantsInitializeComponent = false;
}

void UTurretMindHUDComponent::DrawStats(UCanvas* Canvas)
{
	if (!Canvas)
	{
		return;
	}

	if (const UWorld* World = GetWorld())
	{
		if (UTurretMindSubsystem* Service = World->GetSubsystem<UTurretMindSubsystem>())
		{
			Service->DrawStats(Canvas);
		}
	}
}
