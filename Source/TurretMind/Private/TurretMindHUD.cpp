// Copyright 2026 Simulated Flow. All Rights Reserved.

#include "TurretMindHUD.h"

#include "TurretMindHUDComponent.h"

ATurretMindHUD::ATurretMindHUD()
{
	StatsComponent = CreateDefaultSubobject<UTurretMindHUDComponent>(TEXT("TurretMindStats"));
}

void ATurretMindHUD::DrawHUD()
{
	Super::DrawHUD();

	if (bDrawTurretMindStats && StatsComponent)
	{
		StatsComponent->DrawStats(Canvas);
	}
}
