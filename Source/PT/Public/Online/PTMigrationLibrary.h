// Copyright NeedsGames. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Online/PTMigrationTypes.h"
#include "PTMigrationLibrary.generated.h"

class AGameModeBase;
class UAbilitySystemComponent;

/** 호스트 인계 스냅샷 캡처/복원/직렬화 유틸리티 */
UCLASS()
class PT_API UPTMigrationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ---- 직렬화 ----
	static bool SerializeSnapshot(const FPTMigrationSnapshot& Snapshot, TArray<uint8>& OutCompressed, int32& OutUncompressedSize);
	static bool DeserializeSnapshot(const TArray<uint8>& Compressed, int32 UncompressedSize, FPTMigrationSnapshot& OutSnapshot);

	/** UPROPERTY(SaveGame) 만 직렬화 */
	static void SaveGameProperties(UObject* Object, TArray<uint8>& OutData);
	static void LoadGameProperties(UObject* Object, const TArray<uint8>& Data);

	// ---- GAS ----
	static void CaptureAttributes(const UAbilitySystemComponent* ASC, TArray<FPTAttributeSnapshot>& OutAttributes);
	static void RestoreAttributes(UAbilitySystemComponent* ASC, const TArray<FPTAttributeSnapshot>& Attributes);

	// ---- 월드 ----
	/** 서버: IPTMigratableInterface 액터 캡처 */
	static void CaptureMigratableActors(UWorld* World, TArray<FPTActorSnapshot>& OutActors);
	/** 서버(새 호스트): 액터 상태 복원 */
	static void RestoreMigratableActors(UWorld* World, const TArray<FPTActorSnapshot>& Actors);

	/** PIE 접두사를 제거한 현재 맵 롱 패키지 이름 */
	UFUNCTION(BlueprintPure, Category = "PT|Migration", meta = (WorldContext = "WorldContextObject"))
	static FString GetCurrentMapPackageName(const UObject* WorldContextObject);

	/** 세션 호환성 확인용 빌드 ID (ProjectVersion + Changelist) */
	UFUNCTION(BlueprintPure, Category = "PT|Migration")
	static FString GetBuildId();
};
