// Copyright 2026 Simulated Flow. All Rights Reserved.

#include "TurretMindSubsystem.h"

#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "CollisionQueryParams.h"
#include "Debug/DebugDrawService.h"
#include "DrawDebugHelpers.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "GlobalRenderResources.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/StringBuilder.h"
#include "SceneTypes.h"
#include "Stats/Stats.h"
#include "TurretMindComponent.h"
#include "TurretMindLog.h"
#include "TurretMindProfile.h"
#include "TurretMindSettings.h"
#include "TurretMindTargetComponent.h"
#include "UObject/UObjectGlobals.h"

DECLARE_STATS_GROUP(TEXT("TurretMind"), STATGROUP_TurretMind, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("TurretMind Acquire"), STAT_TurretMindAcquire, STATGROUP_TurretMind);
DECLARE_CYCLE_STAT(TEXT("TurretMind Index"), STAT_TurretMindIndex, STATGROUP_TurretMind);
DECLARE_DWORD_COUNTER_STAT(TEXT("Targets Considered"), STAT_TurretMindConsidered, STATGROUP_TurretMind);
DECLARE_DWORD_COUNTER_STAT(TEXT("Line Traces"), STAT_TurretMindTraces, STATGROUP_TurretMind);

namespace TurretMindNames
{
	static const FName Machinegun(TEXT("Machinegun"));
	static const FName Cannon(TEXT("Cannon"));
	static const FName Missile(TEXT("Missile"));
}

// -------------------------------------------------------------------------------------------------------
// Console commands
// -------------------------------------------------------------------------------------------------------

namespace TurretMindConsole
{
	/** Apply a lambda to the service of every world the command could plausibly mean. */
	template <typename FuncType>
	static void ForEachService(UWorld* World, FuncType Func)
	{
		TArray<UTurretMindSubsystem*> Found;

		if (World)
		{
			if (UTurretMindSubsystem* Service = World->GetSubsystem<UTurretMindSubsystem>())
			{
				Found.Add(Service);
			}
		}

		if (Found.Num() == 0 && GEngine)
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.World())
				{
					if (UTurretMindSubsystem* Service = Context.World()->GetSubsystem<UTurretMindSubsystem>())
					{
						Found.AddUnique(Service);
					}
				}
			}
		}

		for (UTurretMindSubsystem* Service : Found)
		{
			Func(*Service);
		}
	}

	static bool ParseBool(const TArray<FString>& Args, bool bDefault)
	{
		if (Args.Num() == 0)
		{
			return bDefault;
		}
		return Args[0].ToBool() || Args[0] == TEXT("1");
	}

	static FAutoConsoleCommandWithWorldAndArgs GStats(
		TEXT("TurretMind.Stats"),
		TEXT("TurretMind.Stats 0|1 - show the on-screen acquisition statistics box."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			const bool bShow = ParseBool(Args, true);
			ForEachService(World, [bShow](UTurretMindSubsystem& Service) { Service.SetShowStats(bShow); });
		}));

	static FAutoConsoleCommandWithWorldAndArgs GDraw(
		TEXT("TurretMind.Draw"),
		TEXT("TurretMind.Draw 0|1 - world-space debug lines: range circles, muzzle-to-target lines, lead crosses."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			const bool bDraw = ParseBool(Args, true);
			ForEachService(World, [bDraw](UTurretMindSubsystem& Service) { Service.SetDrawDebug(bDraw); });
		}));

	static FAutoConsoleCommandWithWorldAndArgs GDrawRanges(
		TEXT("TurretMind.DrawRanges"),
		TEXT("TurretMind.DrawRanges 0|1 - include the range circles in the debug draw."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			const bool bDraw = ParseBool(Args, true);
			ForEachService(World, [bDraw](UTurretMindSubsystem& Service) { Service.SetDrawRanges(bDraw); });
		}));

	static FAutoConsoleCommandWithWorldAndArgs GBudget(
		TEXT("TurretMind.Budget"),
		TEXT("TurretMind.Budget <n> - hard cap on turret re-evaluations per frame."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() == 0)
			{
				ForEachService(World, [](UTurretMindSubsystem& Service)
				{
					UE_LOG(LogTurretMind, Display, TEXT("TurretMind.Budget = %d"), Service.GetAcquisitionBudget());
				});
				return;
			}
			const int32 Budget = FCString::Atoi(*Args[0]);
			ForEachService(World, [Budget](UTurretMindSubsystem& Service) { Service.SetAcquisitionBudget(Budget); });
		}));

	static FAutoConsoleCommandWithWorldAndArgs GTraceBudget(
		TEXT("TurretMind.TraceBudget"),
		TEXT("TurretMind.TraceBudget <n> - hard cap on line-of-sight traces per frame."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() == 0)
			{
				ForEachService(World, [](UTurretMindSubsystem& Service)
				{
					UE_LOG(LogTurretMind, Display, TEXT("TurretMind.TraceBudget = %d"), Service.GetTraceBudget());
				});
				return;
			}
			const int32 Budget = FCString::Atoi(*Args[0]);
			ForEachService(World, [Budget](UTurretMindSubsystem& Service) { Service.SetTraceBudget(Budget); });
		}));

	static FAutoConsoleCommandWithWorldAndArgs GPolicy(
		TEXT("TurretMind.Policy"),
		TEXT("TurretMind.Policy <Nearest|LowestHealth|HighestThreat|FurthestAlongPath|FirstSeen|Custom|Clear> - force one policy on every turret."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() == 0 || Args[0].Equals(TEXT("Clear"), ESearchCase::IgnoreCase)
				|| Args[0].Equals(TEXT("None"), ESearchCase::IgnoreCase)
				|| Args[0].Equals(TEXT("Off"), ESearchCase::IgnoreCase))
			{
				ForEachService(World, [](UTurretMindSubsystem& Service) { Service.ClearPolicyOverride(); });
				UE_LOG(LogTurretMind, Display, TEXT("TurretMind.Policy cleared - every turret is back on its own profile."));
				return;
			}

			const UEnum* PolicyEnum = StaticEnum<ETurretMindPolicy>();
			const int64 Value = PolicyEnum ? PolicyEnum->GetValueByNameString(Args[0]) : INDEX_NONE;
			if (Value == INDEX_NONE)
			{
				UE_LOG(LogTurretMind, Warning, TEXT("TurretMind.Policy: unknown policy '%s'."), *Args[0]);
				return;
			}

			const ETurretMindPolicy Policy = static_cast<ETurretMindPolicy>(Value);
			ForEachService(World, [Policy](UTurretMindSubsystem& Service) { Service.SetPolicyOverride(Policy); });
		}));

	static FAutoConsoleCommandWithWorldAndArgs GLineOfSight(
		TEXT("TurretMind.LineOfSight"),
		TEXT("TurretMind.LineOfSight 0|1|Clear - force the line-of-sight requirement on or off for every turret."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() == 0 || Args[0].Equals(TEXT("Clear"), ESearchCase::IgnoreCase))
			{
				ForEachService(World, [](UTurretMindSubsystem& Service) { Service.ClearLineOfSightOverride(); });
				return;
			}
			const bool bRequire = ParseBool(Args, true);
			ForEachService(World, [bRequire](UTurretMindSubsystem& Service) { Service.SetLineOfSightOverride(bRequire); });
		}));
}

// -------------------------------------------------------------------------------------------------------
// Lifetime
// -------------------------------------------------------------------------------------------------------

void UTurretMindSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ApplySettings();
	CreateBuiltinProfiles();

	Targets.Reserve(256);
	Turrets.Reserve(64);
	CandidateScratch.Reserve(128);

