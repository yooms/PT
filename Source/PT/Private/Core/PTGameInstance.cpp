// Copyright NeedsGames. All Rights Reserved.

#include "Core/PTGameInstance.h"
#include "Core/PTPlayerController.h"
#include "Online/PTHostMigrationSubsystem.h"
#include "Online/PTSessionSubsystem.h"
#include "Shader/PTShaderWarmupSubsystem.h"
#include "PTLogChannels.h"

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PTGameInstance)

void UPTGameInstance::Init()
{
	Super::Init();
	UE_LOG(LogPT, Log, TEXT("PTGameInstance initialized"));
}

// -------------------------------------------------------------------------------------------------
// Console commands (개발/테스트용)
// -------------------------------------------------------------------------------------------------

namespace PTConsole
{
	template<typename T>
	static T* GetSubsystem(UWorld* World)
	{
		UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<T>() : nullptr;
	}

	static FAutoConsoleCommandWithWorldAndArgs HostCmd(
		TEXT("pt.Host"),
		TEXT("Host a listen-server session. Usage: pt.Host [/Game/Map/Path] [MaxPlayers]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UPTSessionSubsystem* Sessions = GetSubsystem<UPTSessionSubsystem>(World))
			{
				FPTHostParams Params;
				Params.MapPath = Args.Num() > 0 ? Args[0] : FString();
				Params.MaxPlayers = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 4;
				Sessions->HostSession(Params);
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs FindCmd(
		TEXT("pt.Find"),
		TEXT("Find sessions and print them to the log."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UPTSessionSubsystem* Sessions = GetSubsystem<UPTSessionSubsystem>(World))
			{
				Sessions->FindSessionsNative(50, [](bool bSuccess, const TArray<FOnlineSessionSearchResult>& Results)
				{
					UE_LOG(LogPTSession, Display, TEXT("pt.Find: %s, %d session(s)"), bSuccess ? TEXT("OK") : TEXT("FAILED"), Results.Num());
					for (int32 i = 0; i < Results.Num(); ++i)
					{
						const FPTSessionInfo Info = UPTSessionSubsystem::MakeSessionInfo(Results[i], i);
						UE_LOG(LogPTSession, Display, TEXT("  [%d] %s  %d/%d  %dms  map=%s  lineage=%s epoch=%d rank=%d %s"),
							i, *Info.HostName, Info.CurrentPlayers, Info.MaxPlayers, Info.PingMs, *Info.MapPath,
							*Info.LineageId, Info.Epoch, Info.HostRank, Info.bBuildCompatible ? TEXT("") : TEXT("(INCOMPATIBLE BUILD)"));
					}
				});
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs JoinCmd(
		TEXT("pt.Join"),
		TEXT("Join a session from the last pt.Find result. Usage: pt.Join <Index>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UPTSessionSubsystem* Sessions = GetSubsystem<UPTSessionSubsystem>(World))
			{
				Sessions->JoinSessionByIndex(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0);
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs LeaveCmd(
		TEXT("pt.Leave"),
		TEXT("Leave the game. If host, hands the session over to another player."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (APTPlayerController* PC = World ? Cast<APTPlayerController>(World->GetFirstPlayerController()) : nullptr)
			{
				PC->LeaveGame();
			}
			else if (UPTSessionSubsystem* Sessions = GetSubsystem<UPTSessionSubsystem>(World))
			{
				Sessions->LeaveSessionAndReturnToMenu();
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs EndForAllCmd(
		TEXT("pt.EndForAll"),
		TEXT("Host only: end the session for everyone (no migration)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (APTPlayerController* PC = World ? Cast<APTPlayerController>(World->GetFirstPlayerController()) : nullptr)
			{
				PC->EndSessionForEveryone();
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs MigrationStatusCmd(
		TEXT("pt.Migration.Status"),
		TEXT("Print host-migration snapshot / candidate info."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UPTHostMigrationSubsystem* Migration = GetSubsystem<UPTHostMigrationSubsystem>(World);
			UPTSessionSubsystem* Sessions = GetSubsystem<UPTSessionSubsystem>(World);
			if (!Migration || !Sessions)
			{
				return;
			}

			const FPTMigrationSnapshot& Snap = Migration->GetLastSnapshot();
			UE_LOG(LogPTMigration, Display, TEXT("State=%d  LocalId=%s  SnapshotValid=%d"), (int32)Migration->GetMigrationState(), *Sessions->GetLocalPlayerId(), Snap.IsValid() ? 1 : 0);
			if (Snap.IsValid())
			{
				UE_LOG(LogPTMigration, Display, TEXT("  lineage=%s epoch=%d host=%s map=%s players=%d actors=%d"),
					*Snap.SessionLineageId, Snap.Epoch, *Snap.HostPlayerId, *Snap.MapPackageName, Snap.Players.Num(), Snap.Actors.Num());
				const TArray<FString> Order = Snap.GetSuccessorOrder();
				for (int32 i = 0; i < Order.Num(); ++i)
				{
					UE_LOG(LogPTMigration, Display, TEXT("  candidate #%d: %s%s"), i, *Order[i], Order[i] == Sessions->GetLocalPlayerId() ? TEXT("  <- me") : TEXT(""));
				}
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs MigrationForceCmd(
		TEXT("pt.Migration.Force"),
		TEXT("Client only (debug): drop the connection to the host to trigger host migration."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UPTHostMigrationSubsystem* Migration = GetSubsystem<UPTHostMigrationSubsystem>(World))
			{
				Migration->DebugForceMigration();
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs ShaderWarmupCmd(
		TEXT("pt.ShaderWarmup.Run"),
		TEXT("Force the shader/PSO warmup screen to run now."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UPTShaderWarmupSubsystem* Warmup = GetSubsystem<UPTShaderWarmupSubsystem>(World))
			{
				Warmup->StartWarmup(true);
			}
		}));
}
