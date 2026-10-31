// Copyright NeedsGames. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "PTMigratableInterface.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UPTMigratableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 호스트 인계 시 상태를 보존해야 하는 월드 액터가 구현하는 인터페이스.
 *  - Transform 과 UPROPERTY(SaveGame) 로 표시된 값이 자동 저장/복원됩니다.
 *  - 레벨 배치 액터는 이름으로 매칭, 런타임 스폰 액터는 클래스로 재생성됩니다.
 *  - 인계 전에 파괴된 레벨 배치 액터는 새 호스트에서도 파괴됩니다.
 */
class PT_API IPTMigratableInterface
{
	GENERATED_BODY()

public:
	/** false 를 반환하면 스냅샷에서 제외 */
	UFUNCTION(BlueprintNativeEvent, Category = "PT|Migration")
	bool ShouldCaptureForMigration() const;
	virtual bool ShouldCaptureForMigration_Implementation() const { return true; }

	/** 새 호스트에서 상태 복원이 끝난 직후 (서버) */
	UFUNCTION(BlueprintNativeEvent, Category = "PT|Migration")
	void OnMigrationRestored();
	virtual void OnMigrationRestored_Implementation() {}
};