#if WITH_EDITOR
	if (UTurretMindSettings::Get().bEnableEditorViewportStats)
	{
		EditorDrawHandle = UDebugDrawService::Register(
			TEXT("Game"),
			FDebugDrawDelegate::CreateUObject(this, &UTurretMindSubsystem::OnEditorViewportDraw));
	}
#endif
}

void UTurretMindSubsystem::Deinitialize()
{
#if WITH_EDITOR
	if (EditorDrawHandle.IsValid())
	{
		UDebugDrawService::Unregister(EditorDrawHandle);
		EditorDrawHandle.Reset();
	}
#endif

	Targets.Reset();
	FreeTargetSlots.Reset();
	Turrets.Reset();
	FreeTurretSlots.Reset();
	Cells.Reset();
	TargetLookup.Reset();
	TurretLookup.Reset();
	LosCache.Reset();
	CandidateScratch.Reset();
	BuiltinProfiles.Reset();
	ResolvedDefaultProfile = nullptr;
	LiveTargetCount = 0;
	LiveTurretCount = 0;

	Super::Deinitialize();
}

bool UTurretMindSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// Game and PIE only. Nothing registers in an editor world, because neither component has run
	// BeginPlay there — a service with no turrets and no targets is not worth ticking.
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UTurretMindSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTurretMindSubsystem, STATGROUP_Tickables);
}

void UTurretMindSubsystem::ApplySettings()
{
	const UTurretMindSettings& Settings = UTurretMindSettings::Get();

	CellSize = FMath::Max(50.0f, Settings.CellSize);
	InvCellSize = 1.0f / CellSize;
	IndexSliceFraction = FMath::Clamp(Settings.IndexSliceFraction, 0.01f, 1.0f);
	VelocitySmoothing = FMath::Clamp(Settings.VelocitySmoothing, 0.0f, 1.0f);
	MaxAcquisitionsPerFrame = FMath::Max(0, Settings.MaxAcquisitionsPerFrame);
	MaxLineTracesPerFrame = FMath::Max(0, Settings.MaxLineTracesPerFrame);
	LosCacheSeconds = FMath::Max(0.0f, Settings.LosCacheSeconds);
	MaxDebugDrawTurrets = FMath::Max(1, Settings.MaxDebugDrawTurrets);

	bShowStats = Settings.bShowStatsByDefault;
	bDrawDebug = Settings.bDrawDebugByDefault;
	bDrawRanges = Settings.bDrawRangesByDefault;
}

void UTurretMindSubsystem::CreateBuiltinProfiles()
{
	// Three transient profiles, created in code so that dropping a TurretMind Turret component onto an
	// actor already does something sensible. The same three exist as editable assets under
	// Content/TurretMind/Profiles; these are the floor, not the recommendation.
	auto MakeProfile = [this](FName Name, float Range, float MinRange, ETurretMindPolicy Policy,
		float Speed, bool bLead, float Stickiness, float Interval, float Threat)
	{
		UTurretMindProfile* Profile = NewObject<UTurretMindProfile>(this, UTurretMindProfile::StaticClass(), Name, RF_Transient);
		Profile->MaxRange = Range;
		Profile->MinRange = MinRange;
		Profile->Policy = Policy;
		Profile->ProjectileSpeed = Speed;
		Profile->bPredictLead = bLead;
		Profile->TargetStickiness = Stickiness;
		Profile->ReacquireInterval = Interval;
		Profile->ThreatScale = Threat;
		BuiltinProfiles.Add(Name, Profile);
	};

	// Long reach, slow shell, leads hard, thinks rarely and does not like changing its mind.
	MakeProfile(TurretMindNames::Cannon, 7000.0f, 600.0f, ETurretMindPolicy::FurthestAlongPath, 6000.0f, true, 0.35f, 0.6f, 100.0f);

	// Short reach, hitscan, shoots whatever is closest, re-checks often.
	MakeProfile(TurretMindNames::Machinegun, 2500.0f, 0.0f, ETurretMindPolicy::Nearest, 0.0f, false, 0.2f, 0.2f, 100.0f);

	// Long reach, slow missile, goes for whatever is most dangerous.
	MakeProfile(TurretMindNames::Missile, 9000.0f, 800.0f, ETurretMindPolicy::HighestThreat, 3500.0f, true, 0.4f, 0.8f, 100.0f);
}

UTurretMindProfile* UTurretMindSubsystem::GetBuiltinProfile(FName ProfileName)
{
	if (const TObjectPtr<UTurretMindProfile>* Found = BuiltinProfiles.Find(ProfileName))
	{
		return *Found;
	}
	if (const TObjectPtr<UTurretMindProfile>* Fallback = BuiltinProfiles.Find(TurretMindNames::Machinegun))
	{
		return *Fallback;
	}
	return nullptr;
}

UTurretMindProfile* UTurretMindSubsystem::GetDefaultProfile()
{
	if (ResolvedDefaultProfile)
	{
		return ResolvedDefaultProfile;
	}

	const UTurretMindSettings& Settings = UTurretMindSettings::Get();
	if (!Settings.DefaultProfile.IsNull())
	{
		// LoadSynchronous once, on the first turret that needs it. Profiles are a handful of floats.
		ResolvedDefaultProfile = Settings.DefaultProfile.LoadSynchronous();
		if (ResolvedDefaultProfile)
		{
			return ResolvedDefaultProfile;
		}
		UE_LOG(LogTurretMind, Warning, TEXT("Default profile '%s' could not be loaded; falling back to the built-in one."),
			*Settings.DefaultProfile.ToString());
	}

	ResolvedDefaultProfile = GetBuiltinProfile(Settings.DefaultBuiltinProfile);
	return ResolvedDefaultProfile;
}

// -------------------------------------------------------------------------------------------------------
// Registration
// -------------------------------------------------------------------------------------------------------

void UTurretMindSubsystem::RegisterTurret(UTurretMindComponent* Turret)
{
	if (!Turret || Turret->TurretSlot != INDEX_NONE)
	{
		return;
	}

	int32 Slot = INDEX_NONE;
	if (FreeTurretSlots.Num() > 0)
	{
		Slot = FreeTurretSlots.Pop(EAllowShrinking::No);
	}
	else
	{
		Slot = Turrets.AddDefaulted();
	}

	FTurretEntry& Entry = Turrets[Slot];
	Entry = FTurretEntry();
	Entry.Component = Turret;
	Entry.Serial = NextSerial++;
	Entry.bAlive = true;
	// Far enough in the past that this turret is due the moment the cursor reaches it.
	Entry.LastEvaluateTime = -1.0e9;

	Turret->TurretSlot = Slot;
	Turret->TurretSerial = Entry.Serial;
	TurretLookup.Add(TObjectKey<UTurretMindComponent>(Turret), Slot);
	++LiveTurretCount;
}

void UTurretMindSubsystem::UnregisterTurret(UTurretMindComponent* Turret)
{
	if (!Turret)
	{
		return;
	}

	const int32 Slot = Turret->TurretSlot;
	if (!Turrets.IsValidIndex(Slot) || Turrets[Slot].Serial != Turret->TurretSerial)
	{
		Turret->TurretSlot = INDEX_NONE;
		return;
	}

	TurretLookup.Remove(TObjectKey<UTurretMindComponent>(Turret));
	Turret->TurretSlot = INDEX_NONE;
	ReleaseTurretSlot(Slot);
}

void UTurretMindSubsystem::ReleaseTurretSlot(int32 Slot)
{
	if (!Turrets.IsValidIndex(Slot) || !Turrets[Slot].bAlive)
	{
		return;
	}

	Turrets[Slot] = FTurretEntry();
	FreeTurretSlots.Add(Slot);
	LiveTurretCount = FMath::Max(0, LiveTurretCount - 1);
}

