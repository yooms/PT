// Copyright NeedsGames. All Rights Reserved.

#include "Online/PTSessionSubsystem.h"
#include "Online/PTHostMigrationSubsystem.h"
#include "Online/PTMigrationLibrary.h"
#include "Online/PTMigrationTypes.h"
#include "PTLogChannels.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameMapsSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Guid.h"
#include "Misc/Parse.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PTSessionSubsystem)

void UPTSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 프로세스 단위 플레이어 ID. 호스트 인계는 같은 프로세스 안에서 일어나므로 영구 저장이 필요 없습니다.
	// 로컬 다중 실행 테스트 시 -PTPlayerId=xxx 로 고정할 수 있습니다.
	if (!FParse::Value(FCommandLine::Get(), TEXT("PTPlayerId="), LocalPlayerId) || LocalPlayerId.IsEmpty())
	{
		LocalPlayerId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	}
	UE_LOG(LogPTSession, Log, TEXT("Local PT PlayerId = %s"), *LocalPlayerId);
}

void UPTSessionSubsystem::Deinitialize()
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	}
	Super::Deinitialize();
}

IOnlineSessionPtr UPTSessionSubsystem::GetSessionInterface() const
{
	if (const UWorld* World = GetWorld())
	{
		return Online::GetSessionInterface(World);
	}
	IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
	return OSS ? OSS->GetSessionInterface() : nullptr;
}

bool UPTSessionSubsystem::IsLANSubsystem() const
{
	const IOnlineSubsystem* OSS = GetWorld() ? Online::GetSubsystem(GetWorld()) : IOnlineSubsystem::Get();
	return !OSS || OSS->GetSubsystemName() == FName(TEXT("NULL"));
}

bool UPTSessionSubsystem::HasActiveSession() const
{
	const IOnlineSessionPtr Sessions = GetSessionInterface();
	return Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession) != nullptr;
}

const FString& UPTSessionSubsystem::GetLocalPlayerId()
{
	if (LocalPlayerId.IsEmpty())
	{
		LocalPlayerId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	}
	return LocalPlayerId;
}

// -------------------------------------------------------------------------------------------------
// Host
// -------------------------------------------------------------------------------------------------

void UPTSessionSubsystem::HostSession(const FPTHostParams& Params)
{
	HostSessionNative(Params, nullptr);
}

void UPTSessionSubsystem::HostSessionNative(const FPTHostParams& Params, FOnComplete OnComplete)
{
	if (!GetSessionInterface().IsValid())
	{
		UE_LOG(LogPTSession, Error, TEXT("HostSession: no session interface"));
		if (OnComplete) { OnComplete(false); }
		OnCreateSessionComplete.Broadcast(false);
		return;
	}

	PendingHostParams = Params;
	if (PendingHostParams.LineageId.IsEmpty())
	{
		PendingHostParams.LineageId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	}
	if (PendingHostParams.MapPath.IsEmpty())
	{
		PendingHostParams.MapPath = UPTMigrationLibrary::GetCurrentMapPackageName(GetGameInstance());
	}
	PendingHostCallback = MoveTemp(OnComplete);

	if (HasActiveSession())
	{
		DestroySessionNative([this](bool) { CreateSessionInternal(); });
	}
	else
	{
		CreateSessionInternal();
	}
}

void UPTSessionSubsystem::CreateSessionInternal()
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid())
	{
		HandleCreateSessionComplete(NAME_GameSession, false);
		return;
	}

	const bool bLAN = IsLANSubsystem();

	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = FMath::Max(1, PendingHostParams.MaxPlayers);
	Settings.NumPrivateConnections = 0;
	Settings.bIsLANMatch = bLAN;
	Settings.bIsDedicated = false;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bAllowInvites = true;
	Settings.bUsesPresence = !bLAN;
	Settings.bAllowJoinViaPresence = !bLAN;
	Settings.bUseLobbiesIfAvailable = !bLAN;

	FString HostName = TEXT("Host");
	if (const ULocalPlayer* LP = GetGameInstance() ? GetGameInstance()->GetFirstGamePlayer() : nullptr)
	{
		HostName = LP->GetNickname();
	}

	const EOnlineDataAdvertisementType::Type Adv = EOnlineDataAdvertisementType::ViaOnlineServiceAndPing;
	Settings.Set(SETTING_MAPNAME, PendingHostParams.MapPath, Adv);
	Settings.Set(FName(PTSessionKeys::MapPath), PendingHostParams.MapPath, Adv);
	Settings.Set(FName(PTSessionKeys::Lineage), PendingHostParams.LineageId, Adv);
	Settings.Set(FName(PTSessionKeys::Epoch), PendingHostParams.Epoch, Adv);
	Settings.Set(FName(PTSessionKeys::HostRank), PendingHostParams.HostRank, Adv);
	Settings.Set(FName(PTSessionKeys::BuildId), UPTMigrationLibrary::GetBuildId(), Adv);
	Settings.Set(FName(PTSessionKeys::HostName), HostName, Adv);

	Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
	CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleCreateSessionComplete));

	UE_LOG(LogPTSession, Log, TEXT("CreateSession: map=%s lineage=%s epoch=%d rank=%d LAN=%d"),
		*PendingHostParams.MapPath, *PendingHostParams.LineageId, PendingHostParams.Epoch, PendingHostParams.HostRank, bLAN ? 1 : 0);

	if (!Sessions->CreateSession(0, NAME_GameSession, Settings))
	{
		HandleCreateSessionComplete(NAME_GameSession, false);
	}
}

