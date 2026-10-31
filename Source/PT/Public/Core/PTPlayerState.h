// Copyright yoomsGames. All Rights Reserved.

#pragma once

#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystem/PTAbilitySet.h"
#include "PTPlayerState.generated.h"

class APTCharacter;
class UPTAbilitySystemComponent;
class UPTHealthSet;
class UPTStaminaSet;

/**
 * ASC 를 소유하는 PlayerState.
 * PTPlayerId 는 호스트 인계 시 같은 플레이어를 찾기 위한 ID (UPTSessionSubsystem::GetLocalPlayerId 에서 생성, 접속 URL ?PTPID= 로 전달).
 */
UCLASS()
class PT_API APTPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	APTPlayerState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "PT|PlayerState")
	UPTAbilitySystemComponent* GetPTAbilitySystemComponent() const { return AbilitySystemComponent; }

	UFUNCTION(BlueprintPure, Category = "PT|PlayerState")
	const FString& GetPTPlayerId() const { return PTPlayerId; }

	UFUNCTION(BlueprintPure, Category = "PT|PlayerState")
	int32 GetJoinOrder() const { return JoinOrder; }

	/** 서버 */
	void SetPTPlayerId(const FString& InId) { PTPlayerId = InId; }
	void SetJoinOrder(int32 InOrder) { JoinOrder = InOrder; }

	/** 서버: 폰이 가진 AbilitySet / DefaultAbilities 를 1회 부여 */
	void GrantAbilitiesFromPawn(APTCharacter* Pawn);

	/** 서버: 리스폰 시 사망 상태 해제 + 체력/스태미나 회복 */
	void ResetForRespawn();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* PlayerState) override;
	virtual void OverrideWith(APlayerState* PlayerState) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "PT|PlayerState")
	TObjectPtr<UPTAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UPTHealthSet> HealthSet;

	UPROPERTY()
	TObjectPtr<UPTStaminaSet> StaminaSet;

	UPROPERTY(Replicated)
	FString PTPlayerId;

	UPROPERTY(Replicated)
	int32 JoinOrder = 0;

	UPROPERTY()
	FPTAbilitySet_GrantedHandles GrantedHandles;

	bool bAbilitiesGranted = false;
};