void UTurretMindSubsystem::RegisterTarget(UTurretMindTargetComponent* Target)
{
	if (!Target || Target->IndexSlot != INDEX_NONE)
	{
		return;
	}

	AActor* Owner = Target->GetOwner();
	if (!Owner)
	{
		return;
	}

	int32 Slot = INDEX_NONE;
	if (FreeTargetSlots.Num() > 0)
	{
		Slot = FreeTargetSlots.Pop(EAllowShrinking::No);
	}
	else
	{
		Slot = Targets.AddDefaulted();
	}

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	FTargetEntry& Entry = Targets[Slot];
	Entry = FTargetEntry();
	Entry.Component = Target;
	Entry.Actor = Owner;
	Entry.Serial = NextSerial++;
	Entry.bAlive = true;
	Entry.RegisterTime = Now;
	Entry.SampleTime = Now;

	const FTransform OwnerTransform = Owner->GetActorTransform();
	Entry.Location = OwnerTransform.GetLocation();
	Entry.AimLocation = OwnerTransform.TransformPosition(Target->AimOffset);
	Entry.Velocity = Target->bUseOwnerVelocity ? Owner->GetVelocity() : FVector::ZeroVector;
	Entry.Threat = Target->Threat;
	Entry.Health = Target->Health;
	Entry.PathProgress = Target->PathProgress;
	Entry.Radius = Target->Radius;
	Entry.TeamId = Target->TeamId;
	Entry.bTargetable = Target->bTargetable;
	Entry.bUseOwnerVelocity = Target->bUseOwnerVelocity;
	Entry.Cell = CellOf(Entry.Location);

	AddToCell(Slot, Entry.Cell);

	Target->IndexSlot = Slot;
	Target->IndexSerial = Entry.Serial;
	Target->CachedVelocity = Entry.Velocity;
	TargetLookup.Add(TObjectKey<UTurretMindTargetComponent>(Target), Slot);
	++LiveTargetCount;
}

void UTurretMindSubsystem::UnregisterTarget(UTurretMindTargetComponent* Target)
{
	if (!Target)
	{
		return;
	}

	const int32 Slot = Target->IndexSlot;
	if (!Targets.IsValidIndex(Slot) || Targets[Slot].Serial != Target->IndexSerial)
	{
		Target->IndexSlot = INDEX_NONE;
		return;
	}

	TargetLookup.Remove(TObjectKey<UTurretMindTargetComponent>(Target));
	Target->IndexSlot = INDEX_NONE;
	ReleaseTargetSlot(Slot);
}

void UTurretMindSubsystem::ReleaseTargetSlot(int32 Slot)
{
	if (!Targets.IsValidIndex(Slot) || !Targets[Slot].bAlive)
	{
		return;
	}

	// Turrets pointing at this slot are not chased down here. They find out on their next aim pass,
	// where the stored serial no longer matches and the target is dropped cleanly. That is what makes
	// deleting a target mid-wave a non-event instead of a crash.
	RemoveFromCell(Slot, Targets[Slot].Cell);
	Targets[Slot] = FTargetEntry();
	FreeTargetSlots.Add(Slot);
	LiveTargetCount = FMath::Max(0, LiveTargetCount - 1);
}

void UTurretMindSubsystem::RefreshTargetNow(UTurretMindTargetComponent* Target)
{
	if (!Target)
	{
		return;
	}
	const int32 Slot = Target->IndexSlot;
	if (Targets.IsValidIndex(Slot) && Targets[Slot].Serial == Target->IndexSerial)
	{
		RefreshTargetEntry(Slot);
	}
}

void UTurretMindSubsystem::MakeTurretDue(UTurretMindComponent* Turret)
{
	if (!Turret)
	{
		return;
	}
	const int32 Slot = Turret->TurretSlot;
	if (Turrets.IsValidIndex(Slot) && Turrets[Slot].Serial == Turret->TurretSerial)
	{
		Turrets[Slot].LastEvaluateTime = -1.0e9;
	}
}

void UTurretMindSubsystem::ClearTurretTarget(UTurretMindComponent* Turret)
{
	if (!Turret)
	{
		return;
	}
	const int32 Slot = Turret->TurretSlot;
	if (Turrets.IsValidIndex(Slot) && Turrets[Slot].Serial == Turret->TurretSerial)
	{
		const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		ApplyTargetToComponent(Turrets[Slot], INDEX_NONE, false, Now);
	}
}

// -------------------------------------------------------------------------------------------------------
// The grid
// -------------------------------------------------------------------------------------------------------

FIntPoint UTurretMindSubsystem::CellOf(const FVector& Location) const
{
	return FIntPoint(
		FMath::FloorToInt(static_cast<float>(Location.X) * InvCellSize),
		FMath::FloorToInt(static_cast<float>(Location.Y) * InvCellSize));
}

void UTurretMindSubsystem::AddToCell(int32 Slot, FIntPoint Cell)
{
	TArray<int32>& Bucket = Cells.FindOrAdd(Cell);
	Bucket.Add(Slot);
}

void UTurretMindSubsystem::RemoveFromCell(int32 Slot, FIntPoint Cell)
{
	if (TArray<int32>* Bucket = Cells.Find(Cell))
	{
		Bucket->RemoveSingleSwap(Slot, EAllowShrinking::No);
		if (Bucket->Num() == 0)
		{
			// An empty bucket is a cell a query would still have to open and find nothing in. Drop it.
			Cells.Remove(Cell);
		}
	}
}

void UTurretMindSubsystem::RefreshTargetEntry(int32 Slot)
{
	if (!Targets.IsValidIndex(Slot))
	{
		return;
	}

	FTargetEntry& Entry = Targets[Slot];
	if (!Entry.bAlive)
	{
		return;
	}

	UTurretMindTargetComponent* Component = Entry.Component.Get();
	AActor* Owner = Component ? Component->GetOwner() : nullptr;
	if (!Component || !IsValid(Component) || !IsValid(Owner))
	{
		// The actor went away without EndPlay reaching us — level unload, forced destruction, a garbage
		// collection between frames. Retire the row here rather than dereferencing a corpse later.
		if (Component)
		{
			TargetLookup.Remove(TObjectKey<UTurretMindTargetComponent>(Component));
			Component->IndexSlot = INDEX_NONE;
		}
		ReleaseTargetSlot(Slot);
		return;
	}

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const FTransform OwnerTransform = Owner->GetActorTransform();
	const FVector NewLocation = OwnerTransform.GetLocation();

	if (Entry.bUseOwnerVelocity)
	{
		Entry.Velocity = Owner->GetVelocity();
	}
	else
	{
		// Measured, not read: a target is allowed to be an actor that simply sets its own transform,
		// and asking a movement component for that would hand back zero for ever.
		const double Elapsed = Now - Entry.SampleTime;
		if (Elapsed > UE_KINDA_SMALL_NUMBER)
		{
			const FVector Measured = (NewLocation - Entry.Location) / Elapsed;
			Entry.Velocity = FMath::Lerp(Entry.Velocity, Measured, VelocitySmoothing);
		}
	}

	Entry.Location = NewLocation;
	Entry.AimLocation = OwnerTransform.TransformPosition(Component->AimOffset);
	Entry.SampleTime = Now;
	Entry.Threat = Component->Threat;
	Entry.Health = Component->Health;
	Entry.PathProgress = Component->PathProgress;
	Entry.Radius = Component->Radius;
	Entry.TeamId = Component->TeamId;
	Entry.bTargetable = Component->bTargetable;
	Entry.bUseOwnerVelocity = Component->bUseOwnerVelocity;
	Component->CachedVelocity = Entry.Velocity;

	const FIntPoint NewCell = CellOf(NewLocation);
	if (NewCell != Entry.Cell)
	{
		RemoveFromCell(Slot, Entry.Cell);
		AddToCell(Slot, NewCell);
		Entry.Cell = NewCell;
	}
}

