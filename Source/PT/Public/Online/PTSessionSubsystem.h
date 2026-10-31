// Copyright yoomsGames. All Rights Reserved.

#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "PTSessionSubsystem.generated.h"

/** BP 용 세션 검색 결과 요약 */
USTRUCT(BlueprintType)
struct FPTSessionInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "PT|Session")
	int32 SearchIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Session")
	FString HostName;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Session")
	FString MapPath;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Session")
	int32 CurrentPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Session")
	int32 MaxPlayers = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Session")
	int32 PingMs = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Session")
	FString LineageId;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Session")
	int32 Epoch = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Session")
	int32 HostRank = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PT|Session")
	bool bBuildCompatible = true;
};

/** 호스팅 파라미터 */
USTRUCT(BlueprintType)
struct FPTHostParams
{
	GENERATED_BODY()

	/** 롱 패키지 경로 (예: /Game/PT/Maps/L_Stage01) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PT|Session")
	FString MapPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PT|Session", meta = (ClampMin = "1", ClampMax = "64"))
	int32 MaxPlayers = 4;

	/** 비워두면 새 계보 ID 생성 (호스트 인계 시 기존 ID 유지) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PT|Session")
	FString LineageId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PT|Session")
	int32 Epoch = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PT|Session")
	int32 HostRank = 0;

	/** 세션 생성 후 자동으로 맵을 listen 으로 엶 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PT|Session")
	bool bTravelToMap = true;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPTOnSessionOperationComplete, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPTOnSessionsFound, bool, bWasSuccessful, const TArray<FPTSessionInfo>&, Sessions);

/**
 * 리슨 서버 세션 관리 (OnlineSubsystem 추상화: Null / Steam / EOS).
 *  - Host / Find / Join / Destroy
 *  - 로컬 플레이어 ID (호스트 인계 시 플레이어 매칭에 사용)
 */
UCLASS()
class PT_API UPTSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	using FOnFindComplete = TFunction<void(bool /*bSuccess*/, const TArray<FOnlineSessionSearchResult>& /*Results*/)>;
	using FOnComplete = TFunction<void(bool /*bSuccess*/)>;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ---------------- BP API ----------------
	UFUNCTION(BlueprintCallable, Category = "PT|Session")
	void HostSession(const FPTHostParams& Params);

	UFUNCTION(BlueprintCallable, Category = "PT|Session")
	void FindSessions(int32 MaxResults = 50);

	/** 마지막 FindSessions 결과의 인덱스로 참가 */
	UFUNCTION(BlueprintCallable, Category = "PT|Session")
	void JoinSessionByIndex(int32 SearchIndex);

	/** 세션 종료 후 기본(메뉴) 맵으로 이동 */
	UFUNCTION(BlueprintCallable, Category = "PT|Session")
	void LeaveSessionAndReturnToMenu();

	UFUNCTION(BlueprintPure, Category = "PT|Session")
	bool IsLANSubsystem() const;

	UFUNCTION(BlueprintPure, Category = "PT|Session")
	bool HasActiveSession() const;

	UFUNCTION(BlueprintPure, Category = "PT|Session")
	const FString& GetLocalPlayerId();

	UFUNCTION(BlueprintPure, Category = "PT|Session")
	FString GetCurrentLineageId() const { return CurrentHostParams.LineageId; }

	UFUNCTION(BlueprintPure, Category = "PT|Session")
	int32 GetCurrentEpoch() const { return CurrentHostParams.Epoch; }

	UFUNCTION(BlueprintPure, Category = "PT|Session")
	int32 GetCurrentMaxPlayers() const { return CurrentHostParams.MaxPlayers; }

	UPROPERTY(BlueprintAssignable, Category = "PT|Session")
	FPTOnSessionOperationComplete OnCreateSessionComplete;

	UPROPERTY(BlueprintAssignable, Category = "PT|Session")
	FPTOnSessionsFound OnSessionsFound;

	UPROPERTY(BlueprintAssignable, Category = "PT|Session")
	FPTOnSessionOperationComplete OnJoinSessionComplete;

	// ---------------- Native API (호스트 인계에서 사용) ----------------
	void HostSessionNative(const FPTHostParams& Params, FOnComplete OnComplete);
	void FindSessionsNative(int32 MaxResults, FOnFindComplete OnComplete);
	void JoinSessionNative(const FOnlineSessionSearchResult& Result, FOnComplete OnComplete);
	void DestroySessionNative(FOnComplete OnComplete);

	static FPTSessionInfo MakeSessionInfo(const FOnlineSessionSearchResult& Result, int32 Index);

protected:
	IOnlineSessionPtr GetSessionInterface() const;
	void CreateSessionInternal();
	void TravelToSession(const FOnlineSessionSearchResult& Result);

	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleFindSessionsComplete(bool bWasSuccessful);
	void HandleJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void HandleDestroySessionComplete(FName SessionName, bool bWasSuccessful);

protected:
	FPTHostParams PendingHostParams;
	FPTHostParams CurrentHostParams;
	FOnComplete PendingHostCallback;
	FOnFindComplete PendingFindCallback;
	FOnComplete PendingJoinCallback;
	TArray<FOnComplete> PendingDestroyCallbacks;

	TSharedPtr<FOnlineSessionSearch> SessionSearch;
	TArray<FOnlineSessionSearchResult> LastSearchResults;
	FOnlineSessionSearchResult PendingJoinResult;

	FString LocalPlayerId;

	FDelegateHandle CreateHandle;
	FDelegateHandle FindHandle;
	FDelegateHandle JoinHandle;
	FDelegateHandle DestroyHandle;

	bool bDestroyInProgress = false;
};
