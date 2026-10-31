// Copyright NeedsGames. All Rights Reserved.

#include "Core/PTGameState.h"
#include "Online/PTHostMigrationSubsystem.h"
#include "Engine/GameInstance.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PTGameState)

APTGameState::APTGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetNetUpdateFrequency(10.f);
}

void APTGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, MigrationData);
}

void APTGameState::SetMigrationPayload(TArray<uint8>&& Compressed, int32 UncompressedSize)
{
	check(HasAuthority());
	MigrationData.Revision++;
	MigrationData.UncompressedSize = UncompressedSize;
	MigrationData.SetPayload(Compressed);
	ForceNetUpdate();
}

void APTGameState::OnRep_MigrationData()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UPTHostMigrationSubsystem* Migration = GI->GetSubsystem<UPTHostMigrationSubsystem>())
		{
			Migration->OnSnapshotReplicated(MigrationData);
		}
	}
}

void APTGameState::MulticastHostLeaving_Implementation(bool bAllowMigration)
{
	if (HasAuthority())
	{
		return;
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UPTHostMigrationSubsystem* Migration = GI->GetSubsystem<UPTHostMigrationSubsystem>())
		{
			Migration->NotifyHostLeaving(bAllowMigration);
		}
	}
}