void UTurretMindSubsystem::RefreshIndexSlice()
{
	SCOPE_CYCLE_COUNTER(STAT_TurretMindIndex);

	const int32 Count = Targets.Num();
	if (Count == 0)
	{
		Stats.IndexRefreshedThisFrame = 0;
		return;
	}

	// A slice, not the lot. Everything the index misses this frame is covered by extrapolating the last
	// sample forward with the measured velocity, which is accurate enough for aiming and free.
	const int32 SliceSize = FMath::Clamp(FMath::CeilToInt(Count * IndexSliceFraction), 1, Count);
	IndexCursor = (Count > 0) ? (IndexCursor % Count) : 0;

	for (int32 Step = 0; Step < SliceSize; ++Step)
	{
		RefreshTargetEntry(IndexCursor);
		IndexCursor = (IndexCursor + 1) % Targets.Num();
	}

	Stats.IndexRefreshedThisFrame = SliceSize;
}

// -------------------------------------------------------------------------------------------------------
// Tick
// -------------------------------------------------------------------------------------------------------

void UTurretMindSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	Stats.AcquisitionsThisFrame = 0;
	Stats.TracesThisFrame = 0;
	Stats.CellsVisited = 0;
	Stats.TargetsConsidered = 0;
	Stats.SwitchesThisFrame = 0;
	Stats.LosCacheHitsThisFrame = 0;
	TraceBudgetRemaining = MaxLineTracesPerFrame;

	RefreshIndexSlice();

	// Cheap, unbudgeted, every turret: keep the barrels moving.
	UpdateAimPoints();

	{
		SCOPE_CYCLE_COUNTER(STAT_TurretMindAcquire);
		const double Start = FPlatformTime::Seconds();
		RunAcquisitionRound();
		Stats.AcquireMs = static_cast<float>((FPlatformTime::Seconds() - Start) * 1000.0);
	}

	PruneLosCache();

	Stats.Turrets = LiveTurretCount;
	Stats.Targets = LiveTargetCount;

	INC_DWORD_STAT_BY(STAT_TurretMindConsidered, Stats.TargetsConsidered);
	INC_DWORD_STAT_BY(STAT_TurretMindTraces, Stats.TracesThisFrame);

	if (bDrawDebug)
	{
		DrawWorldDebug();
	}
}

FVector UTurretMindSubsystem::PredictedAimLocation(const FTargetEntry& Entry, double Now) const
{
	const double Elapsed = FMath::Clamp(Now - Entry.SampleTime, 0.0, 1.0);
	return Entry.AimLocation + Entry.Velocity * Elapsed;
}

bool UTurretMindSubsystem::IsTargetSlotValid(int32 Slot, uint32 Serial) const
{
	if (!Targets.IsValidIndex(Slot))
	{
		return false;
	}
	const FTargetEntry& Entry = Targets[Slot];
	return Entry.bAlive && Entry.Serial == Serial && Entry.bTargetable && Entry.Actor.IsValid();
}

void UTurretMindSubsystem::UpdateAimPoints()
{
	UWorld* World = GetWorld();
	const double Now = World->GetTimeSeconds();

	int32 OnTarget = 0;

	for (int32 Slot = 0; Slot < Turrets.Num(); ++Slot)
	{
		if (!Turrets[Slot].bAlive)
		{
			continue;
		}

		UTurretMindComponent* Component = Turrets[Slot].Component.Get();
		if (!Component || !IsValid(Component))
		{
			ReleaseTurretSlot(Slot);
			continue;
		}

		if (!Component->bEnabled)
		{
			if (Turrets[Slot].TargetSlot != INDEX_NONE)
			{
				ApplyTargetToComponent(Turrets[Slot], INDEX_NONE, false, Now);
			}
			continue;
		}

		const int32 TargetSlot = Turrets[Slot].TargetSlot;
		if (TargetSlot == INDEX_NONE)
		{
			continue;
		}

		if (!IsTargetSlotValid(TargetSlot, Turrets[Slot].TargetSerial))
		{
			// Target died, was unregistered, or went untargetable. Drop it and let the next
			// re-evaluation find something else; the turret does not stall in the meantime.
			ApplyTargetToComponent(Turrets[Slot], INDEX_NONE, false, Now);
			continue;
		}

		const AActor* Owner = Component->GetOwner();
		if (!Owner)
		{
			continue;
		}

		const UTurretMindProfile* Profile = Component->GetEffectiveProfile();
		if (!Profile)
		{
			continue;
		}

		const FTargetEntry& Entry = Targets[TargetSlot];
		const FVector Muzzle = Owner->GetActorTransform().TransformPosition(Component->MuzzleOffset);
		const FVector TargetNow = PredictedAimLocation(Entry, Now);

		Component->AimPoint = Profile->bPredictLead
			? PredictAimPoint(Muzzle, TargetNow, Entry.Velocity, Profile->ProjectileSpeed, Profile->MaxLeadSeconds)
			: TargetNow;

		++OnTarget;
	}

	Stats.TurretsOnTarget = OnTarget;
}

void UTurretMindSubsystem::RunAcquisitionRound()
{
	const int32 Count = Turrets.Num();
	if (Count == 0 || MaxAcquisitionsPerFrame <= 0)
	{
		return;
	}

	UWorld* World = GetWorld();
	const double Now = World->GetTimeSeconds();

	int32 BudgetLeft = MaxAcquisitionsPerFrame;
	int32 Examined = 0;

	// The round-robin. The cursor keeps its place between frames, so with a budget of four and forty
	// turrets everybody gets a turn inside ten frames — later than they would like, never never.
	while (BudgetLeft > 0 && Examined < Count)
	{
		TurretCursor = (TurretCursor + 1) % Turrets.Num();
		++Examined;

		const int32 Slot = TurretCursor;
		if (!Turrets.IsValidIndex(Slot) || !Turrets[Slot].bAlive)
		{
			continue;
		}

		UTurretMindComponent* Component = Turrets[Slot].Component.Get();
		if (!Component || !IsValid(Component) || !Component->GetOwner())
		{
			ReleaseTurretSlot(Slot);
			continue;
		}

		if (!Component->bEnabled)
		{
			continue;
		}

		const UTurretMindProfile* Profile = Component->GetEffectiveProfile();
		if (!Profile)
		{
			continue;
		}

		if (Now - Turrets[Slot].LastEvaluateTime < Profile->ReacquireInterval)
		{
			continue;
		}

		--BudgetLeft;
		++Stats.AcquisitionsThisFrame;

		if (EvaluateTurret(Slot))
		{
			// Only a completed evaluation resets the clock. One that ran out of trace budget is due
			// again immediately, and the frame budget guarantees it makes progress next time round.
			if (Turrets.IsValidIndex(Slot) && Turrets[Slot].bAlive)
			{
				Turrets[Slot].LastEvaluateTime = Now;
			}
		}
	}
}

// -------------------------------------------------------------------------------------------------------
// Scoring
// -------------------------------------------------------------------------------------------------------

void UTurretMindSubsystem::MakeTargetInfo(const FTargetEntry& Entry, float Distance, FTurretMindTargetInfo& OutInfo) const
{
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	OutInfo.Actor = Entry.Actor.Get();
	OutInfo.Location = Entry.Location;
	OutInfo.AimLocation = Entry.AimLocation;
	OutInfo.Velocity = Entry.Velocity;
	OutInfo.Distance = Distance;
	OutInfo.Threat = Entry.Threat;
	OutInfo.Health = Entry.Health;
	OutInfo.PathProgress = Entry.PathProgress;
	OutInfo.Radius = Entry.Radius;
	OutInfo.TimeRegistered = static_cast<float>(Now - Entry.RegisterTime);
	OutInfo.TeamId = Entry.TeamId;
}

