// Copyright NeedsGames. All Rights Reserved.

#include "UI/SPTStatusOverlay.h"

#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SPTStatusOverlay::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
		.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.88f))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(560.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(0.f, 0.f, 0.f, 12.f)
				[
					SNew(STextBlock)
					.Text(InArgs._Title)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 26))
					.ColorAndOpacity(FLinearColor::White)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(0.f, 0.f, 0.f, 18.f)
				[
					SNew(STextBlock)
					.Text(InArgs._Detail)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
					.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.f))
					.Justification(ETextJustify::Center)
					.AutoWrapText(true)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(10.f)
					[
						SNew(SProgressBar)
						.Percent(InArgs._Progress)
						.FillColorAndOpacity(FLinearColor(0.2f, 0.6f, 1.f, 1.f))
					]
				]
			]
		]
	];
}

void FPTStatusOverlayHandle::Show(UGameInstance* GameInstance, TAttribute<FText> Title, TAttribute<FText> Detail, TAttribute<TOptional<float>> Progress, int32 ZOrder)
{
	Hide();

	OwningGameInstance = GameInstance;
	CachedZOrder = ZOrder;

	Widget = SNew(SPTStatusOverlay)
		.Title(Title)
		.Detail(Detail)
		.Progress(Progress);

	Attach();
}

void FPTStatusOverlayHandle::Hide()
{
	Detach();
	Widget.Reset();
}

void FPTStatusOverlayHandle::Reattach()
{
	if (Widget.IsValid())
	{
		Detach();
		Attach();
	}
}

void FPTStatusOverlayHandle::Attach()
{
	UGameInstance* GI = OwningGameInstance.Get();
	UGameViewportClient* Viewport = GI ? GI->GetGameViewportClient() : nullptr;
	if (Viewport && Widget.IsValid())
	{
		Viewport->AddViewportWidgetContent(Widget.ToSharedRef(), CachedZOrder);
		AttachedViewport = Viewport;
	}
}

void FPTStatusOverlayHandle::Detach()
{
	if (UGameViewportClient* Viewport = AttachedViewport.Get())
	{
		if (Widget.IsValid())
		{
			Viewport->RemoveViewportWidgetContent(Widget.ToSharedRef());
		}
	}
	AttachedViewport.Reset();
}
