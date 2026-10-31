// Copyright NeedsGames. All Rights Reserved.

#include "Traversal/PTTraversalTypes.h"
#include "Animation/AnimMontage.h"
#include "Components/PrimitiveComponent.h"
#include "UObject/CoreNet.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PTTraversalTypes)

bool FPTTraversalCheckResult::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	uint8 ActionByte = static_cast<uint8>(Action);
	Ar << ActionByte;
	Action = static_cast<EPTTraversalAction>(ActionByte);

	uint8 Flags = (bHasFrontLedge ? 1 : 0) | (bHasBackLedge ? 2 : 0) | (bHasBackFloor ? 4 : 0) | (bProcedural ? 8 : 0);
	Ar << Flags;
	bHasFrontLedge = (Flags & 1) != 0;
	bHasBackLedge = (Flags & 2) != 0;
	bHasBackFloor = (Flags & 4) != 0;
	bProcedural = (Flags & 8) != 0;

	Ar << FrontLedgeLocation;
	Ar << FrontLedgeNormal;
	Ar << BackLedgeLocation;
	Ar << BackFloorLocation;
	Ar << ObstacleHeight;
	Ar << ObstacleDepth;
	Ar << BackFloorHeight;
	Ar << Speed;
	Ar << MontageStartTime;
	Ar << MontagePlayRate;

	UObject* HitObject = HitComponent;
	UObject* MontageObject = Montage;
	if (Map)
	{
		Map->SerializeObject(Ar, UPrimitiveComponent::StaticClass(), HitObject);
		Map->SerializeObject(Ar, UAnimMontage::StaticClass(), MontageObject);
	}
	if (Ar.IsLoading())
	{
		HitComponent = Cast<UPrimitiveComponent>(HitObject);
		Montage = Cast<UAnimMontage>(MontageObject);
	}

	bOutSuccess = true;
	return true;
}
