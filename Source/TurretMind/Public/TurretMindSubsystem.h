// Copyright 2026 Simulated Flow. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "TurretMindTypes.h"
#include "UObject/ObjectKey.h"
#include "TurretMindSubsystem.generated.h"

class AActor;
class APlayerController;
class UCanvas;
class UTurretMindComponent;
class UTurretMindProfile;
class UTurretMindTargetComponent;

/**
 * The service. One per world, ticks itself, owns every turret and every target in that world.
 *
 * The naive turret asks GetAllActorsOfClass, sorts the result and pulls a trace — per turret, per
 * frame. Forty turrets and two hundred attackers is eight thousand distance checks and forty traces
 * every frame, and it is exactly where tower defence prototypes fall over once the wave gets big.
 *
 * TurretMind replaces all of it with four things:
 *
 *   1. A spatial index. Targets live in a uniform grid over the XY plane, not in a list. A turret with
 *      4000 cm of range and a 1000 cm cell size opens about eighty-one cells, most of them empty, and
 *      looks at the handful of targets inside them. The eight thousand checks become dozens — and
 *      Targets Considered in the statistics box is that number, on screen, per frame.
 *   2. A staggered index refresh. Targets move, so the grid has to be maintained, but only a slice of
 *      it per frame (IndexSliceFraction, a quarter by default). Between refreshes a target's position
 *      is extrapolated from its last sample and its measured velocity.
 *   3. A round-robin re-evaluation under a hard budget. A cursor walks the turret list; at most
 *      MaxAcquisitionsPerFrame turrets get to think again, and traces are rationed separately on top of
 *      that. A turret that does not get a turn keeps the target it already has. Nothing stalls.
 *   4. Hysteresis. A new target has to beat the held one by TargetStickiness before the turret will
 *      switch, so two equally good attackers do not make the barrel flick back and forth.
 *
 * Aiming is not budgeted, and that distinction is the whole trick: every turret's aim point is
 * recomputed every frame from the last known target position, which costs a subtraction and a
 * normalise. Choosing what to shoot is what is expensive, and choosing is what gets rationed. Drop the
 * budget to four and the turrets keep tracking smoothly — they just change their minds less often.
 *
 * The service does not shoot. It answers "what" and "where to lead", and stops there.
 */
