// Copyright yoomsGames. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UGameInstance;
class UGameViewportClient;

/**
 * 에셋 없이 동작하는 전체 화면 상태 오버레이 (셰이더 컴파일 / 호스트 인계 진행 표시).
 * Progress 가 unset 이면 마퀴(무한) 진행 표시.
 */
class PT_API SPTStatusOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPTStatusOverlay) {}
		SLATE_ATTRIBUTE(FText, Title)
		SLATE_ATTRIBUTE(FText, Detail)
		SLATE_ATTRIBUTE(TOptional<float>, Progress)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};

/** 뷰포트에 오버레이를 붙이고, 맵 로드 후 다시 붙이는 헬퍼 */
class PT_API FPTStatusOverlayHandle
{
public:
	void Show(UGameInstance* GameInstance, TAttribute<FText> Title, TAttribute<FText> Detail, TAttribute<TOptional<float>> Progress, int32 ZOrder = 1000);
	void Hide();
	/** LoadMap 은 RemoveAllViewportWidgets 를 호출하므로 맵 로드 후 다시 붙여야 함 */
	void Reattach();
	bool IsShown() const { return Widget.IsValid(); }

private:
	void Attach();
	void Detach();

	TWeakObjectPtr<UGameInstance> OwningGameInstance;
	TWeakObjectPtr<UGameViewportClient> AttachedViewport;
	TSharedPtr<SPTStatusOverlay> Widget;
	int32 CachedZOrder = 1000;
};
