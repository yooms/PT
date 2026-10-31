// Copyright NeedsGames. All Rights Reserved.

#include "Shader/PTShaderWarmupSubsystem.h"
#include "Shader/PTShaderWarmupSettings.h"
#include "PTLogChannels.h"

#include "Async/Async.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "Misc/EngineVersion.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "UObject/UObjectGlobals.h"
#include "PipelineStateCache.h"
#include "RHI.h"
#include "DynamicRHI.h"
#include "ShaderCompiler.h"
#include "ShaderPipelineCache.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PTShaderWarmupSubsystem)

#define LOCTEXT_NAMESPACE "PTShaderWarmup"

namespace PTShaderWarmupPrivate
{
	static const TCHAR* ConfigSection = TEXT("/Script/PT.PTShaderWarmupState");
	static const TCHAR* ConfigKey = TEXT("Fingerprint");
	/** 남은 작업이 0 인 상태가 이 시간 동안 유지되어야 완료로 판단 (다음 배치가 늦게 잡히는 경우 대비) */
	static constexpr double SettleTime = 0.75;
}

void UPTShaderWarmupSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::Tick), 0.0f);

	// LoadMap 은 뷰포트 위젯을 모두 제거하므로, 워밍업 도중 맵이 바뀌면 진행 화면을 다시 붙임
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);

	// 런타임 Pak 마운트 (DLC / 패치). 다른 스레드에서 호출될 수 있으므로 게임 스레드로 전달
	TWeakObjectPtr<UPTShaderWarmupSubsystem> WeakThis(this);
	PakMountedHandle = FCoreDelegates::GetOnPakFileMounted2().AddLambda([WeakThis](const IPakFile&)
	{
		AsyncTask(ENamedThreads::GameThread, [WeakThis]()
		{
			if (UPTShaderWarmupSubsystem* This = WeakThis.Get())
			{
				This->HandlePakMounted();
			}
		});
	});
}

void UPTShaderWarmupSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	FCoreDelegates::GetOnPakFileMounted2().Remove(PakMountedHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	Overlay.Hide();
	WarmupAssetsHandle.Reset();
	Super::Deinitialize();
}

bool UPTShaderWarmupSubsystem::CanShowOverlay() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI && GI->GetGameViewportClient() != nullptr;
}

// -------------------------------------------------------------------------------------------------
// Fingerprint
// -------------------------------------------------------------------------------------------------

FString UPTShaderWarmupSubsystem::ComputeFingerprint() const
{
	const UPTShaderWarmupSettings* Settings = GetDefault<UPTShaderWarmupSettings>();

	FString Data;
	Data += FApp::GetBuildVersion();
	Data += TEXT("|");
	Data += FEngineVersion::Current().ToString();
	Data += TEXT("|");
	Data += Settings->FingerprintSalt;

	// GPU / 드라이버가 바뀌면 드라이버 PSO 캐시가 무효화되므로 다시 워밍업
	Data += TEXT("|");
	Data += GDynamicRHI ? GDynamicRHI->GetName() : TEXT("NoRHI");
	Data += TEXT("|");
	Data += GRHIAdapterName;
	Data += TEXT("|");
	Data += GRHIAdapterUserDriverVersion;
	Data += TEXT("|");
	Data += GRHIAdapterInternalDriverVersion;

	// 설치된 콘텐츠 (Pak / IoStore 컨테이너) 목록
	TArray<FString> ContainerFiles;
	const FString PakDir = FPaths::ProjectContentDir() / TEXT("Paks");
	IFileManager::Get().FindFilesRecursive(ContainerFiles, *PakDir, TEXT("*.*"), true, false, false);
	ContainerFiles.Sort();
	for (const FString& File : ContainerFiles)
	{
		const FString Ext = FPaths::GetExtension(File).ToLower();
		if (Ext == TEXT("pak") || Ext == TEXT("utoc") || Ext == TEXT("ucas"))
		{
			const int64 Size = IFileManager::Get().FileSize(*File);
			const FDateTime Stamp = IFileManager::Get().GetTimeStamp(*File);
			Data += FString::Printf(TEXT("|%s:%lld:%lld"), *FPaths::GetCleanFilename(File), Size, Stamp.GetTicks());
		}
	}

	return FMD5::HashAnsiString(*Data);
}

FString UPTShaderWarmupSubsystem::LoadSavedFingerprint() const
{
	FString Value;
	if (GConfig)
	{
		GConfig->GetString(PTShaderWarmupPrivate::ConfigSection, PTShaderWarmupPrivate::ConfigKey, Value, GGameUserSettingsIni);
	}
	return Value;
}

