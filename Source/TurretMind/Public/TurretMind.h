// Copyright 2026 Simulated Flow. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

/** TurretMind — one budgeted target acquisition service for many turrets (runtime module). */
class FTurretMindModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
