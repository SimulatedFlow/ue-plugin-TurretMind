// Copyright 2026 Silvan Teufel. All Rights Reserved.

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
