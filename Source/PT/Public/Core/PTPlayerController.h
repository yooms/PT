// Copyright NeedsGames. All Rights Reserved.

#pragma once

#include "GameFramework/PlayerController.h"
#include "PTPlayerController.generated.h"

class UPTAbilitySystemComponent;

UCLASS()
class PT_API APTPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	APTPlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintPure, Category = "PT|PlayerController")
	UPTAbilitySystemComponent* GetPTAbilitySystemComponent() const;

	/**
	 * 게임에서 나가기.
	 *  - 호스트: 최신 스냅샷을 보내고 다른 플레이어에게 호스트를 인계한 뒤 나감
	 *  - 클라이언트: 세션에서 나가 메뉴로
	 */
	UFUNCTION(BlueprintCallable, Category = "PT|Session")
	void LeaveGame();

	/** 호스트 전용: 모두의 세션을 종료 (인계하지 않음) */
	UFUNCTION(BlueprintCallable, Category = "PT|Session")
	void EndSessionForEveryone();

	virtual void PostProcessInput(const float DeltaTime, const bool bGamePaused) override;
	virtual void ClientWasKicked_Implementation(const FText& KickReason) override;

protected:
	void HostLeave(bool bAllowMigration);

	/** 호스트가 나가기 전에 클라이언트에게 마지막 스냅샷/알림이 전달될 시간 */
	UPROPERTY(EditDefaultsOnly, Category = "PT|Session")
	float HostLeaveFlushDelay = 0.5f;

	FTimerHandle LeaveTimerHandle;
};