float UTurretMindSubsystem::ScoreTarget(
	const FTurretMindTurretInfo& TurretInfo,
	const UTurretMindProfile& Profile,
	ETurretMindPolicy Policy,
	const FTargetEntry& Entry,
	float Distance,
	const FTurretMindScoreDelegate* CustomScorer) const
{
	// Every policy answers in the same currency: a fitness in 0..1 where 1 is the best target there
	// could be. That is what lets one stickiness number mean "twenty per cent better" no matter which
	// policy the turret is running.
	const float Range = FMath::Max(TurretInfo.MaxRange, 1.0f);
	const float Closeness = FMath::Clamp(1.0f - Distance / Range, 0.0f, 1.0f);

	float Fitness = 0.0f;

	switch (Policy)
	{
	case ETurretMindPolicy::Nearest:
		Fitness = Closeness;
		break;

	case ETurretMindPolicy::LowestHealth:
		Fitness = 1.0f - FMath::Clamp(Entry.Health, 0.0f, 1.0f);
		break;

	case ETurretMindPolicy::HighestThreat:
		Fitness = FMath::Clamp(Entry.Threat / FMath::Max(Profile.ThreatScale, UE_KINDA_SMALL_NUMBER), 0.0f, 1.0f);
		break;

	case ETurretMindPolicy::FurthestAlongPath:
		Fitness = FMath::Clamp(Entry.PathProgress, 0.0f, 1.0f);
		break;

	case ETurretMindPolicy::FirstSeen:
	{
		// Older is better, saturating: the ordering is what matters, and a bounded curve keeps the
		// result inside 0..1 without needing to know how long the match has been running.
		const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		const float Age = static_cast<float>(FMath::Max(0.0, Now - Entry.RegisterTime));
		Fitness = Age / (Age + 10.0f);
		break;
	}

	case ETurretMindPolicy::Custom:
		if (CustomScorer && CustomScorer->IsBound())
		{
			FTurretMindTargetInfo Info;
			MakeTargetInfo(Entry, Distance, Info);
			return CustomScorer->Execute(TurretInfo, Info);
		}
		// No scorer bound: behave like Nearest rather than treating every target as equally worthless.
		Fitness = Closeness;
		break;

	default:
		Fitness = Closeness;
		break;
	}

	// One per cent of distance mixed in as a tie-break. Under Furthest Along Path two attackers side by
	// side on the same path have identical progress, and without this the winner would be whichever one
	// happened to sit earlier in a cell bucket — which changes when they cross a cell boundary, and
	// that is a visible flicker.
	return Fitness * 0.99f + Closeness * 0.01f;
}

void UTurretMindSubsystem::GatherCandidates(
	const FTurretMindTurretInfo& TurretInfo,
	const UTurretMindProfile& Profile,
	ETurretMindPolicy Policy,
	const AActor* IgnoreActor,
	const FTurretMindScoreDelegate* CustomScorer,
	TArray<FCandidate>& OutCandidates,
	int32 CurrentTargetSlot,
	float& OutCurrentScore)
{
	OutCandidates.Reset();
	OutCurrentScore = -1.0f;

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const float MaxRange = FMath::Max(Profile.MaxRange, 0.0f);
	const float MinRange = FMath::Clamp(Profile.MinRange, 0.0f, MaxRange);
	const float MaxRangeSq = MaxRange * MaxRange;
	const float MinRangeSq = MinRange * MinRange;
	const float ConeCos = Profile.GetConeCosine();
	const bool bTestCone = !Profile.HasFullTraverse();
	const FVector Forward = TurretInfo.Forward.GetSafeNormal();
	const int32 MaxConsidered = FMath::Max(1, Profile.MaxTargetsConsidered);

	// The rectangle of cells the range covers, padded by one so a target that has drifted out of its
	// last sampled cell since the index touched it is still found.
	const FIntPoint MinCell = CellOf(TurretInfo.MuzzleLocation - FVector(MaxRange, MaxRange, 0.0f)) - FIntPoint(1, 1);
	const FIntPoint MaxCell = CellOf(TurretInfo.MuzzleLocation + FVector(MaxRange, MaxRange, 0.0f)) + FIntPoint(1, 1);
	const int64 RectCells = static_cast<int64>(MaxCell.X - MinCell.X + 1) * static_cast<int64>(MaxCell.Y - MinCell.Y + 1);

	int32 Considered = 0;
	int32 CellsOpened = 0;

	auto ConsiderBucket = [&](const TArray<int32>& Bucket)
	{
		for (int32 Slot : Bucket)
		{
			if (Considered >= MaxConsidered)
			{
				return;
			}
			if (!Targets.IsValidIndex(Slot))
			{
				continue;
			}

			const FTargetEntry& Entry = Targets[Slot];
			if (!Entry.bAlive || !Entry.bTargetable)
			{
				continue;
			}

			AActor* TargetActor = Entry.Actor.Get();
			if (!TargetActor || TargetActor == IgnoreActor)
			{
				continue;
			}

			if (!Profile.PassesTeamFilter(TurretInfo.TeamId, Entry.TeamId))
			{
				continue;
			}

			const FVector Predicted = PredictedAimLocation(Entry, Now);
			const FVector ToTarget = Predicted - TurretInfo.MuzzleLocation;
			const float DistSq = static_cast<float>(ToTarget.SizeSquared());
			if (DistSq > MaxRangeSq || DistSq < MinRangeSq)
			{
				continue;
			}

			++Considered;

			if (bTestCone)
			{
				const FVector Direction = ToTarget.GetSafeNormal();
				if (!Direction.IsNearlyZero() && FVector::DotProduct(Direction, Forward) < ConeCos)
				{
					continue;
				}
			}

			const float Distance = FMath::Sqrt(DistSq);
			const float Score = ScoreTarget(TurretInfo, Profile, Policy, Entry, Distance, CustomScorer);

			if (Slot == CurrentTargetSlot)
			{
				OutCurrentScore = Score;
			}

			FCandidate& Candidate = OutCandidates.AddDefaulted_GetRef();
			Candidate.Slot = Slot;
			Candidate.Score = Score;
			Candidate.AimLocation = Predicted;
			Candidate.Velocity = Entry.Velocity;
			Candidate.Radius = Entry.Radius;
		}
	};

	if (RectCells > static_cast<int64>(Cells.Num()))
	{
		// The range covers more cells than the world has occupied ones — a huge range, a tiny cell
		// size, or a nearly empty level. Walking the occupied cells is then strictly cheaper than
		// probing every coordinate in the rectangle, and self-tuning beats a magic constant.
		for (const TPair<FIntPoint, TArray<int32>>& Pair : Cells)
		{
			if (Pair.Key.X < MinCell.X || Pair.Key.X > MaxCell.X || Pair.Key.Y < MinCell.Y || Pair.Key.Y > MaxCell.Y)
			{
				continue;
			}
			++CellsOpened;
			ConsiderBucket(Pair.Value);
			if (Considered >= MaxConsidered)
			{
				break;
			}
		}
	}
	else
	{
		for (int32 Y = MinCell.Y; Y <= MaxCell.Y && Considered < MaxConsidered; ++Y)
		{
			for (int32 X = MinCell.X; X <= MaxCell.X && Considered < MaxConsidered; ++X)
			{
				const TArray<int32>* Bucket = Cells.Find(FIntPoint(X, Y));
				if (!Bucket)
				{
					continue;
				}
				++CellsOpened;
				ConsiderBucket(*Bucket);
			}
		}
	}

	Stats.CellsVisited += CellsOpened;
	Stats.TargetsConsidered += Considered;
}

// -------------------------------------------------------------------------------------------------------
// Line of sight
// -------------------------------------------------------------------------------------------------------

