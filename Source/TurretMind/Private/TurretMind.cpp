// Copyright 2026 Simulated Flow. All Rights Reserved.

#include "TurretMind.h"
#include "TurretMindLog.h"

DEFINE_LOG_CATEGORY(LogTurretMind);

#define LOCTEXT_NAMESPACE "FTurretMindModule"

void FTurretMindModule::StartupModule()
{
	UE_LOG(LogTurretMind, Log, TEXT("TurretMind started."));
}

void FTurretMindModule::ShutdownModule()
{
	UE_LOG(LogTurretMind, Log, TEXT("TurretMind shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTurretMindModule, TurretMind)
