// Copyright yoomsGames. All Rights Reserved.

#include "Online/PTHostMigrationSubsystem.h"
#include "Online/PTMigrationLibrary.h"
#include "Online/PTSessionSubsystem.h"
#include "Core/PTPlayerState.h"
#include "AbilitySystem/PTAbilitySystemComponent.h"
#include "PTLogChannels.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "OnlineSessionSettings.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PTHostMigrationSubsystem)

#define LOCTEXT_NAMESPACE "PTHostMigration"

void UPTHostMigrationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UPTSessionSubsystem>();
	Super::Initialize(Collection);

	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &ThisClass::HandleNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &ThisClass::HandleTravelFailure);
	}
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);
}

void UPTHostMigrationSubsystem::Deinitialize()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);

	if (FTimerManager* TM = GetTimerManager())
	{
		TM->ClearAllTimersForObject(this);
	}
	Overlay.Hide();

	Super::Deinitialize();
}

UPTSessionSubsystem* UPTHostMigrationSubsystem::GetSessionSubsystem() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTSessionSubsystem>() : nullptr;
}

FTimerManager* UPTHostMigrationSubsystem::GetTimerManager() const
{
	// GameInstance 의 TimerManager 는 맵 전환에도 유지됨
	return GetGameInstance() ? &GetGameInstance()->GetTimerManager() : nullptr;
}

double UPTHostMigrationSubsystem::GetElapsed() const
{
	return FPlatformTime::Seconds() - MigrationStartTime;
}

// -------------------------------------------------------------------------------------------------
// 입력 이벤트
// -------------------------------------------------------------------------------------------------

void UPTHostMigrationSubsystem::OnSnapshotReplicated(const FPTReplicatedMigrationData& Data)
{
	if (Data.Revision == LastRevision && LastSnapshot.IsValid())
	{
		return;
	}

	TArray<uint8> Compressed;
	if (!Data.GetPayload(Compressed))
	{
		// 일부 청크만 도착 - 다음 OnRep 에서 다시 시도
		return;
	}

	FPTMigrationSnapshot Snapshot;
	if (UPTMigrationLibrary::DeserializeSnapshot(Compressed, Data.UncompressedSize, Snapshot))
	{
		LastSnapshot = MoveTemp(Snapshot);
		LastRevision = Data.Revision;
		UE_LOG(LogPTMigration, Verbose, TEXT("Snapshot rev %d received (%d players, %d actors, %d bytes)"),
			Data.Revision, LastSnapshot.Players.Num(), LastSnapshot.Actors.Num(), Compressed.Num());
	}
}

void UPTHostMigrationSubsystem::NotifyHostLeaving(bool bAllowMigration)
{
	UE_LOG(LogPTMigration, Log, TEXT("Host announced leaving (allow migration: %d)"), bAllowMigration ? 1 : 0);
	bHostAnnouncedLeave = true;
	bHostAllowsMigration = bAllowMigration;
}

void UPTHostMigrationSubsystem::MarkIntentionalLeave()
{
	bIntentionalLeave = true;
	if (State != EPTMigrationState::Idle && State != EPTMigrationState::Completed && State != EPTMigrationState::Failed)
	{
		if (FTimerManager* TM = GetTimerManager())
		{
			TM->ClearAllTimersForObject(this);
		}
		Overlay.Hide();
		SetState(EPTMigrationState::Idle);
	}
}

void UPTHostMigrationSubsystem::NotifyKicked()
{
	bWasKicked = true;
}

bool UPTHostMigrationSubsystem::ConsumePendingRestore(FPTMigrationSnapshot& OutSnapshot)
{
	if (!bHasPendingRestore)
	{
		return false;
	}
	OutSnapshot = PendingRestore;
	PendingRestore = FPTMigrationSnapshot();
	bHasPendingRestore = false;
	return true;
}

void UPTHostMigrationSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
	UE_LOG(LogPTMigration, Log, TEXT("NetworkFailure: %s (%s) state=%d"), ENetworkFailure::ToString(FailureType), *ErrorString, (int32)State);

	// 새 호스트 접속 실패 -> 다시 검색
	if (State == EPTMigrationState::Joining)
	{
		StartSearching();
		return;
	}

	if (State != EPTMigrationState::Idle && State != EPTMigrationState::Completed && State != EPTMigrationState::Failed)
	{
		return;
	}

	const UPTHostMigrationSettings* Settings = GetDefault<UPTHostMigrationSettings>();
	if (!Settings->bEnableHostMigration)
	{
		return;
	}

	if (!World || World->GetNetMode() != NM_Client || !NetDriver || NetDriver->NetDriverName != NAME_GameNetDriver)
	{
		return;
	}

	if (bIntentionalLeave || bWasKicked || (bHostAnnouncedLeave && !bHostAllowsMigration))
	{
		return;
	}

	switch (FailureType)
	{
	case ENetworkFailure::ConnectionLost:
	case ENetworkFailure::ConnectionTimeout:
	case ENetworkFailure::FailureReceived:
		break;
	default:
		// 버전 불일치 등은 인계하지 않음
		return;
	}

	if (!LastSnapshot.IsValid())
	{
		UE_LOG(LogPTMigration, Warning, TEXT("Host lost but no valid snapshot - cannot migrate"));
		return;
	}

	BeginMigration(World);
}

void UPTHostMigrationSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
	UE_LOG(LogPTMigration, Log, TEXT("TravelFailure: %s (%s)"), ETravelFailure::ToString(FailureType), *ErrorString);

	if (State == EPTMigrationState::Joining)
	{
		StartSearching();
	}
	else if (State == EPTMigrationState::Promoting)
	{
		FailMigration(TEXT("Failed to open map as new host"));
	}
}

void UPTHostMigrationSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!LoadedWorld || LoadedWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	Overlay.Reattach();

	const ENetMode NetMode = LoadedWorld->GetNetMode();

	switch (State)
	{
	case EPTMigrationState::WaitingForDisconnectTravel:
		if (FTimerManager* TM = GetTimerManager())
		{
			TM->ClearTimer(TravelSafetyTimerHandle);
		}
		StartMigrationFlow();
		break;

	case EPTMigrationState::Promoting:
		if (NetMode == NM_ListenServer)
		{
			CompleteMigration();
			if (LocalRank > 0)
			{
				if (FTimerManager* TM = GetTimerManager())
				{
					TM->SetTimer(RedundantHostTimerHandle, this, &ThisClass::CheckRedundantHost, GetDefault<UPTHostMigrationSettings>()->RedundantHostCheckDelay, false);
				}
			}
		}
		break;

	case EPTMigrationState::Joining:
		if (NetMode == NM_Client)
		{
			CompleteMigration();
		}
		break;

	case EPTMigrationState::Idle:
	case EPTMigrationState::Completed:
	case EPTMigrationState::Failed:
		ResetTransientFlags();
		// 새 게임 세션에 들어왔으면 이전 스냅샷은 폐기
		if (NetMode != NM_Client)
		{
			LastSnapshot = FPTMigrationSnapshot();
			LastRevision = -1;
		}
		break;

	default:
		break;
	}
}

// -------------------------------------------------------------------------------------------------
// 인계 흐름
// -------------------------------------------------------------------------------------------------