void UPTSessionSubsystem::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
	}

	UE_LOG(LogPTSession, Log, TEXT("CreateSession complete: %s"), bWasSuccessful ? TEXT("OK") : TEXT("FAILED"));

	if (bWasSuccessful)
	{
		CurrentHostParams = PendingHostParams;
		if (PendingHostParams.bTravelToMap && !PendingHostParams.MapPath.IsEmpty())
		{
			UGameplayStatics::OpenLevel(GetGameInstance(), FName(*PendingHostParams.MapPath), true, TEXT("listen"));
		}
	}

	FOnComplete Callback = MoveTemp(PendingHostCallback);
	PendingHostCallback = nullptr;
	if (Callback) { Callback(bWasSuccessful); }
	OnCreateSessionComplete.Broadcast(bWasSuccessful);
}

// -------------------------------------------------------------------------------------------------
// Find
// -------------------------------------------------------------------------------------------------

void UPTSessionSubsystem::FindSessions(int32 MaxResults)
{
	FindSessionsNative(MaxResults, nullptr);
}

void UPTSessionSubsystem::FindSessionsNative(int32 MaxResults, FOnFindComplete OnComplete)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid() || SessionSearch.IsValid())
	{
		// 인터페이스 없음 또는 이미 검색 중
		if (OnComplete) { OnComplete(false, {}); }
		if (!SessionSearch.IsValid()) { OnSessionsFound.Broadcast(false, {}); }
		return;
	}

	PendingFindCallback = MoveTemp(OnComplete);

	SessionSearch = MakeShared<FOnlineSessionSearch>();
	SessionSearch->MaxSearchResults = FMath::Max(1, MaxResults);
	SessionSearch->bIsLanQuery = IsLANSubsystem();
	if (!SessionSearch->bIsLanQuery)
	{
		SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	}

	Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
	FindHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::HandleFindSessionsComplete));

	if (!Sessions->FindSessions(0, SessionSearch.ToSharedRef()))
	{
		HandleFindSessionsComplete(false);
	}
}

void UPTSessionSubsystem::HandleFindSessionsComplete(bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
	}

	LastSearchResults = SessionSearch.IsValid() ? SessionSearch->SearchResults : TArray<FOnlineSessionSearchResult>();
	SessionSearch.Reset();

	UE_LOG(LogPTSession, Verbose, TEXT("FindSessions complete: %s, %d result(s)"), bWasSuccessful ? TEXT("OK") : TEXT("FAILED"), LastSearchResults.Num());

	FOnFindComplete Callback = MoveTemp(PendingFindCallback);
	PendingFindCallback = nullptr;
	if (Callback) { Callback(bWasSuccessful, LastSearchResults); }

	TArray<FPTSessionInfo> Infos;
	for (int32 i = 0; i < LastSearchResults.Num(); ++i)
	{
		Infos.Add(MakeSessionInfo(LastSearchResults[i], i));
	}
	OnSessionsFound.Broadcast(bWasSuccessful, Infos);
}

FPTSessionInfo UPTSessionSubsystem::MakeSessionInfo(const FOnlineSessionSearchResult& Result, int32 Index)
{
	FPTSessionInfo Info;
	Info.SearchIndex = Index;
	Info.PingMs = Result.PingInMs;
	Info.MaxPlayers = Result.Session.SessionSettings.NumPublicConnections;
	Info.CurrentPlayers = FMath::Max(0, Info.MaxPlayers - Result.Session.NumOpenPublicConnections);

	const FOnlineSessionSettings& S = Result.Session.SessionSettings;
	if (!S.Get(FName(PTSessionKeys::HostName), Info.HostName) || Info.HostName.IsEmpty())
	{
		Info.HostName = Result.Session.OwningUserName;
	}
	S.Get(FName(PTSessionKeys::MapPath), Info.MapPath);
	S.Get(FName(PTSessionKeys::Lineage), Info.LineageId);
	S.Get(FName(PTSessionKeys::Epoch), Info.Epoch);
	S.Get(FName(PTSessionKeys::HostRank), Info.HostRank);

	FString BuildId;
	if (S.Get(FName(PTSessionKeys::BuildId), BuildId))
	{
		Info.bBuildCompatible = (BuildId == UPTMigrationLibrary::GetBuildId());
	}
	return Info;
}

// -------------------------------------------------------------------------------------------------
// Join
// -------------------------------------------------------------------------------------------------

void UPTSessionSubsystem::JoinSessionByIndex(int32 SearchIndex)
{
	if (!LastSearchResults.IsValidIndex(SearchIndex))
	{
		UE_LOG(LogPTSession, Warning, TEXT("JoinSessionByIndex: invalid index %d"), SearchIndex);
		OnJoinSessionComplete.Broadcast(false);
		return;
	}
	JoinSessionNative(LastSearchResults[SearchIndex], nullptr);
}