void UPTShaderWarmupSubsystem::SaveFingerprint(const FString& Fingerprint) const
{
	if (GConfig)
	{
		GConfig->SetString(PTShaderWarmupPrivate::ConfigSection, PTShaderWarmupPrivate::ConfigKey, *Fingerprint, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

// -------------------------------------------------------------------------------------------------
// Flow
// -------------------------------------------------------------------------------------------------

void UPTShaderWarmupSubsystem::RunInitialCheck()
{
	bInitialCheckDone = true;

	const UPTShaderWarmupSettings* Settings = GetDefault<UPTShaderWarmupSettings>();
	const bool bSkipInEditor = GIsEditor && !Settings->bRunInEditor;

	if (!Settings->bEnableWarmup || bSkipInEditor || IsRunningDedicatedServer())
	{
		// StartupMode=0(Paused) 이므로 반드시 재개
		FShaderPipelineCache::SetBatchMode(FShaderPipelineCache::BatchMode::Background);
		FShaderPipelineCache::ResumeBatching();
		UE_LOG(LogPTShader, Log, TEXT("Shader warmup skipped (enabled=%d, editor=%d)"), Settings->bEnableWarmup ? 1 : 0, GIsEditor ? 1 : 0);
		return;
	}

	const FString Current = ComputeFingerprint();
	const FString Saved = LoadSavedFingerprint();

	if (Current == Saved)
	{
		UE_LOG(LogPTShader, Log, TEXT("Shader cache up to date (%s) - background mode"), *Current);
		FShaderPipelineCache::SetBatchMode(FShaderPipelineCache::BatchMode::Background);
		FShaderPipelineCache::ResumeBatching();
		return;
	}

	UE_LOG(LogPTShader, Log, TEXT("Shader fingerprint changed (%s -> %s) - running warmup"), Saved.IsEmpty() ? TEXT("<none>") : *Saved, *Current);
	StartWarmup(true);
}

void UPTShaderWarmupSubsystem::StartWarmup(bool bForce)
{
	if (bRunning)
	{
		return;
	}
	if (!bForce && ComputeFingerprint() == LoadSavedFingerprint())
	{
		return;
	}

	bInitialCheckDone = true;
	bRunning = true;
	bPendingRewarm = false;
	StartTime = FPlatformTime::Seconds();
	ZeroRemainingSince = -1.0;
	MaxRemainingSeen = 0;

	// 로딩 화면 동안에는 큰 배치로 빠르게
	FShaderPipelineCache::SetBatchMode(FShaderPipelineCache::BatchMode::Fast);
	FShaderPipelineCache::ResumeBatching();

	const UPTShaderWarmupSettings* Settings = GetDefault<UPTShaderWarmupSettings>();
	TArray<FSoftObjectPath> Assets;
	for (const FSoftObjectPath& Path : Settings->WarmupAssets)
	{
		if (Path.IsValid())
		{
			Assets.Add(Path);
		}
	}
	if (Assets.Num() > 0)
	{
		WarmupAssetsHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Assets, FStreamableDelegate(), FStreamableManager::AsyncLoadHighPriority);
	}

	if (CanShowOverlay())
	{
		Overlay.Show(GetGameInstance(),
			MakeAttributeUObject(this, &ThisClass::GetOverlayTitle),
			MakeAttributeUObject(this, &ThisClass::GetOverlayDetail),
			MakeAttributeUObject(this, &ThisClass::GetOverlayProgress),
			2000);
	}

	UE_LOG(LogPTShader, Log, TEXT("Shader warmup started (%d warmup assets)"), Assets.Num());
}

int32 UPTShaderWarmupSubsystem::QueryRemainingWork() const
{
	int32 Remaining = (int32)FShaderPipelineCache::NumPrecompilesRemaining();

	if (GetDefault<UPTShaderWarmupSettings>()->bWaitForPSOPrecache)
	{
		Remaining += (int32)PipelineStateCache::NumActivePrecacheRequests();
	}

	if (GShaderCompilingManager && !FPlatformProperties::RequiresCookedData())
	{
		Remaining += GShaderCompilingManager->GetNumRemainingJobs();
	}

	if (WarmupAssetsHandle.IsValid() && !WarmupAssetsHandle->HasLoadCompleted())
	{
		Remaining += 1;
	}
	return Remaining;
}

bool UPTShaderWarmupSubsystem::Tick(float DeltaTime)
{
	if (!bInitialCheckDone)
	{
		// 뷰포트가 생긴 뒤(첫 프레임 이후) 판단해야 오버레이를 띄울 수 있음
		if (CanShowOverlay() || IsRunningDedicatedServer() || GIsEditor)
		{
			RunInitialCheck();
		}
		return true;
	}

	if (!bRunning)
	{
		return true;
	}

	if (!Overlay.IsShown() && CanShowOverlay())
	{
		Overlay.Show(GetGameInstance(),
			MakeAttributeUObject(this, &ThisClass::GetOverlayTitle),
			MakeAttributeUObject(this, &ThisClass::GetOverlayDetail),
			MakeAttributeUObject(this, &ThisClass::GetOverlayProgress),
			2000);
	}

	const UPTShaderWarmupSettings* Settings = GetDefault<UPTShaderWarmupSettings>();
	const double Now = FPlatformTime::Seconds();
	const double Elapsed = Now - StartTime;

	LastRemaining = QueryRemainingWork();
	MaxRemainingSeen = FMath::Max(MaxRemainingSeen, LastRemaining);

	if (LastRemaining == 0)
	{
		if (ZeroRemainingSince < 0.0)
		{
			ZeroRemainingSince = Now;
		}
	}
	else
	{
		ZeroRemainingSince = -1.0;
	}

	const bool bSettled = ZeroRemainingSince >= 0.0 && (Now - ZeroRemainingSince) >= PTShaderWarmupPrivate::SettleTime;
	if (bSettled && Elapsed >= Settings->MinimumDisplayTime)
	{
		FinishWarmup(false);
	}
	else if (Elapsed >= Settings->MaxWaitTime)
	{
		FinishWarmup(true);
	}
	return true;
}

void UPTShaderWarmupSubsystem::FinishWarmup(bool bTimedOut)
{
	bRunning = false;
	WarmupAssetsHandle.Reset();
	Overlay.Hide();

	// 인게임에서는 프레임에 영향이 적은 백그라운드 모드
	FShaderPipelineCache::SetBatchMode(FShaderPipelineCache::BatchMode::Background);

	if (!bTimedOut)
	{
		SaveFingerprint(ComputeFingerprint());
	}

	UE_LOG(LogPTShader, Log, TEXT("Shader warmup %s in %.1fs (max queued %d)"), bTimedOut ? TEXT("TIMED OUT") : TEXT("finished"), FPlatformTime::Seconds() - StartTime, MaxRemainingSeen);
	OnWarmupComplete.Broadcast();

	if (bPendingRewarm)
	{
		StartWarmup(true);
	}
}

void UPTShaderWarmupSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (bRunning && LoadedWorld && LoadedWorld->GetGameInstance() == GetGameInstance())
	{
		Overlay.Reattach();
	}
}

void UPTShaderWarmupSubsystem::HandlePakMounted()
{
	if (!GetDefault<UPTShaderWarmupSettings>()->bRewarmOnRuntimePakMount || !bInitialCheckDone)
	{
		return;
	}

	UE_LOG(LogPTShader, Log, TEXT("New content mounted - shader warmup will rerun"));

	if (bRunning)
	{
		bPendingRewarm = true;
		return;
	}

	// 플레이 중(네트워크 게임)에는 화면을 가리지 않고 빠른 배치만 적용, 다음 실행 시 전체 워밍업
	const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (World && World->GetNetMode() != NM_Standalone)
	{
		FShaderPipelineCache::SetBatchMode(FShaderPipelineCache::BatchMode::Fast);
		SaveFingerprint(FString());
		return;
	}

	StartWarmup(true);
}

// -------------------------------------------------------------------------------------------------
// Overlay
// -------------------------------------------------------------------------------------------------

float UPTShaderWarmupSubsystem::GetProgress() const
{
	if (!bRunning)
	{
		return 1.f;
	}
	if (MaxRemainingSeen <= 0)
	{
		return 0.f;
	}
	return FMath::Clamp(1.f - float(LastRemaining) / float(MaxRemainingSeen), 0.f, 1.f);
}

FText UPTShaderWarmupSubsystem::GetOverlayTitle() const
{
	return LOCTEXT("Title", "Compiling shaders");
}

FText UPTShaderWarmupSubsystem::GetOverlayDetail() const
{
	if (LastRemaining > 0)
	{
		return FText::Format(LOCTEXT("DetailRemaining", "Optimizing graphics for your GPU. This only happens on first launch or after an update.\n{0} remaining"), FText::AsNumber(LastRemaining));
	}
	return LOCTEXT("DetailFinishing", "Finishing up...");
}

TOptional<float> UPTShaderWarmupSubsystem::GetOverlayProgress() const
{
	if (MaxRemainingSeen <= 0)
	{
		return TOptional<float>(); // 마퀴
	}
	return GetProgress();
}

#undef LOCTEXT_NAMESPACE
