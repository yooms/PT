// Copyright NeedsGames. All Rights Reserved.

#pragma once

#include "Engine/GameInstance.h"
#include "PTGameInstance.generated.h"

/**
 * PT GameInstance.
 * 대부분의 기능은 GameInstanceSubsystem 으로 분리되어 있습니다.
 *  - UPTSessionSubsystem        : 세션 호스트/검색/참가
 *  - UPTHostMigrationSubsystem  : 호스트 인계
 *  - UPTShaderWarmupSubsystem   : 셰이더(PSO) 프리컴파일
 *
 * 테스트용 콘솔 명령:
 *  pt.Host [MapPath] [MaxPlayers]  / pt.Find / pt.Join <Index> / pt.Leave / pt.EndForAll
 *  pt.Migration.Status / pt.Migration.Force / pt.ShaderWarmup.Run
 */
UCLASS(Config = Game)
class PT_API UPTGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
};