void UPTSessionSubsystem::JoinSessionNative(const FOnlineSessionSearchResult& Result, FOnComplete OnComplete)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid() || !Result.IsValid())
	{
		if (OnComplete) { OnComplete(false); }
		OnJoinSessionComplete.Broadcast(false);
		return;
	}

	PendingJoinResult = Result;
	PendingJoinCallback = MoveTemp(OnComplete);

	auto DoJoin = [this]()
	{
		IOnlineSessionPtr SessionInt = GetSessionInterface();
		if (!SessionInt.IsValid())
		{
			HandleJoinSessionComplete(NAME_GameSession, EOnJoinSessionCompleteResult::UnknownError);
			return;
		}

		SessionInt->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
		JoinHandle = SessionInt->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleJoinSessionComplete));

		if (!SessionInt->JoinSession(0, NAME_GameSession, PendingJoinResult))
		{
			HandleJoinSessionComplete(NAME_GameSession, EOnJoinSessionCompleteResult::UnknownError);
		}
	};

	if (HasActiveSession())
	{
		DestroySessionNative([DoJoin](bool) { DoJoin(); });
	}
	else
	{
		DoJoin();
	}
}

void UPTSessionSubsystem::HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
	}

	const bool bSuccess = (Result == EOnJoinSessionCompleteResult::Success);
	UE_LOG(LogPTSession, Log, TEXT("JoinSession complete: %s (%d)"), bSuccess ? TEXT("OK") : TEXT("FAILED"), (int32)Result);

	if (bSuccess)
	{
		TravelToSession(PendingJoinResult);
	}

	FOnComplete Callback = MoveTemp(PendingJoinCallback);
	PendingJoinCallback = nullptr;
	if (Callback) { Callback(bSuccess); }
	OnJoinSessionComplete.Broadcast(bSuccess);
}

void UPTSessionSubsystem::TravelToSession(const FOnlineSessionSearchResult& Result)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	FString ConnectString;
	if (!Sessions.IsValid() || !Sessions->GetResolvedConnectString(NAME_GameSession, ConnectString))
	{
		UE_LOG(LogPTSession, Error, TEXT("TravelToSession: could not resolve connect string"));
		return;
	}

	ConnectString += FString::Printf(TEXT("?PTPID=%s"), *GetLocalPlayerId());

	UE_LOG(LogPTSession, Log, TEXT("ClientTravel -> %s"), *ConnectString);
	if (APlayerController* PC = GetGameInstance()->GetFirstLocalPlayerController())
	{
		PC->ClientTravel(ConnectString, TRAVEL_Absolute);
	}
	else if (GEngine && GetWorld())
	{
		// 로컬 PlayerController 가 아직 없는 경우 (맵 전환 직후 등)
		GEngine->SetClientTravel(GetWorld(), *ConnectString, TRAVEL_Absolute);
	}
}

// -------------------------------------------------------------------------------------------------
// Destroy / Leave
// -------------------------------------------------------------------------------------------------

void UPTSessionSubsystem::DestroySessionNative(FOnComplete OnComplete)
{
	IOnlineSessionPtr Sessions = GetSessionInterface();
	if (!Sessions.IsValid() || !Sessions->GetNamedSession(NAME_GameSession))
	{
		if (OnComplete) { OnComplete(true); }
		return;
	}

	if (OnComplete)
	{
		PendingDestroyCallbacks.Add(MoveTemp(OnComplete));
	}

	if (bDestroyInProgress)
	{
		return;
	}
	bDestroyInProgress = true;

	Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleDestroySessionComplete));

	if (!Sessions->DestroySession(NAME_GameSession))
	{
		HandleDestroySessionComplete(NAME_GameSession, false);
	}
}

void UPTSessionSubsystem::HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (IOnlineSessionPtr Sessions = GetSessionInterface())
	{
		Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	}
	bDestroyInProgress = false;

	UE_LOG(LogPTSession, Log, TEXT("DestroySession complete: %s"), bWasSuccessful ? TEXT("OK") : TEXT("FAILED"));

	TArray<FOnComplete> Callbacks = MoveTemp(PendingDestroyCallbacks);
	PendingDestroyCallbacks.Reset();
	for (FOnComplete& Callback : Callbacks)
	{
		if (Callback) { Callback(bWasSuccessful); }
	}
}

void UPTSessionSubsystem::LeaveSessionAndReturnToMenu()
{
	if (UPTHostMigrationSubsystem* Migration = GetGameInstance()->GetSubsystem<UPTHostMigrationSubsystem>())
	{
		Migration->MarkIntentionalLeave();
	}

	CurrentHostParams = FPTHostParams();
	TWeakObjectPtr<UPTSessionSubsystem> WeakThis(this);
	DestroySessionNative([WeakThis](bool)
	{
		if (UPTSessionSubsystem* This = WeakThis.Get())
		{
			UGameplayStatics::OpenLevel(This->GetGameInstance(), FName(*UGameMapsSettings::GetGameDefaultMap()), true);
		}
	});
}