bool UTurretMindSubsystem::ResolveLineOfSight(int32 TurretSlot, int32 TargetSlot, const FVector& From, const FVector& To,
	ECollisionChannel Channel, const AActor* IgnoreTurret, const AActor* IgnoreTarget,
	int32& InOutTraceBudget, bool& bOutResolved)
{
	bOutResolved = true;

	UWorld* World = GetWorld();
	if (!World)
	{
		return true;
	}

	const double Now = World->GetTimeSeconds();
	const uint32 TargetSerial = Targets.IsValidIndex(TargetSlot) ? Targets[TargetSlot].Serial : 0;
	const uint64 Key = (static_cast<uint64>(static_cast<uint32>(TurretSlot)) << 32) | static_cast<uint32>(TargetSlot);

	if (const FLosEntry* Cached = LosCache.Find(Key))
	{
		if (Cached->TargetSerial == TargetSerial && (Now - Cached->Time) <= LosCacheSeconds)
		{
			++Stats.LosCacheHitsThisFrame;
			return Cached->bVisible;
		}
	}

	if (InOutTraceBudget <= 0)
	{
		// Out of traces this frame. Say so instead of guessing: the caller leaves the turret on the
		// target it already has and finishes the evaluation next frame.
		bOutResolved = false;
		return false;
	}

	--InOutTraceBudget;
	++Stats.TracesThisFrame;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(TurretMindLineOfSight), /*bTraceComplex=*/false);
	if (IgnoreTurret)
	{
		Params.AddIgnoredActor(IgnoreTurret);
	}
	if (IgnoreTarget)
	{
		Params.AddIgnoredActor(IgnoreTarget);
	}

	const bool bBlocked = World->LineTraceTestByChannel(From, To, Channel, Params);
	const bool bVisible = !bBlocked;

	FLosEntry& Entry = LosCache.FindOrAdd(Key);
	Entry.Time = Now;
	Entry.TargetSerial = TargetSerial;
	Entry.bVisible = bVisible;

	return bVisible;
}

bool UTurretMindSubsystem::PeekLineOfSight(int32 TurretSlot, int32 TargetSlot) const
{
	if (!Targets.IsValidIndex(TargetSlot))
	{
		return true;
	}

	const uint64 Key = (static_cast<uint64>(static_cast<uint32>(TurretSlot)) << 32) | static_cast<uint32>(TargetSlot);
	if (const FLosEntry* Cached = LosCache.Find(Key))
	{
		if (Cached->TargetSerial == Targets[TargetSlot].Serial)
		{
			return Cached->bVisible;
		}
	}

	return true;
}

void UTurretMindSubsystem::PruneLosCache()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	// Once a second, not every frame: a stale entry costs a few bytes, and walking the map costs more
	// than the entries do.
	if (Now - LastLosPruneTime < 1.0)
	{
		return;
	}
	LastLosPruneTime = Now;

	const double Cutoff = Now - FMath::Max(LosCacheSeconds * 4.0, 2.0);
	for (auto It = LosCache.CreateIterator(); It; ++It)
	{
		if (It.Value().Time < Cutoff)
		{
			It.RemoveCurrent();
		}
	}
}

// -------------------------------------------------------------------------------------------------------
// Evaluation
// -------------------------------------------------------------------------------------------------------

void UTurretMindSubsystem::ApplyTargetToComponent(FTurretEntry& Turret, int32 NewSlot, bool bHasLos, double Now)
{
	UTurretMindComponent* Component = Turret.Component.Get();
	if (!Component)
	{
		return;
	}

	AActor* const PreviousActor = Component->CurrentTarget.Get();
	const int32 PreviousSlot = Turret.TargetSlot;
	const uint32 PreviousSerial = Turret.TargetSerial;

	if (NewSlot == INDEX_NONE || !Targets.IsValidIndex(NewSlot) || !Targets[NewSlot].bAlive)
	{
		Turret.TargetSlot = INDEX_NONE;
		Turret.TargetSerial = 0;
		Turret.TargetScore = 0.0f;
		Component->CurrentTarget = nullptr;
		Component->CurrentTargetComponent = nullptr;
		Component->bHasLineOfSight = false;
		Component->TargetAcquiredTime = 0.0f;

		// Broadcast last. A listener is allowed to destroy the turret, spawn something, or register a
		// new target, all of which can move the arrays this function was reading.
		if (PreviousActor)
		{
			Component->OnTargetLost.Broadcast(PreviousActor);
		}
		return;
	}

	const FTargetEntry& Entry = Targets[NewSlot];
	AActor* const NewActor = Entry.Actor.Get();

	Turret.TargetSlot = NewSlot;
	Turret.TargetSerial = Entry.Serial;

	Component->CurrentTarget = NewActor;
	Component->CurrentTargetComponent = Entry.Component;
	Component->bHasLineOfSight = bHasLos;

	const bool bChanged = (PreviousSlot != NewSlot) || (PreviousSerial != Entry.Serial);
	if (!bChanged)
	{
		return;
	}

	Component->TargetAcquiredTime = static_cast<float>(Now);

	if (PreviousActor)
	{
		++Stats.SwitchesThisFrame;
		Component->OnTargetSwitched.Broadcast(PreviousActor, NewActor);
	}
	else
	{
		Component->OnTargetAcquired.Broadcast(NewActor);
	}
}