UCLASS()
class TURRETMIND_API UTurretMindSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	// ----------------------------------------------------------------------------------------------
	// Registration — normally done for you by the two components
	// ----------------------------------------------------------------------------------------------

	/** Put a turret into the round-robin. Called by UTurretMindComponent::BeginPlay. */
	void RegisterTurret(UTurretMindComponent* Turret);

	/** Take a turret out again. Safe to call twice, safe to call during shutdown. */
	void UnregisterTurret(UTurretMindComponent* Turret);

	/** Put a target into the spatial index. Called by UTurretMindTargetComponent::BeginPlay. */
	void RegisterTarget(UTurretMindTargetComponent* Target);

	/** Take a target out of the index. Turrets holding it drop it on their next aim pass. */
	void UnregisterTarget(UTurretMindTargetComponent* Target);

	/** Bring one target's row of the index up to date now instead of waiting for its slice. */
	void RefreshTargetNow(UTurretMindTargetComponent* Target);

	/**
	 * Make one turret due for re-evaluation immediately, without losing the target it holds. It still
	 * has to wait for the cursor and still costs one slot of the frame budget.
	 */
	void MakeTurretDue(UTurretMindComponent* Turret);

	/** Drop one turret's target without picking a new one. Fires On Target Lost. */
	void ClearTurretTarget(UTurretMindComponent* Turret);

	// ----------------------------------------------------------------------------------------------
	// Queries
	// ----------------------------------------------------------------------------------------------

	/** What the service did on the most recent frame. */
	UFUNCTION(BlueprintPure, Category = "TurretMind")
	const FTurretMindStats& GetStats() const { return Stats; }

	/**
	 * One-off query with nothing registered on the asking side: score every target inside Profile's
	 * range around Origin and hand back the best one plus the point to lead to.
	 *
	 * This is the escape hatch for things that are not turrets — a homing missile picking a new target
	 * mid-flight, a UI that wants to show what a turret would pick if it were built here. It is not
	 * budgeted and not cached: it runs the full scoring pass, line-of-sight traces included, right now.
	 * Do not call it from a per-frame loop over a hundred actors; that is what the components are for.
	 */
	UFUNCTION(BlueprintCallable, Category = "TurretMind", meta = (AdvancedDisplay = "5"))
	bool FindBestTarget(FVector Origin, FVector Forward, UTurretMindProfile* Profile, int32 TeamId, AActor*& OutTarget, FVector& OutAimPoint, AActor* IgnoreActor = nullptr);

	/**
	 * Solve the intercept point: where to aim so a projectile of this speed and a target at this
	 * position and velocity arrive together.
	 *
	 * Solved by iteration, three passes, not by the closed-form quadratic. The quadratic is exact for
	 * the assumption it makes — constant velocity forever — and that assumption is wrong by the next
	 * frame, because the target turns, is pushed, or stops. Three iterations land within a few
	 * centimetres of the exact answer for any sane speed ratio, cost three square roots, and cannot
	 * produce the negative discriminant the closed form has to handle. Re-solved every frame anyway.
	 *
	 * Projectile Speed at or below zero means hitscan: the answer is the target's position.
	 */
	UFUNCTION(BlueprintPure, Category = "TurretMind")
	static FVector PredictAimPoint(FVector MuzzleLocation, FVector TargetLocation, FVector TargetVelocity, float ProjectileSpeed, float MaxLeadSeconds = 3.0f);

	// ----------------------------------------------------------------------------------------------
	// Budgets and overrides
	// ----------------------------------------------------------------------------------------------

	/** Change the hard cap on turret re-evaluations per frame. Takes effect next frame. */
	UFUNCTION(BlueprintCallable, Category = "TurretMind")
	void SetAcquisitionBudget(int32 InMaxAcquisitionsPerFrame);

	/** Current per-frame acquisition budget. */
	UFUNCTION(BlueprintPure, Category = "TurretMind")
	int32 GetAcquisitionBudget() const { return MaxAcquisitionsPerFrame; }

	/** Change the hard cap on line-of-sight traces per frame. Takes effect next frame. */
	UFUNCTION(BlueprintCallable, Category = "TurretMind")
	void SetTraceBudget(int32 InMaxLineTracesPerFrame);

	/** Current per-frame trace budget. */
	UFUNCTION(BlueprintPure, Category = "TurretMind")
	int32 GetTraceBudget() const { return MaxLineTracesPerFrame; }

	/**
	 * Force every turret in this world onto one policy, whatever their profiles say. Meant for tuning
	 * and for demo buttons; clear it with Clear Policy Override when you are done.
	 */
	UFUNCTION(BlueprintCallable, Category = "TurretMind")
	void SetPolicyOverride(ETurretMindPolicy InPolicy);

	/** Hand the policy decision back to each turret's profile. */
	UFUNCTION(BlueprintCallable, Category = "TurretMind")
	void ClearPolicyOverride();

	/** True while a policy override is in force. */
	UFUNCTION(BlueprintPure, Category = "TurretMind")
	bool HasPolicyOverride() const { return bHasPolicyOverride; }

	/** The policy currently forced on every turret. Meaningless while Has Policy Override is false. */
	UFUNCTION(BlueprintPure, Category = "TurretMind")
	ETurretMindPolicy GetPolicyOverride() const { return PolicyOverride; }

	/** Force line of sight on or off for every turret, whatever their profiles say. */
	UFUNCTION(BlueprintCallable, Category = "TurretMind")
	void SetLineOfSightOverride(bool bRequireLineOfSight);

	/** Hand the line-of-sight decision back to each turret's profile. */
	UFUNCTION(BlueprintCallable, Category = "TurretMind")
	void ClearLineOfSightOverride();

	/** True while a line-of-sight override is in force. */
	UFUNCTION(BlueprintPure, Category = "TurretMind")
	bool HasLineOfSightOverride() const { return bHasLosOverride; }

	// ----------------------------------------------------------------------------------------------
	// Display
	// ----------------------------------------------------------------------------------------------

	/**
	 * Draw the statistics box onto a canvas. This is the whole HUD renderer.
	 *
	 * Call it from your own HUD class: Service->DrawStats(Canvas); — one line, in DrawHUD.
	 * Or use ATurretMindHUD / UTurretMindHUDComponent, which do exactly that for you.
	 */
	UFUNCTION(BlueprintCallable, Category = "TurretMind|Debug")
	void DrawStats(UCanvas* Canvas);

	/** Show or hide the on-screen statistics box. Same as TurretMind.Stats. */
	UFUNCTION(BlueprintCallable, Category = "TurretMind|Debug")
	void SetShowStats(bool bInShowStats) { bShowStats = bInShowStats; }

	/** True while the statistics box is being drawn. */
	UFUNCTION(BlueprintPure, Category = "TurretMind|Debug")
	bool IsShowingStats() const { return bShowStats; }

	/** Master switch for the world-space debug lines. Same as TurretMind.Draw. */
	UFUNCTION(BlueprintCallable, Category = "TurretMind|Debug")
	void SetDrawDebug(bool bInDrawDebug) { bDrawDebug = bInDrawDebug; }

	/** True while the world-space debug lines are being drawn. */
	UFUNCTION(BlueprintPure, Category = "TurretMind|Debug")
	bool IsDrawingDebug() const { return bDrawDebug; }

	/** Include the range circles in the world-space debug draw. Same as TurretMind.DrawRanges. */
	UFUNCTION(BlueprintCallable, Category = "TurretMind|Debug")
	void SetDrawRanges(bool bInDrawRanges) { bDrawRanges = bInDrawRanges; }

	/** True while range circles are part of the debug draw. */
	UFUNCTION(BlueprintPure, Category = "TurretMind|Debug")
	bool IsDrawingRanges() const { return bDrawRanges; }

	/** One of the three built-in profiles: Machinegun, Cannon, Missile. Never returns null. */
	UFUNCTION(BlueprintPure, Category = "TurretMind")
	UTurretMindProfile* GetBuiltinProfile(FName ProfileName);

	/** The profile a turret component with an empty Profile ends up using. Never returns null. */
	UFUNCTION(BlueprintPure, Category = "TurretMind")
	UTurretMindProfile* GetDefaultProfile();

	/** Grid cell size in cm, as read from the project settings when this world came up. */
	UFUNCTION(BlueprintPure, Category = "TurretMind")
	float GetCellSize() const { return CellSize; }

