// Copyright NeedsGames. All Rights Reserved.

#include "Core/PTGameMode.h"
#include "Core/PTGameState.h"
#include "Core/PTPlayerController.h"
#include "Core/PTPlayerState.h"
#include "Character/PTCharacter.h"
#include "AbilitySystem/PTAbilitySystemComponent.h"
#include "Online/PTHostMigrationSubsystem.h"
#include "Online/PTMigrationLibrary.h"
#include "Online/PTSessionSubsystem.h"
#include "PTGameplayTags.h"
#include "PTLogChannels.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameSession.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Guid.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PTGameMode)

APTGameMode::APTGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DefaultPawnClass = APTCharacter::StaticClass();
	PlayerControllerClass = APTPlayerController::StaticClass();
	PlayerStateClass = APTPlayerState::StaticClass();
	GameStateClass = APTGameState::StaticClass();
	bUseSeamlessTravel = true;
}

void APTGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	UGameInstance* GI = GetGameInstance();
	UPTHostMigrationSubsystem* Migration = GI ? GI->GetSubsystem<UPTHostMigrationSubsystem>() : nullptr;
	UPTSessionSubsystem* Sessions = GI ? GI->GetSubsystem<UPTSessionSubsystem>() : nullptr;

	if (Migration && Migration->ConsumePendingRestore(RestoreSnapshot))
	{
		bRestoringFromMigration = true;
		SessionLineageId = RestoreSnapshot.SessionLineageId;
		SessionEpoch = RestoreSnapshot.Epoch + 1;

		for (const FPTPlayerSnapshot& P : RestoreSnapshot.Players)
		{
			NextJoinOrder = FMath::Max(NextJoinOrder, P.JoinOrder + 1);
		}

		UE_LOG(LogPTMigration, Log, TEXT("GameMode: restoring from migration snapshot (lineage=%s, new epoch=%d, %d players, %d actors)"),
			*SessionLineageId, SessionEpoch, RestoreSnapshot.Players.Num(), RestoreSnapshot.Actors.Num());
	}
	else
	{
		SessionLineageId = Sessions ? Sessions->GetCurrentLineageId() : FString();
		SessionEpoch = Sessions ? Sessions->GetCurrentEpoch() : 0;
		if (SessionLineageId.IsEmpty())
		{
			// 세션 없이 listen 으로 연 경우 (에디터 PIE 등)
			SessionLineageId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		}
	}
}

void APTGameMode::StartPlay()
{
	Super::StartPlay();

	if (bRestoringFromMigration)
	{
		RestoreWorldFromSnapshot();
	}

	const UPTHostMigrationSettings* Settings = GetDefault<UPTHostMigrationSettings>();
	if (bEnableMigrationSnapshots && Settings->bEnableHostMigration && GetNetMode() == NM_ListenServer)
	{
		GetWorldTimerManager().SetTimer(SnapshotTimerHandle, this, &ThisClass::CaptureAndPublishSnapshot, Settings->SnapshotInterval, true, 0.5f);
	}
}

FString APTGameMode::InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId, const FString& Options, const FString& Portal)
{
	FString Error = Super::InitNewPlayer(NewPlayerController, UniqueId, Options, Portal);
	if (!Error.IsEmpty())
	{
		return Error;
	}

	APTPlayerState* PS = NewPlayerController ? NewPlayerController->GetPlayerState<APTPlayerState>() : nullptr;
	if (!PS)
	{
		return Error;
	}

	FString PlayerId = UGameplayStatics::ParseOption(Options, TEXT("PTPID"));
	if (PlayerId.IsEmpty() && NewPlayerController->IsLocalController())
	{
		if (UPTSessionSubsystem* Sessions = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTSessionSubsystem>() : nullptr)
		{
			PlayerId = Sessions->GetLocalPlayerId();
		}
	}
	if (PlayerId.IsEmpty())
	{
		PlayerId = UniqueId.IsValid() ? UniqueId.ToString() : FGuid::NewGuid().ToString(EGuidFormats::Digits);
	}
	PS->SetPTPlayerId(PlayerId);

	const FPTPlayerSnapshot* Restored = bRestoringFromMigration ? RestoreSnapshot.FindPlayer(PlayerId) : nullptr;
	PS->SetJoinOrder(Restored ? Restored->JoinOrder : NextJoinOrder++);

	return Error;
}

void APTGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	ScheduleSnapshotSoon();
}

void APTGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);
	ScheduleSnapshotSoon();
}

void APTGameMode::FinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation)
{
	Super::FinishRestartPlayer(NewPlayer, StartRotation);
	RestorePlayerFromSnapshot(NewPlayer);
}

void APTGameMode::ScheduleSnapshotSoon()
{
	if (GetNetMode() == NM_ListenServer)
	{
		// 접속/퇴장 직후 후보 목록이 바로 반영되도록
		GetWorldTimerManager().SetTimer(SnapshotSoonTimerHandle, this, &ThisClass::CaptureAndPublishSnapshot, 0.25f, false);
	}
}

// -------------------------------------------------------------------------------------------------
// Snapshot
// -------------------------------------------------------------------------------------------------

void APTGameMode::CaptureMigrationGlobalData_Implementation(TArray<FPTKeyValue>& OutData)
{
}

void APTGameMode::RestoreMigrationGlobalData_Implementation(const TArray<FPTKeyValue>& Data)
{
}

