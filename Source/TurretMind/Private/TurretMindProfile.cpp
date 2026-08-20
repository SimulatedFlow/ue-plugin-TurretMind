// Copyright 2026 Simulated Flow. All Rights Reserved.

#include "TurretMindProfile.h"

UTurretMindProfile::UTurretMindProfile()
{
}

float UTurretMindProfile::GetConeCosine() const
{
	if (HasFullTraverse())
	{
		// Everything in front, behind and beside passes. -1 makes the dot product test a no-op.
		return -1.0f;
	}

	const float HalfAngleRadians = FMath::DegreesToRadians(FMath::Clamp(FieldOfViewDegrees, 1.0f, 360.0f) * 0.5f);
	return FMath::Cos(HalfAngleRadians);
}

bool UTurretMindProfile::PassesTeamFilter(int32 TurretTeamId, int32 TargetTeamId) const
{
	switch (TeamFilter)
	{
	case ETurretMindTeamFilter::AnyTeam:
		return true;

	case ETurretMindTeamFilter::SameTeam:
		return TargetTeamId == TurretTeamId;

	case ETurretMindTeamFilter::ListedTeams:
		return TargetTeamIds.Contains(TargetTeamId);

	case ETurretMindTeamFilter::DifferentTeam:
	default:
		return TargetTeamId != TurretTeamId;
	}
}
