// Copyright 2026 Silvan Teufel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TurretMindSettings.generated.h"

class UTurretMindProfile;

/**
 * Project-wide defaults for the acquisition service.
 * Edit under Project Settings > Plugins > TurretMind; stored in DefaultGame.ini.
 *
 * Everything here is read once, when a world's subsystem comes up. The budgets, the debug switches and
 * the policy override can all be moved at runtime afterwards — through the subsystem's setters, the
 * Blueprint library, or the TurretMind.* console commands — without touching the config.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "TurretMind"))
class TURRETMIND_API UTurretMindSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UTurretMindSettings();

	// UDeveloperSettings interface
	virtual FName GetCategoryName() const override;
	virtual FName GetSectionName() const override;

	/** Convenience accessor. Never returns null. */
	static const UTurretMindSettings& Get();

	// ----------------------------------------------------------------------------------------------
	// Spatial index
	// ----------------------------------------------------------------------------------------------

	/**
	 * Edge length of one grid cell in cm, on the XY plane.
	 *
	 * The rule of thumb: a cell should hold a handful of targets, and a turret's range should cover a
	 * handful of cells. Too small and a long-range turret opens hundreds of near-empty cells; too large
	 * and every query drags in the whole level again. 1000 cm suits ranges in the 2000-6000 cm band,
	 * which is where turrets normally live.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Spatial Index", meta = (ClampMin = "50.0", UIMax = "20000.0"))
	float CellSize = 1000.0f;

	/**
	 * Fraction of the target index refreshed per frame, 0..1. 0.25 means every target's position,
	 * velocity and grid cell is brought up to date once every four frames.
	 *
	 * Targets move, so the index has to be maintained — but not all of it, and not all at once. Between
	 * refreshes a target's position is extrapolated from its last sample and measured velocity, so
	 * turrets keep tracking smoothly on the frames the index skipped them. Push it to 1.0 for perfect
	 * freshness at four times the maintenance cost.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Spatial Index", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float IndexSliceFraction = 0.25f;

	/**
	 * How hard a newly measured velocity pulls the stored one, 0..1. 1 means "believe every sample",
	 * which turns a single jittery frame into a wild lead point; 0.5 smooths that out without adding
	 * noticeable lag at a quarter-second sample interval.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Spatial Index", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VelocitySmoothing = 0.5f;

	// ----------------------------------------------------------------------------------------------
	// Budgets
	// ----------------------------------------------------------------------------------------------

	/**
	 * Hard cap on turrets re-evaluated in one frame.
	 *
	 * A cursor walks the turret list; whoever does not get a turn keeps the target it already has and is
	 * served on a later frame. Nothing stalls, nothing stands still — the reaction time stretches, which
	 * is exactly the trade a hundred-turret level wants to make.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Budget", meta = (ClampMin = "0", UIMax = "256"))
	int32 MaxAcquisitionsPerFrame = 16;

	/**
	 * Hard cap on line-of-sight traces fired in one frame, counted separately from the acquisition
	 * budget because a trace costs an order of magnitude more than a distance comparison. A turret that
	 * runs into an empty trace budget mid-evaluation keeps its target and finishes next frame.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Budget", meta = (ClampMin = "0", UIMax = "256"))
	int32 MaxLineTracesPerFrame = 8;

	/**
	 * Seconds a line-of-sight answer stays good for one turret/target pair. Below this the cached
	 * verdict is reused and no trace is fired. 0.3 s is roughly the time it takes for a walking target
	 * to matter, and it is what makes a hundred turrets affordable at eight traces a frame.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Budget", meta = (ClampMin = "0.0", UIMax = "5.0"))
	float LosCacheSeconds = 0.3f;

	// ----------------------------------------------------------------------------------------------
	// Defaults
	// ----------------------------------------------------------------------------------------------

	/**
	 * Profile used by a turret component that has none of its own. Leave it empty and the built-in
	 * profile named below is used, so the plugin works before any asset exists.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Defaults", meta = (AllowedClasses = "/Script/TurretMind.TurretMindProfile"))
	TSoftObjectPtr<UTurretMindProfile> DefaultProfile;

	/** Which built-in profile to fall back on: Machinegun, Cannon or Missile. */
	UPROPERTY(Config, EditAnywhere, Category = "Defaults")
	FName DefaultBuiltinProfile = FName(TEXT("Machinegun"));

	// ----------------------------------------------------------------------------------------------
	// Debug
	// ----------------------------------------------------------------------------------------------

	/** Show the statistics box as soon as a world's service comes up. Same as TurretMind.Stats 1. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bShowStatsByDefault = false;

	/** Draw the world-space debug lines from the start. Same as TurretMind.Draw 1. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bDrawDebugByDefault = false;

	/** Include the range circles in the world-space debug draw. Same as TurretMind.DrawRanges 1. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bDrawRangesByDefault = true;

	/**
	 * At most this many turrets are drawn by the world-space debug draw. Debug lines are not budgeted
	 * the way acquisition is, and two hundred range circles will cost more than the thing they measure.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Debug", meta = (ClampMin = "1", UIMax = "512"))
	int32 MaxDebugDrawTurrets = 64;

	/**
	 * Draw the statistics box in an editor viewport through the debug draw service as well as through
	 * AHUD. The AHUD path is the one that ships; this one is what makes a screenshot inside the editor
	 * possible. Compiled out entirely outside the editor.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bEnableEditorViewportStats = true;
};