void APTGameMode::BuildSnapshot(FPTMigrationSnapshot& OutSnapshot)
{
	UGameInstance* GI = GetGameInstance();
	UPTSessionSubsystem* Sessions = GI ? GI->GetSubsystem<UPTSessionSubsystem>() : nullptr;

	OutSnapshot = FPTMigrationSnapshot();
	OutSnapshot.SessionLineageId = SessionLineageId;
	OutSnapshot.Epoch = SessionEpoch;
	OutSnapshot.HostPlayerId = Sessions ? Sessions->GetLocalPlayerId() : FString();
	OutSnapshot.MapPackageName = UPTMigrationLibrary::GetCurrentMapPackageName(this);
	OutSnapshot.MaxPlayers = (Sessions && Sessions->GetCurrentMaxPlayers() > 0) ? Sessions->GetCurrentMaxPlayers() : (GameSession ? GameSession->MaxPlayers : 4);
	OutSnapshot.bIsLAN = Sessions ? Sessions->IsLANSubsystem() : true;
	OutSnapshot.ServerWorldTime = GetWorld()->GetTimeSeconds();

	if (GameState)
	{
		for (APlayerState* RawPS : GameState->PlayerArray)
		{
			APTPlayerState* PS = Cast<APTPlayerState>(RawPS);
			if (!PS || PS->IsInactive() || PS->GetPTPlayerId().IsEmpty())
			{
				continue;
			}

			FPTPlayerSnapshot& P = OutSnapshot.Players.AddDefaulted_GetRef();
			P.PlayerId = PS->GetPTPlayerId();
			P.PlayerName = PS->GetPlayerName();
			P.JoinOrder = PS->GetJoinOrder();

			if (APawn* Pawn = PS->GetPawn())
			{
				P.bHasPawn = true;
				P.PawnTransform = Pawn->GetActorTransform();
				P.ControlRotation = Pawn->GetController() ? Pawn->GetController()->GetControlRotation() : Pawn->GetActorRotation();
				UPTMigrationLibrary::SaveGameProperties(Pawn, P.PawnSaveData);
			}

			UPTMigrationLibrary::CaptureAttributes(PS->GetPTAbilitySystemComponent(), P.Attributes);
			UPTMigrationLibrary::SaveGameProperties(PS, P.PlayerStateSaveData);
		}

		UPTMigrationLibrary::SaveGameProperties(GameState, OutSnapshot.GameStateSaveData);
	}

	UPTMigrationLibrary::CaptureMigratableActors(GetWorld(), OutSnapshot.Actors);
	CaptureMigrationGlobalData(OutSnapshot.GlobalData);
}

void APTGameMode::CaptureAndPublishSnapshot()
{
	if (!HasAuthority() || GetNetMode() != NM_ListenServer)
	{
		return;
	}

	APTGameState* PTGameState = GetGameState<APTGameState>();
	if (!PTGameState)
	{
		return;
	}

	FPTMigrationSnapshot Snapshot;
	BuildSnapshot(Snapshot);

	TArray<uint8> Compressed;
	int32 UncompressedSize = 0;
	if (UPTMigrationLibrary::SerializeSnapshot(Snapshot, Compressed, UncompressedSize))
	{
		if (Compressed.Num() > 48 * 1024)
		{
			UE_LOG(LogPTMigration, Warning, TEXT("Migration snapshot is large (%d bytes compressed). Consider reducing SaveGame data."), Compressed.Num());
		}
		PTGameState->SetMigrationPayload(MoveTemp(Compressed), UncompressedSize);
	}
}

// -------------------------------------------------------------------------------------------------
// Restore
// -------------------------------------------------------------------------------------------------

void APTGameMode::RestoreWorldFromSnapshot()
{
	UPTMigrationLibrary::RestoreMigratableActors(GetWorld(), RestoreSnapshot.Actors);

	if (GameState)
	{
		UPTMigrationLibrary::LoadGameProperties(GameState, RestoreSnapshot.GameStateSaveData);
	}

	RestoreMigrationGlobalData(RestoreSnapshot.GlobalData);
}

void APTGameMode::RestorePlayerFromSnapshot(AController* Controller)
{
	if (!bRestoringFromMigration || !Controller)
	{
		return;
	}

	APTPlayerState* PS = Controller->GetPlayerState<APTPlayerState>();
	if (!PS)
	{
		return;
	}

	const FString& PlayerId = PS->GetPTPlayerId();
	const FPTPlayerSnapshot* Entry = RestoreSnapshot.FindPlayer(PlayerId);
	if (!Entry || RestoredPlayerIds.Contains(PlayerId))
	{
		return;
	}
	RestoredPlayerIds.Add(PlayerId);

	APawn* Pawn = Controller->GetPawn();
	if (APTCharacter* Character = Cast<APTCharacter>(Pawn))
	{
		if (Entry->bHasPawn)
		{
			Character->TeleportForMigrationRestore(Entry->PawnTransform, Entry->ControlRotation);
		}
	}
	if (Pawn)
	{
		UPTMigrationLibrary::LoadGameProperties(Pawn, Entry->PawnSaveData);
	}

	UPTMigrationLibrary::LoadGameProperties(PS, Entry->PlayerStateSaveData);
	UPTMigrationLibrary::RestoreAttributes(PS->GetPTAbilitySystemComponent(), Entry->Attributes);

	if (Pawn)
	{
		FGameplayEventData Payload;
		Payload.EventTag = PTGameplayTags::Event_Migration_Restored;
		Payload.Target = Pawn;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Pawn, PTGameplayTags::Event_Migration_Restored, Payload);
	}

	UE_LOG(LogPTMigration, Log, TEXT("Restored player %s (%s)"), *Entry->PlayerName, *PlayerId);
	OnPlayerRestoredFromMigration(Controller);
}