private:
	/** One row of the spatial index: a flat snapshot of a target, refreshed a slice at a time. */
	struct FTargetEntry
	{
		TWeakObjectPtr<UTurretMindTargetComponent> Component;
		TWeakObjectPtr<AActor> Actor;

		/** Last sampled actor location. Distance and cell membership are computed from this. */
		FVector Location = FVector::ZeroVector;

		/** Last sampled aim location: actor transform applied to the target's Aim Offset. */
		FVector AimLocation = FVector::ZeroVector;

		/** Smoothed velocity in cm/s, measured between index samples unless the owner supplies one. */
		FVector Velocity = FVector::ZeroVector;

		/** World time Location was sampled, so a stale row can be extrapolated forward. */
		double SampleTime = 0.0;

		/** World time this target was registered, for the First Seen policy. */
		double RegisterTime = 0.0;

		float Threat = 0.0f;
		float Health = 1.0f;
		float PathProgress = 0.0f;
		float Radius = 50.0f;
		int32 TeamId = 0;

		/** Which grid cell this row currently sits in. */
		FIntPoint Cell = FIntPoint::ZeroValue;

		/** Generation of this slot, bumped every time the slot is handed out again. */
		uint32 Serial = 0;

		bool bTargetable = true;
		bool bUseOwnerVelocity = false;

		/** False means this slot is on the free list and its contents are meaningless. */
		bool bAlive = false;
	};

	/** One turret in the round-robin. */
	struct FTurretEntry
	{
		TWeakObjectPtr<UTurretMindComponent> Component;

		/** Slot of the held target in Targets, or INDEX_NONE. */
		int32 TargetSlot = INDEX_NONE;

		/** Generation of that slot when it was picked up. A mismatch means the target is long gone. */
		uint32 TargetSerial = 0;

		/** Fitness the held target scored at the last re-evaluation. The other half of the hysteresis. */
		float TargetScore = 0.0f;

		/** World time this turret last completed a re-evaluation. */
		double LastEvaluateTime = 0.0;

		/** Generation of this slot. */
		uint32 Serial = 0;

		bool bAlive = false;
	};

	/** A cached line-of-sight verdict for one turret/target pair. */
	struct FLosEntry
	{
		double Time = 0.0;
		uint32 TargetSerial = 0;
		bool bVisible = false;
	};

	/** A target that survived range, team and cone filtering, with its fitness. */
	struct FCandidate
	{
		int32 Slot = INDEX_NONE;
		float Score = 0.0f;
		FVector AimLocation = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float Radius = 50.0f;
	};

	/** Read the project settings into the runtime copies. Called once, on Initialize. */
	void ApplySettings();

	/** Build the three transient fallback profiles so the plugin works with no assets authored. */
	void CreateBuiltinProfiles();

	/** Grid coordinates of a world position. */
	FIntPoint CellOf(const FVector& Location) const;

	/** Put a row into its cell bucket. */
	void AddToCell(int32 Slot, FIntPoint Cell);

	/** Take a row out of its cell bucket, dropping the bucket when it empties. */
	void RemoveFromCell(int32 Slot, FIntPoint Cell);

	/** Retire a target row: out of its cell, off the lookup, onto the free list. */
	void ReleaseTargetSlot(int32 Slot);

	/** Retire a turret row. */
	void ReleaseTurretSlot(int32 Slot);

	/** Re-sample one target row from its component, moving it between cells if it left its own. */
	void RefreshTargetEntry(int32 Slot);

	/** Refresh one slice of the index this frame and advance the slice cursor. */
	void RefreshIndexSlice();

	/**
	 * The cheap pass: every turret, every frame, recompute the aim point for the target it already
	 * holds. Not budgeted, on purpose — see the class comment.
	 */
	void UpdateAimPoints();

	/** The budgeted pass: walk the cursor over the turrets and re-evaluate the ones that are due. */
	void RunAcquisitionRound();

	/** Re-evaluate one turret. Returns false when it ran out of trace budget and should retry. */
	bool EvaluateTurret(int32 TurretSlot);

	/** Collect and score every target inside range around a muzzle. The spatial query lives here. */
	void GatherCandidates(
		const FTurretMindTurretInfo& TurretInfo,
		const UTurretMindProfile& Profile,
		ETurretMindPolicy Policy,
		const AActor* IgnoreActor,
		const FTurretMindScoreDelegate* CustomScorer,
		TArray<FCandidate>& OutCandidates,
		int32 CurrentTargetSlot,
		float& OutCurrentScore);

	/** Turn one index row into the 0..1 fitness the policy asks for. Higher is always better. */
	float ScoreTarget(
		const FTurretMindTurretInfo& TurretInfo,
		const UTurretMindProfile& Profile,
		ETurretMindPolicy Policy,
		const FTargetEntry& Entry,
		float Distance,
		const FTurretMindScoreDelegate* CustomScorer) const;

	/** Fill a Blueprint-facing snapshot from an index row. */
	void MakeTargetInfo(const FTargetEntry& Entry, float Distance, FTurretMindTargetInfo& OutInfo) const;

	/**
	 * Cached, budgeted line-of-sight test. Serves the cache when it is fresh, fires a trace when there
	 * is budget left, and otherwise reports "do not know" through bOutResolved so the caller can leave
	 * the turret alone until next frame.
	 */
	bool ResolveLineOfSight(int32 TurretSlot, int32 TargetSlot, const FVector& From, const FVector& To,
		ECollisionChannel Channel, const AActor* IgnoreTurret, const AActor* IgnoreTarget,
		int32& InOutTraceBudget, bool& bOutResolved);

	/**
	 * Read the last cached line-of-sight verdict for a pair without firing a trace and without caring
	 * how old it is. Used for the target a turret keeps out of stickiness, which the sorted walk broke
	 * out of before testing. Unknown pairs come back visible: it was visible when it was picked.
	 */
	bool PeekLineOfSight(int32 TurretSlot, int32 TargetSlot) const;

	/** Drop line-of-sight entries nobody will ask about again. Runs on a slow timer, not every frame. */
	void PruneLosCache();

	/** Extrapolate a row's aim position forward from its last sample. */
	FVector PredictedAimLocation(const FTargetEntry& Entry, double Now) const;

	/** Is this slot still the target the turret picked up, and still shootable? */
	bool IsTargetSlotValid(int32 Slot, uint32 Serial) const;

	/** Push the picked target, aim point and line-of-sight verdict into a turret component. */
	void ApplyTargetToComponent(FTurretEntry& Turret, int32 NewSlot, bool bHasLos, double Now);

	/**
	 * Range circles, muzzle-to-aim lines and the lead cross. Console: TurretMind.Draw 1.
	 *
	 * Declared unconditionally and emptied out by ENABLE_DRAW_DEBUG in the .cpp instead of being
	 * compiled away here: that macro lives behind an Engine header this one deliberately does not pull
	 * in, and a member function that exists in some translation units and not others is a trap.
	 */
	void DrawWorldDebug();

