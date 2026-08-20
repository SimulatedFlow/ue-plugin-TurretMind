// Copyright 2026 Silvan Teufel. All Rights Reserved.

#include "TurretMindSettings.h"

UTurretMindSettings::UTurretMindSettings()
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("TurretMind");
}

FName UTurretMindSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

FName UTurretMindSettings::GetSectionName() const
{
	return TEXT("TurretMind");
}

const UTurretMindSettings& UTurretMindSettings::Get()
{
	const UTurretMindSettings* Settings = GetDefault<UTurretMindSettings>();
	check(Settings);
	return *Settings;
}