bool UTurretMindSubsystem::EvaluateTurret(int32 TurretSlot)
{
	UWorld* World = GetWorld();
	if (!World || !Turrets.IsValidIndex(TurretSlot))
	{
		return true;
	}

	UTurretMindComponent* Component = Turrets[TurretSlot].Component.Get();
	AActor* Owner = Component ? Component->GetOwner() : nullptr;
	UTurretMindProfile* Profile = Component ? Component->GetEffectiveProfile() : nullptr;
	if (!Component || !Owner || !Profile)
	{
		return true;
	}

	const double Now = World->GetTimeSeconds();
	const FTransform OwnerTransform = Owner->GetActorTransform();

	FTurretMindTurretInfo TurretInfo;
	TurretInfo.Actor = Owner;
	TurretInfo.MuzzleLocation = OwnerTransform.TransformPosition(Component->MuzzleOffset);
	TurretInfo.Forward = OwnerTransform.GetUnitAxis(EAxis::X);
	TurretInfo.MaxRange = Profile->MaxRange;
	TurretInfo.TeamId = Component->TeamId;

	const ETurretMindPolicy Policy = bHasPolicyOverride ? PolicyOverride : Profile->Policy;
	const bool bRequireLos = bHasLosOverride ? bLosOverrideValue : Profile->bRequireLineOfSight;

	const int32 CurrentSlot = IsTargetSlotValid(Turrets[TurretSlot].TargetSlot, Turrets[TurretSlot].TargetSerial)
		? Turrets[TurretSlot].TargetSlot
		: INDEX_NONE;

	float CurrentScore = -1.0f;
	const FTurretMindScoreDelegate* Scorer = Component->CustomScorer.IsBound() ? &Component->CustomScorer : nullptr;

	GatherCandidates(TurretInfo, *Profile, Policy, Owner, Scorer, CandidateScratch, CurrentSlot, CurrentScore);

	int32 BestSlot = INDEX_NONE;
	float BestScore = -1.0f;
	bool bRanOutOfTraces = false;

	if (!bRequireLos)
	{
		for (const FCandidate& Candidate : CandidateScratch)
		{
			if (Candidate.Score > BestScore)
			{
				BestScore = Candidate.Score;
				BestSlot = Candidate.Slot;
			}
		}
	}
	else
	{
		// Best first, then walk down until one of them can actually be seen. Anything below the first
		// visible candidate is worse by definition and never costs a trace.
		CandidateScratch.Sort([](const FCandidate& A, const FCandidate& B) { return A.Score > B.Score; });

		for (const FCandidate& Candidate : CandidateScratch)
		{
			AActor* TargetActor = Targets.IsValidIndex(Candidate.Slot) ? Targets[Candidate.Slot].Actor.Get() : nullptr;

			bool bResolved = false;
			const bool bVisible = ResolveLineOfSight(
				TurretSlot, Candidate.Slot,
				TurretInfo.MuzzleLocation, Candidate.AimLocation,
				Profile->TraceChannel.GetValue(),
				Owner, TargetActor,
				TraceBudgetRemaining, bResolved);

			if (!bResolved)
			{
				bRanOutOfTraces = true;
				break;
			}

			if (bVisible)
			{
				BestSlot = Candidate.Slot;
				BestScore = Candidate.Score;
				break;
			}
		}
	}

	if (bRanOutOfTraces && BestSlot == INDEX_NONE)
	{
		// Nothing decided and no traces left. Leave the turret exactly as it is — it keeps aiming at
		// what it had — and come back next frame with a fresh ration.
		return false;
	}

	int32 ChosenSlot = INDEX_NONE;

	if (BestSlot == INDEX_NONE)
	{
		// Nothing in range, nothing on a valid team, or nothing visible. Let the target go.
		ChosenSlot = INDEX_NONE;
	}
	else if (CurrentSlot == INDEX_NONE || BestSlot == CurrentSlot || CurrentScore < 0.0f)
	{
		// No incumbent, or the incumbent did not even make the candidate list any more.
		ChosenSlot = BestSlot;
	}
	else
	{
		// Hysteresis. Two attackers a hair apart in score would otherwise make the barrel flick back
		// and forth every re-evaluation, which reads as broken however correct the ranking is.
		const float Stickiness = FMath::Max(0.0f, Profile->TargetStickiness);
		ChosenSlot = (BestScore > CurrentScore * (1.0f + Stickiness)) ? BestSlot : CurrentSlot;
	}

	Turrets[TurretSlot].TargetScore = (ChosenSlot == BestSlot) ? BestScore : CurrentScore;

	// Only the candidate the walk stopped on was actually traced. When stickiness keeps the incumbent
	// instead, its verdict comes out of the cache rather than being assumed clear — a turret Blueprint
	// that gates its trigger on Has Line Of Sight would otherwise be told to shoot at a wall.
	bool bChosenHasLos = true;
	if (bRequireLos && ChosenSlot != INDEX_NONE && ChosenSlot != BestSlot)
	{
		bChosenHasLos = PeekLineOfSight(TurretSlot, ChosenSlot);
	}

	// Last thing this function touches: the delegates inside can move the arrays above.
	ApplyTargetToComponent(Turrets[TurretSlot], ChosenSlot, bChosenHasLos, Now);
	return true;
}

// -------------------------------------------------------------------------------------------------------
// Public queries
// -------------------------------------------------------------------------------------------------------

FVector UTurretMindSubsystem::PredictAimPoint(FVector MuzzleLocation, FVector TargetLocation, FVector TargetVelocity, float ProjectileSpeed, float MaxLeadSeconds)
{
	if (ProjectileSpeed <= 0.0f)
	{
		// Hitscan. There is no travel time to lead against, so the aim point is the target itself.
		return TargetLocation;
	}

	const float MaxLead = FMath::Max(0.0f, MaxLeadSeconds);

	FVector Aim = TargetLocation;
	for (int32 Iteration = 0; Iteration < 3; ++Iteration)
	{
		const float TravelTime = FMath::Min(static_cast<float>((Aim - MuzzleLocation).Size()) / ProjectileSpeed, MaxLead);
		Aim = TargetLocation + TargetVelocity * TravelTime;
	}

	return Aim;
}

bool UTurretMindSubsystem::FindBestTarget(FVector Origin, FVector Forward, UTurretMindProfile* Profile, int32 TeamId, AActor*& OutTarget, FVector& OutAimPoint, AActor* IgnoreActor)
{
	OutTarget = nullptr;
	OutAimPoint = Origin;

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	UTurretMindProfile* UseProfile = Profile ? Profile : GetDefaultProfile();
	if (!UseProfile)
	{
		return false;
	}

	FTurretMindTurretInfo TurretInfo;
	TurretInfo.Actor = IgnoreActor;
	TurretInfo.MuzzleLocation = Origin;
	TurretInfo.Forward = Forward.IsNearlyZero() ? FVector::ForwardVector : Forward.GetSafeNormal();
	TurretInfo.MaxRange = UseProfile->MaxRange;
	TurretInfo.TeamId = TeamId;

	TArray<FCandidate> Candidates;
	float UnusedScore = -1.0f;
	GatherCandidates(TurretInfo, *UseProfile, UseProfile->Policy, IgnoreActor, nullptr, Candidates, INDEX_NONE, UnusedScore);

	if (Candidates.Num() == 0)
	{
		return false;
	}

	Candidates.Sort([](const FCandidate& A, const FCandidate& B) { return A.Score > B.Score; });

	const bool bRequireLos = bHasLosOverride ? bLosOverrideValue : UseProfile->bRequireLineOfSight;

	for (const FCandidate& Candidate : Candidates)
	{
		if (!Targets.IsValidIndex(Candidate.Slot))
		{
			continue;
		}
		AActor* TargetActor = Targets[Candidate.Slot].Actor.Get();
		if (!TargetActor)
		{
			continue;
		}

		if (bRequireLos)
		{
			// Deliberately not the cached, budgeted path: this call is a one-off answer for something
			// that is not in the round-robin, so it pays for its own trace right now.
			FCollisionQueryParams Params(SCENE_QUERY_STAT(TurretMindFindBestTarget), /*bTraceComplex=*/false);
			if (IgnoreActor)
			{
				Params.AddIgnoredActor(IgnoreActor);
			}
			Params.AddIgnoredActor(TargetActor);

			++Stats.TracesThisFrame;
			if (World->LineTraceTestByChannel(Origin, Candidate.AimLocation, UseProfile->TraceChannel.GetValue(), Params))
			{
				continue;
			}
		}

		OutTarget = TargetActor;
		OutAimPoint = UseProfile->bPredictLead
			? PredictAimPoint(Origin, Candidate.AimLocation, Candidate.Velocity, UseProfile->ProjectileSpeed, UseProfile->MaxLeadSeconds)
			: Candidate.AimLocation;
		return true;
	}

	return false;
}

// -------------------------------------------------------------------------------------------------------
// Budgets and overrides
// -------------------------------------------------------------------------------------------------------

void UTurretMindSubsystem::SetAcquisitionBudget(int32 InMaxAcquisitionsPerFrame)
{
	MaxAcquisitionsPerFrame = FMath::Max(0, InMaxAcquisitionsPerFrame);
}

void UTurretMindSubsystem::SetTraceBudget(int32 InMaxLineTracesPerFrame)
{
	MaxLineTracesPerFrame = FMath::Max(0, InMaxLineTracesPerFrame);
}

void UTurretMindSubsystem::SetPolicyOverride(ETurretMindPolicy InPolicy)
{
	PolicyOverride = InPolicy;
	bHasPolicyOverride = true;
}

void UTurretMindSubsystem::ClearPolicyOverride()
{
	bHasPolicyOverride = false;
}

void UTurretMindSubsystem::SetLineOfSightOverride(bool bRequireLineOfSight)
{
	bLosOverrideValue = bRequireLineOfSight;
	bHasLosOverride = true;

	// Every cached verdict was taken under the old rule; throwing them away costs one round of traces
	// and stops a stale "blocked" from surviving the switch.
	LosCache.Reset();
}

void UTurretMindSubsystem::ClearLineOfSightOverride()
{
	bHasLosOverride = false;
	LosCache.Reset();
}

// -------------------------------------------------------------------------------------------------------
// Display
// -------------------------------------------------------------------------------------------------------