void UPTHostMigrationSubsystem::BeginMigration(UWorld* FailedWorld)
{
	UPTSessionSubsystem* Sessions = GetSessionSubsystem();
	if (!Sessions)
	{
		return;
	}

	ActiveSnapshot = LastSnapshot;
	CaptureLiveWorldState(FailedWorld);

	const TArray<FString> Successors = ActiveSnapshot.GetSuccessorOrder();
	NumCandidates = Successors.Num();
	LocalRank = Successors.IndexOfByKey(Sessions->GetLocalPlayerId());

	UE_LOG(LogPTMigration, Log, TEXT("=== Host migration begin: lineage=%s epoch=%d candidates=%d localRank=%d ==="),
		*ActiveSnapshot.SessionLineageId, ActiveSnapshot.Epoch, NumCandidates, LocalRank);

	if (LocalRank == INDEX_NONE)
	{
		FailMigration(TEXT("Local player not found in host snapshot"));
		return;
	}

	MigrationStartTime = FPlatformTime::Seconds();
	FailureReason.Reset();
	SetState(EPTMigrationState::WaitingForDisconnectTravel);

	Overlay.Show(GetGameInstance(),
		MakeAttributeUObject(this, &ThisClass::GetOverlayTitle),
		MakeAttributeUObject(this, &ThisClass::GetOverlayDetail),
		TAttribute<TOptional<float>>(TOptional<float>()));

	if (FTimerManager* TM = GetTimerManager())
	{
		const UPTHostMigrationSettings* Settings = GetDefault<UPTHostMigrationSettings>();
		const float Timeout = Settings->MigrationTimeout + LocalRank * Settings->CandidatePromotionDelay;
		TM->SetTimer(TimeoutTimerHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (State != EPTMigrationState::Completed && State != EPTMigrationState::Idle)
			{
				FailMigration(TEXT("Timed out"));
			}
		}), Timeout, false);

		// 엔진이 기본 맵으로 이동하지 않는 경우 대비
		TM->SetTimer(TravelSafetyTimerHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (State == EPTMigrationState::WaitingForDisconnectTravel)
			{
				StartMigrationFlow();
			}
		}), 5.f, false);
	}
}

void UPTHostMigrationSubsystem::CaptureLiveWorldState(UWorld* World)
{
	// 스냅샷 이후 변경된 플레이어 위치/어트리뷰트를 클라이언트가 가진 최신 복제 상태로 갱신
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		return;
	}

	for (APlayerState* PS : GameState->PlayerArray)
	{
		const APTPlayerState* PTPS = Cast<APTPlayerState>(PS);
		if (!PTPS || PTPS->GetPTPlayerId().IsEmpty())
		{
			continue;
		}

		FPTPlayerSnapshot* Entry = ActiveSnapshot.Players.FindByPredicate([PTPS](const FPTPlayerSnapshot& P) { return P.PlayerId == PTPS->GetPTPlayerId(); });
		if (!Entry)
		{
			continue;
		}

		if (const APawn* Pawn = PTPS->GetPawn())
		{
			Entry->bHasPawn = true;
			Entry->PawnTransform = Pawn->GetActorTransform();
			Entry->ControlRotation = Pawn->IsLocallyControlled() ? Pawn->GetControlRotation() : Pawn->GetBaseAimRotation();
		}

		if (const UPTAbilitySystemComponent* ASC = PTPS->GetPTAbilitySystemComponent())
		{
			TArray<FPTAttributeSnapshot> LiveAttributes;
			UPTMigrationLibrary::CaptureAttributes(ASC, LiveAttributes);
			if (LiveAttributes.Num() > 0)
			{
				Entry->Attributes = MoveTemp(LiveAttributes);
			}
		}
	}
}

void UPTHostMigrationSubsystem::StartMigrationFlow()
{
	UPTSessionSubsystem* Sessions = GetSessionSubsystem();
	if (!Sessions)
	{
		FailMigration(TEXT("No session subsystem"));
		return;
	}

	// 클라이언트 측에 남아있는 이전 세션 정리 후 진행
	TWeakObjectPtr<UPTHostMigrationSubsystem> WeakThis(this);
	Sessions->DestroySessionNative([WeakThis](bool)
	{
		UPTHostMigrationSubsystem* This = WeakThis.Get();
		if (!This || This->State != EPTMigrationState::WaitingForDisconnectTravel)
		{
			return;
		}

		if (This->LocalRank == 0)
		{
			This->PromoteSelf();
		}
		else
		{
			This->StartSearching();
		}
	});
}

