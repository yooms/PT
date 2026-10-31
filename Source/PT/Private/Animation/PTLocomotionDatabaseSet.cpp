// Copyright yoomsGames. All Rights Reserved.

#include "Animation/PTLocomotionDatabaseSet.h"
#include "PoseSearch/PoseSearchDatabase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PTLocomotionDatabaseSet)

void UPTLocomotionDatabaseSet::SelectDatabases(const FPTLocomotionQuery& Query, TArray<UPoseSearchDatabase*>& OutDatabases) const
{
	OutDatabases.Reset();

	for (const FPTDatabaseRule& Rule : Rules)
	{
		if (!Rule.Matches(Query))
		{
			continue;
		}

		for (UPoseSearchDatabase* Database : Rule.Databases)
		{
			if (Database)
			{
				OutDatabases.AddUnique(Database);
			}
		}

		if (Rule.bStopEvaluation)
		{
			break;
		}
	}

	if (OutDatabases.Num() == 0)
	{
		for (UPoseSearchDatabase* Database : FallbackDatabases)
		{
			if (Database)
			{
				OutDatabases.AddUnique(Database);
			}
		}
	}
}