void UTurretMindSubsystem::DrawStats(UCanvas* Canvas)
{
	if (!Canvas || !bShowStats)
	{
		return;
	}

	LastHudDrawFrame = GFrameCounter;

	UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
	if (!Font)
	{
		return;
	}

	constexpr float BoxX = 24.0f;
	constexpr float BoxY = 90.0f;
	constexpr float BoxWidth = 296.0f;
	constexpr float LineHeight = 15.0f;
	constexpr int32 LineCount = 13;

	FCanvasTileItem Background(
		FVector2D(BoxX - 8.0f, BoxY - 8.0f),
		GWhiteTexture,
		FVector2D(BoxWidth, LineCount * LineHeight + 16.0f),
		FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
	Background.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Background);

	float LineY = BoxY;

	auto DrawLine = [&](FStringView Line, const FLinearColor& Color)
	{
		FCanvasTextStringViewItem Item(FVector2D(BoxX, LineY), Line, Font, Color);
		Canvas->DrawItem(Item);
		LineY += LineHeight;
	};

	const FLinearColor Heading(0.55f, 0.85f, 1.0f, 1.0f);
	const FLinearColor Body(0.9f, 0.9f, 0.9f, 1.0f);
	const FLinearColor Good(0.55f, 0.95f, 0.55f, 1.0f);

	TStringBuilder<160> Line;

	Line.Reset();
	Line.Append(TEXT("TurretMind"));
	DrawLine(Line.ToView(), Heading);

	Line.Reset();
	Line.Appendf(TEXT("Turrets            %d  (%d on target)"), Stats.Turrets, Stats.TurretsOnTarget);
	DrawLine(Line.ToView(), Body);

	Line.Reset();
	Line.Appendf(TEXT("Targets            %d"), Stats.Targets);
	DrawLine(Line.ToView(), Body);

	Line.Reset();
	Line.Appendf(TEXT("Acquisitions       %d / %d"), Stats.AcquisitionsThisFrame, MaxAcquisitionsPerFrame);
	DrawLine(Line.ToView(), Stats.AcquisitionsThisFrame <= MaxAcquisitionsPerFrame ? Good : Body);

	Line.Reset();
	Line.Appendf(TEXT("Line traces        %d / %d"), Stats.TracesThisFrame, MaxLineTracesPerFrame);
	DrawLine(Line.ToView(), Stats.TracesThisFrame <= MaxLineTracesPerFrame ? Good : Body);

	Line.Reset();
	Line.Appendf(TEXT("LoS cache hits     %d"), Stats.LosCacheHitsThisFrame);
	DrawLine(Line.ToView(), Body);

	Line.Reset();
	Line.Appendf(TEXT("Cells visited      %d"), Stats.CellsVisited);
	DrawLine(Line.ToView(), Body);

	// The headline. Naive is turrets x targets; this is what the grid actually looked at.
	const int32 Naive = Stats.AcquisitionsThisFrame * Stats.Targets;
	Line.Reset();
	Line.Appendf(TEXT("Targets considered %d  (naive %d)"), Stats.TargetsConsidered, Naive);
	DrawLine(Line.ToView(), Good);

	Line.Reset();
	Line.Appendf(TEXT("Switches           %d"), Stats.SwitchesThisFrame);
	DrawLine(Line.ToView(), Body);

	Line.Reset();
	Line.Appendf(TEXT("Index refreshed    %d / %d"), Stats.IndexRefreshedThisFrame, Stats.Targets);
	DrawLine(Line.ToView(), Body);

	Line.Reset();
	Line.Appendf(TEXT("Acquire            %.3f ms"), Stats.AcquireMs);
	DrawLine(Line.ToView(), Body);

	const UEnum* PolicyEnum = StaticEnum<ETurretMindPolicy>();
	Line.Reset();
	if (bHasPolicyOverride && PolicyEnum)
	{
		Line.Appendf(TEXT("Policy             %s (forced)"), *PolicyEnum->GetNameStringByValue(static_cast<int64>(PolicyOverride)));
	}
	else
	{
		Line.Append(TEXT("Policy             per profile"));
	}
	DrawLine(Line.ToView(), Body);

	Line.Reset();
	if (bHasLosOverride)
	{
		Line.Appendf(TEXT("Line of sight      %s (forced)"), bLosOverrideValue ? TEXT("on") : TEXT("off"));
	}
	else
	{
		Line.Append(TEXT("Line of sight      per profile"));
	}
	DrawLine(Line.ToView(), Body);
}

void UTurretMindSubsystem::DrawWorldDebug()
{
#if ENABLE_DRAW_DEBUG
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	int32 Drawn = 0;

	for (int32 Slot = 0; Slot < Turrets.Num() && Drawn < MaxDebugDrawTurrets; ++Slot)
	{
		const FTurretEntry& Turret = Turrets[Slot];
		if (!Turret.bAlive)
		{
			continue;
		}

		const UTurretMindComponent* Component = Turret.Component.Get();
		const AActor* Owner = Component ? Component->GetOwner() : nullptr;
		const UTurretMindProfile* Profile = Component ? Component->GetEffectiveProfile() : nullptr;
		if (!Component || !Owner || !Profile || !Component->bEnabled)
		{
			continue;
		}

		++Drawn;

		const FVector Muzzle = Owner->GetActorTransform().TransformPosition(Component->MuzzleOffset);

		if (bDrawRanges)
		{
			// Flat on the XY plane, because the grid is flat on the XY plane. Seeing the circle is
			// seeing the query.
			DrawDebugCircle(World, Owner->GetActorLocation(), Profile->MaxRange, 48,
				FColor(40, 90, 140), false, -1.0f, SDPG_World, 2.0f,
				FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), false);

			if (Profile->MinRange > 1.0f)
			{
				DrawDebugCircle(World, Owner->GetActorLocation(), Profile->MinRange, 24,
					FColor(140, 60, 40), false, -1.0f, SDPG_World, 1.0f,
					FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), false);
			}
		}

		if (Turret.TargetSlot == INDEX_NONE || !IsTargetSlotValid(Turret.TargetSlot, Turret.TargetSerial))
		{
			continue;
		}

		const FTargetEntry& Entry = Targets[Turret.TargetSlot];
		const FVector TargetNow = PredictedAimLocation(Entry, Now);
		const FColor LineColor = Component->bHasLineOfSight ? FColor(80, 220, 110) : FColor(220, 90, 70);

		// Where the target is now.
		DrawDebugLine(World, Muzzle, TargetNow, LineColor, false, -1.0f, SDPG_World, 2.0f);

		// Where the turret is actually pointing. With lead on, the two separate visibly the moment the
		// target moves across the line of fire — which is the whole point of the feature.
		const FVector Aim = Component->GetAimPoint();
		if (!Aim.Equals(TargetNow, 1.0f))
		{
			DrawDebugLine(World, TargetNow, Aim, FColor(250, 210, 70), false, -1.0f, SDPG_World, 1.5f);
		}
		DrawDebugCrosshairs(World, Aim, FRotator::ZeroRotator, 90.0f, FColor(250, 210, 70), false, -1.0f, SDPG_World);
	}
#endif // ENABLE_DRAW_DEBUG
}

#if WITH_EDITOR
void UTurretMindSubsystem::OnEditorViewportDraw(UCanvas* Canvas, APlayerController* PlayerController)
{
	if (!bShowStats || !Canvas)
	{
		return;
	}

	// A game HUD already drew the box this frame or last. Do not stack a second one on top of it.
	if (GFrameCounter - LastHudDrawFrame <= 1)
	{
		return;
	}

	const uint64 SavedFrame = LastHudDrawFrame;
	DrawStats(Canvas);
	LastHudDrawFrame = SavedFrame;
}
#endif