void UPTHostMigrationSubsystem::PromoteSelf()
{
	UPTSessionSubsystem* Sessions = GetSessionSubsystem();
	if (!Sessions)
	{
		FailMigration(TEXT("No session subsystem"));
		return;
	}

	if (FTimerManager* TM = GetTimerManager())
	{
		TM->ClearTimer(SearchTimerHandle);
	}

	UE_LOG(LogPTMigration, Log, TEXT("Promoting self to host (rank %d) -> %s"), LocalRank, *ActiveSnapshot.MapPackageName);
	SetState(EPTMigrationState::Promoting);

	PendingRestore = ActiveSnapshot;
	bHasPendingRestore = true;

	FPTHostParams Params;
	Params.MapPath = ActiveSnapshot.MapPackageName;
	Params.MaxPlayers = ActiveSnapshot.MaxPlayers;
	Params.LineageId = ActiveSnapshot.SessionLineageId;
	Params.Epoch = ActiveSnapshot.Epoch + 1;
	Params.HostRank = LocalRank;
	Params.bTravelToMap = true;

	TWeakObjectPtr<UPTHostMigrationSubsystem> WeakThis(this);
	Sessions->HostSessionNative(Params, [WeakThis](bool bSuccess)
	{
		if (UPTHostMigrationSubsystem* This = WeakThis.Get())
		{
			if (!bSuccess)
			{
				This->bHasPendingRestore = false;
				This->FailMigration(TEXT("Failed to create session as new host"));
			}
		}
	});
}

void UPTHostMigrationSubsystem::StartSearching()
{
	SetState(EPTMigrationState::Searching);
	if (FTimerManager* TM = GetTimerManager())
	{
		TM->SetTimer(SearchTimerHandle, this, &ThisClass::DoSearch, 0.25f, false);
	}
}

void UPTHostMigrationSubsystem::DoSearch()
{
	if (State != EPTMigrationState::Searching)
	{
		return;
	}

	UPTSessionSubsystem* Sessions = GetSessionSubsystem();
	if (!Sessions)
	{
		FailMigration(TEXT("No session subsystem"));
		return;
	}

	TWeakObjectPtr<UPTHostMigrationSubsystem> WeakThis(this);
	Sessions->FindSessionsNative(50, [WeakThis](bool bSuccess, const TArray<FOnlineSessionSearchResult>& Results)
	{
		if (UPTHostMigrationSubsystem* This = WeakThis.Get())
		{
			This->HandleSearchResults(bSuccess, Results);
		}
	});
}

bool UPTHostMigrationSubsystem::GetMigrationSessionRank(const FOnlineSessionSearchResult& Result, int32 MinEpoch, int32& OutRank, int32& OutEpoch) const
{
	const FPTSessionInfo Info = UPTSessionSubsystem::MakeSessionInfo(Result, INDEX_NONE);
	if (Info.LineageId != ActiveSnapshot.SessionLineageId || Info.Epoch < MinEpoch || !Info.bBuildCompatible)
	{
		return false;
	}
	OutRank = Info.HostRank;
	OutEpoch = Info.Epoch;
	return true;
}

void UPTHostMigrationSubsystem::HandleSearchResults(bool bSuccess, const TArray<FOnlineSessionSearchResult>& Results)
{
	if (State != EPTMigrationState::Searching)
	{
		return;
	}

	// 새 호스트 세션 중 가장 높은 Epoch, 같은 Epoch 이면 가장 높은 순위(작은 Rank)를 선택
	const FOnlineSessionSearchResult* Best = nullptr;
	int32 BestRank = MAX_int32;
	int32 BestEpoch = -1;
	for (const FOnlineSessionSearchResult& Result : Results)
	{
		int32 Rank = 0, Epoch = 0;
		if (GetMigrationSessionRank(Result, ActiveSnapshot.Epoch + 1, Rank, Epoch))
		{
			if (Epoch > BestEpoch || (Epoch == BestEpoch && Rank < BestRank))
			{
				Best = &Result;
				BestRank = Rank;
				BestEpoch = Epoch;
			}
		}
	}

	if (Best)
	{
		UE_LOG(LogPTMigration, Log, TEXT("Found new host session (epoch %d, rank %d)"), BestEpoch, BestRank);
		JoinNewHost(*Best);
		return;
	}

	const UPTHostMigrationSettings* Settings = GetDefault<UPTHostMigrationSettings>();
	const double Elapsed = GetElapsed();

	if (LocalRank > 0 && Elapsed >= LocalRank * Settings->CandidatePromotionDelay)
	{
		UE_LOG(LogPTMigration, Log, TEXT("Higher-ranked candidates did not host within %.1fs - promoting self"), Elapsed);
		PromoteSelf();
		return;
	}

	if (FTimerManager* TM = GetTimerManager())
	{
		TM->SetTimer(SearchTimerHandle, this, &ThisClass::DoSearch, Settings->SearchInterval, false);
	}
}

