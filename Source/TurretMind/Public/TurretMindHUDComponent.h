// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "TurretMindHUDComponent.generated.h"

class UCanvas;

/**
 * Already have your own HUD class? Add this to it and call Draw Stats (Canvas) from your own Draw HUD.
 *
 * That is the whole component. It exists so that using TurretMind's statistics box never means giving
 * up the HUD class your project already has — which is the reason most drop-in HUD features go unused.
 */
UCLASS(ClassGroup = (TurretMind), meta = (BlueprintSpawnableComponent, DisplayName = "TurretMind HUD"))
class TURRETMIND_API UTurretMindHUDComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTurretMindHUDComponent();

	/** Draw the statistics box onto this canvas, if the box is switched on. One line, in Draw HUD. */
	UFUNCTION(BlueprintCallable, Category = "TurretMind")
	void DrawStats(UCanvas* Canvas);
};
