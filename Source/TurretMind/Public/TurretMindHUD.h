// Copyright 2026 Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "TurretMindHUD.generated.h"

class UTurretMindHUDComponent;

/**
 * The ready-made HUD class: set it as your game mode's HUD Class, type TurretMind.Stats 1, and the
 * numbers are on screen with nothing else to wire up.
 *
 * Deliberately AHUD-based. Draw HUD runs in a cooked Shipping build, which is where a performance
 * claim has to be checkable — a debug overlay that only exists in the editor proves nothing about the
 * build the player gets. The editor-viewport path in the subsystem is the second attachment point, not
 * this one.
 *
 * Already have your own HUD class? Do not replace it — add UTurretMindHUDComponent to it and call
 * Draw Stats (Canvas) from your own Draw HUD instead. Both paths end in the same subsystem call.
 */
UCLASS(meta = (DisplayName = "TurretMind HUD"))
class TURRETMIND_API ATurretMindHUD : public AHUD
{
	GENERATED_BODY()

public:
	ATurretMindHUD();

	// AHUD interface
	virtual void DrawHUD() override;

	/** Draw the statistics box when this HUD renders. Off makes this behave like a plain AHUD. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TurretMind")
	bool bDrawTurretMindStats = true;

protected:
	/** Present so a Blueprint child of this class can reach the same call the C++ path uses. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TurretMind")
	TObjectPtr<UTurretMindHUDComponent> StatsComponent;
};