void UPTHostMigrationSubsystem::JoinNewHost(const FOnlineSessionSearchResult& Result)
{
	UPTSessionSubsystem* Sessions = GetSessionSubsystem();
	if (!Sessions)
	{
		FailMigration(TEXT("No session subsystem"));
		return;
	}

	SetState(EPTMigrationState::Joining);

	TWeakObjectPtr<UPTHostMigrationSubsystem> WeakThis(this);
	Sessions->JoinSessionNative(Result, [WeakThis](bool bSuccess)
	{
		UPTHostMigrationSubsystem* This = WeakThis.Get();
		if (This && !bSuccess && This->State == EPTMigrationState::Joining)
		{
			This->StartSearching();
		}
	});
}

void UPTHostMigrationSubsystem::CheckRedundantHost()
{
	// 하위 순위인데 아무도 들어오지 않았다면, 상위 순위 호스트가 따로 존재하는지 확인
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState || World->GetNetMode() != NM_ListenServer || GameState->PlayerArray.Num() > 1)
	{
		return;
	}

	UPTSessionSubsystem* Sessions = GetSessionSubsystem();
	if (!Sessions)
	{
		return;
	}

	const int32 MyEpoch = Sessions->GetCurrentEpoch();
	const int32 MyRank = LocalRank;

	TWeakObjectPtr<UPTHostMigrationSubsystem> WeakThis(this);
	Sessions->FindSessionsNative(50, [WeakThis, MyEpoch, MyRank](bool bSuccess, const TArray<FOnlineSessionSearchResult>& Results)
	{
		UPTHostMigrationSubsystem* This = WeakThis.Get();
		if (!This)
		{
			return;
		}

		for (const FOnlineSessionSearchResult& Result : Results)
		{
			int32 Rank = 0, Epoch = 0;
			if (This->GetMigrationSessionRank(Result, MyEpoch, Rank, Epoch) && (Epoch > MyEpoch || Rank < MyRank))
			{
				UE_LOG(LogPTMigration, Log, TEXT("Redundant host detected - joining higher-ranked host (epoch %d, rank %d)"), Epoch, Rank);
				This->Overlay.Show(This->GetGameInstance(),
					MakeAttributeUObject(This, &ThisClass::GetOverlayTitle),
					MakeAttributeUObject(This, &ThisClass::GetOverlayDetail),
					TAttribute<TOptional<float>>(TOptional<float>()));
				This->JoinNewHost(Result);
				return;
			}
		}
	});
}

void UPTHostMigrationSubsystem::CompleteMigration()
{
	UE_LOG(LogPTMigration, Log, TEXT("=== Host migration complete (%.1fs, state=%d) ==="), GetElapsed(), (int32)State);

	if (FTimerManager* TM = GetTimerManager())
	{
		TM->ClearTimer(SearchTimerHandle);
		TM->ClearTimer(TimeoutTimerHandle);
		TM->ClearTimer(TravelSafetyTimerHandle);
	}

	Overlay.Hide();
	ResetTransientFlags();
	SetState(EPTMigrationState::Completed);
}

