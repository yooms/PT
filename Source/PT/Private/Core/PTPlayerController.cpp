// Copyright NeedsGames. All Rights Reserved.

#include "Core/PTPlayerController.h"
#include "Core/PTGameMode.h"
#include "Core/PTGameState.h"
#include "Core/PTPlayerState.h"
#include "AbilitySystem/PTAbilitySystemComponent.h"
#include "Online/PTHostMigrationSubsystem.h"
#include "Online/PTSessionSubsystem.h"
#include "PTLogChannels.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PTPlayerController)

APTPlayerController::APTPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UPTAbilitySystemComponent* APTPlayerController::GetPTAbilitySystemComponent() const
{
	const APTPlayerState* PS = GetPlayerState<APTPlayerState>();
	return PS ? PS->GetPTAbilitySystemComponent() : nullptr;
}

void APTPlayerController::PostProcessInput(const float DeltaTime, const bool bGamePaused)
{
	if (UPTAbilitySystemComponent* ASC = GetPTAbilitySystemComponent())
	{
		ASC->ProcessAbilityInput(DeltaTime, bGamePaused);
	}
	Super::PostProcessInput(DeltaTime, bGamePaused);
}

void APTPlayerController::ClientWasKicked_Implementation(const FText& KickReason)
{
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UPTHostMigrationSubsystem* Migration = GI->GetSubsystem<UPTHostMigrationSubsystem>())
		{
			Migration->NotifyKicked();
		}
	}
	Super::ClientWasKicked_Implementation(KickReason);
}

void APTPlayerController::LeaveGame()
{
	if (GetNetMode() == NM_ListenServer && IsLocalController())
	{
		HostLeave(true);
		return;
	}

	if (UPTSessionSubsystem* Sessions = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTSessionSubsystem>() : nullptr)
	{
		Sessions->LeaveSessionAndReturnToMenu();
	}
}

void APTPlayerController::EndSessionForEveryone()
{
	if (GetNetMode() == NM_ListenServer && IsLocalController())
	{
		HostLeave(false);
	}
}

void APTPlayerController::HostLeave(bool bAllowMigration)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UE_LOG(LogPTMigration, Log, TEXT("Host leaving (allow migration: %d)"), bAllowMigration ? 1 : 0);

	if (APTGameMode* GM = World->GetAuthGameMode<APTGameMode>())
	{
		// 마지막 상태를 즉시 스냅샷
		GM->CaptureAndPublishSnapshot();
	}
	if (APTGameState* GS = World->GetGameState<APTGameState>())
	{
		GS->MulticastHostLeaving(bAllowMigration);
	}

	TWeakObjectPtr<APTPlayerController> WeakThis(this);
	World->GetTimerManager().SetTimer(LeaveTimerHandle, FTimerDelegate::CreateWeakLambda(this, [WeakThis]()
	{
		APTPlayerController* This = WeakThis.Get();
		if (UPTSessionSubsystem* Sessions = This && This->GetGameInstance() ? This->GetGameInstance()->GetSubsystem<UPTSessionSubsystem>() : nullptr)
		{
			Sessions->LeaveSessionAndReturnToMenu();
		}
	}), FMath::Max(0.05f, HostLeaveFlushDelay), false);
}