#if WITH_EDITOR
	/**
	 * Second attachment point for the statistics box, editor only.
	 *
	 * The path that ships is AHUD::DrawHUD (see ATurretMindHUD and UTurretMindHUDComponent), because
	 * UDebugDrawService is compiled out of a cooked build entirely. This one exists so the numbers show
	 * up in a play-in-editor viewport without anyone having to set a HUD class first — which is where
	 * the store screenshots come from, and what cannot be advertised if it does not draw.
	 */
	void OnEditorViewportDraw(UCanvas* Canvas, APlayerController* PlayerController);
#endif

	/** The spatial index rows. Slots are reused through FreeTargetSlots; the array never shrinks. */
	TArray<FTargetEntry> Targets;
	TArray<int32> FreeTargetSlots;
	int32 LiveTargetCount = 0;

	/** The turret rows. */
	TArray<FTurretEntry> Turrets;
	TArray<int32> FreeTurretSlots;
	int32 LiveTurretCount = 0;

	/**
	 * The grid. Cell coordinate to the target slots inside it.
	 *
	 * This is the reason the plugin exists. Forty turrets asking two hundred targets "how far away are
	 * you" is eight thousand distance checks every frame before a single shot is fired, and that is
	 * before the traces. With the grid, a turret opens the cells its range actually covers — about
	 * eighty-one of them at 4000 cm range and 1000 cm cells, nearly all empty — and looks at the dozen
	 * or so targets it finds there. Targets Considered in the statistics box is that number.
	 */
	TMap<FIntPoint, TArray<int32>> Cells;

	/** Component to slot, so registering and unregistering stay O(1) at any target count. */
	TMap<TObjectKey<UTurretMindTargetComponent>, int32> TargetLookup;
	TMap<TObjectKey<UTurretMindComponent>, int32> TurretLookup;

	/**
	 * Line-of-sight verdicts, keyed by turret slot in the high word and target slot in the low word.
	 * The stored target serial catches the case where the slot was recycled for a different target.
	 */
	TMap<uint64, FLosEntry> LosCache;

	/** Scratch space for the scoring pass. Reserved once, then only ever reset. */
	TArray<FCandidate> CandidateScratch;

	/** Rolling cursor over Targets for the staggered index refresh. */
	int32 IndexCursor = 0;

	/** Rolling cursor over Turrets for the round-robin re-evaluation. */
	int32 TurretCursor = 0;

	/** Generation counter handed to the next allocated slot. Wraps harmlessly. */
	uint32 NextSerial = 1;

	/** Runtime copies of the project settings. */
	float CellSize = 1000.0f;
	float InvCellSize = 0.001f;
	float IndexSliceFraction = 0.25f;
	float VelocitySmoothing = 0.5f;
	int32 MaxAcquisitionsPerFrame = 16;
	int32 MaxLineTracesPerFrame = 8;
	float LosCacheSeconds = 0.3f;
	int32 MaxDebugDrawTurrets = 64;

	/** Traces left in this frame's ration. Reset at the top of Tick, spent by ResolveLineOfSight. */
	int32 TraceBudgetRemaining = 0;

	/** Policy forced on every turret, ignoring their profiles. */
	ETurretMindPolicy PolicyOverride = ETurretMindPolicy::Nearest;
	bool bHasPolicyOverride = false;

	/** Line-of-sight requirement forced on every turret, ignoring their profiles. */
	bool bLosOverrideValue = true;
	bool bHasLosOverride = false;

	/** Display switches. */
	bool bShowStats = false;
	bool bDrawDebug = false;
	bool bDrawRanges = true;

	/** World time of the last line-of-sight cache prune. */
	double LastLosPruneTime = 0.0;

	/** Frame the last real HUD pass drew on, so the editor path never draws a second box on top. */
	uint64 LastHudDrawFrame = 0;

	/** Handle for the editor-only statistics draw. */
	FDelegateHandle EditorDrawHandle;

	/** The three transient fallback profiles, by name. */
	UPROPERTY()
	TMap<FName, TObjectPtr<UTurretMindProfile>> BuiltinProfiles;

	/** The project's default profile asset once it has been resolved, or null if there is none. */
	UPROPERTY()
	TObjectPtr<UTurretMindProfile> ResolvedDefaultProfile;

	/** Published through GetStats(). */
	FTurretMindStats Stats;
};