void UPTHostMigrationSubsystem::FailMigration(const FString& Reason)
{
	UE_LOG(LogPTMigration, Warning, TEXT("=== Host migration FAILED: %s ==="), *Reason);
	FailureReason = Reason;
	bHasPendingRestore = false;

	if (FTimerManager* TM = GetTimerManager())
	{
		TM->ClearTimer(SearchTimerHandle);
		TM->ClearTimer(TimeoutTimerHandle);
		TM->ClearTimer(TravelSafetyTimerHandle);
	}

	SetState(EPTMigrationState::Failed);
	ResetTransientFlags();

	// 실패 메시지를 잠시 보여준 뒤 숨김 (이미 기본/메뉴 맵에 있음)
	if (!Overlay.IsShown())
	{
		Overlay.Show(GetGameInstance(),
			MakeAttributeUObject(this, &ThisClass::GetOverlayTitle),
			MakeAttributeUObject(this, &ThisClass::GetOverlayDetail),
			TAttribute<TOptional<float>>(TOptional<float>(1.f)));
	}

	if (FTimerManager* TM = GetTimerManager())
	{
		TM->SetTimer(FailedOverlayTimerHandle, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			Overlay.Hide();
			if (State == EPTMigrationState::Failed)
			{
				SetState(EPTMigrationState::Idle);
			}
		}), 4.f, false);
	}
}

void UPTHostMigrationSubsystem::DebugForceMigration()
{
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World || World->GetNetMode() != NM_Client || !LastSnapshot.IsValid())
	{
		UE_LOG(LogPTMigration, Warning, TEXT("DebugForceMigration: must be a connected client with a valid snapshot"));
		return;
	}

	// 실제 연결을 끊어 엔진의 일반 경로(NetworkFailure)를 타게 함
	if (UNetDriver* Driver = World->GetNetDriver())
	{
		if (UNetConnection* Conn = Driver->ServerConnection)
		{
			Conn->Close();
		}
	}
}

void UPTHostMigrationSubsystem::SetState(EPTMigrationState NewState)
{
	if (State != NewState)
	{
		UE_LOG(LogPTMigration, Verbose, TEXT("Migration state %d -> %d"), (int32)State, (int32)NewState);
		State = NewState;
		OnMigrationStateChanged.Broadcast(NewState);
	}
}

void UPTHostMigrationSubsystem::ResetTransientFlags()
{
	bHostAnnouncedLeave = false;
	bHostAllowsMigration = true;
	bIntentionalLeave = false;
	bWasKicked = false;
}

FText UPTHostMigrationSubsystem::GetOverlayTitle() const
{
	switch (State)
	{
	case EPTMigrationState::Promoting: return LOCTEXT("TitlePromoting", "Becoming the new host");
	case EPTMigrationState::Joining:   return LOCTEXT("TitleJoining", "Connecting to the new host");
	case EPTMigrationState::Failed:    return LOCTEXT("TitleFailed", "Host migration failed");
	default:                           return LOCTEXT("TitleLost", "Host disconnected");
	}
}

FText UPTHostMigrationSubsystem::GetOverlayDetail() const
{
	switch (State)
	{
	case EPTMigrationState::Failed:
		return FText::FromString(FailureReason);
	case EPTMigrationState::Searching:
		return FText::Format(LOCTEXT("DetailSearching", "Searching for the new host... ({0}s)\nYou are candidate #{1} of {2}"),
			FText::AsNumber(FMath::FloorToInt(GetElapsed())), FText::AsNumber(LocalRank + 1), FText::AsNumber(NumCandidates));
	case EPTMigrationState::Promoting:
		return LOCTEXT("DetailPromoting", "Restoring the game world. Other players will reconnect shortly.");
	case EPTMigrationState::Joining:
		return LOCTEXT("DetailJoining", "Restoring your character...");
	default:
		return LOCTEXT("DetailWaiting", "Preparing host migration...");
	}
}

#undef LOCTEXT_NAMESPACE
