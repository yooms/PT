// Copyright yoomsGames. All Rights Reserved.

#include "Traversal/PTTraversalAnimSet.h"
#include "PTLogChannels.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "PoseSearch/PoseSearchLibrary.h"
#include "PoseSearch/PoseSearchResult.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PTTraversalAnimSet)

bool UPTTraversalAnimSet::ContainsMontage(const UAnimMontage* Montage) const
{
	return Montage && Entries.ContainsByPredicate([Montage](const FPTTraversalAnimEntry& Entry) { return Entry.Montage == Montage; });
}

bool UPTTraversalAnimSet::SelectMontage(UAnimInstance* AnimInstance, EPTGait Gait, bool bInAir, FPTTraversalCheckResult& InOutResult) const
{
	// 1) 조건에 맞는 후보
	TArray<const FPTTraversalAnimEntry*> Candidates;
	for (const FPTTraversalAnimEntry& Entry : Entries)
	{
		if (!Entry.Montage || Entry.Action != InOutResult.Action || Entry.bInAir != bInAir)
		{
			continue;
		}
		if (InOutResult.ObstacleHeight < Entry.MinHeight || InOutResult.ObstacleHeight > Entry.MaxHeight)
		{
			continue;
		}
		if (Entry.Gaits.Num() > 0 && !Entry.Gaits.Contains(Gait))
		{
			continue;
		}
		if (InOutResult.Speed < Entry.MinSpeed || InOutResult.Speed > Entry.MaxSpeed)
		{
			continue;
		}
		Candidates.Add(&Entry);
	}

	// 속도 조건 때문에 후보가 없으면 속도 조건 없이 다시 (예: 경계 속도)
	if (Candidates.Num() == 0)
	{
		for (const FPTTraversalAnimEntry& Entry : Entries)
		{
			if (Entry.Montage && Entry.Action == InOutResult.Action && Entry.bInAir == bInAir
				&& InOutResult.ObstacleHeight >= Entry.MinHeight && InOutResult.ObstacleHeight <= Entry.MaxHeight)
			{
				Candidates.Add(&Entry);
			}
		}
	}

	if (Candidates.Num() == 0)
	{
		return false;
	}

	// 2) 모션 매칭으로 현재 포즈와 이어지는 몽타주/시작 시간 선택
	if (bUseMotionMatching && AnimInstance && Candidates.Num() > 0)
	{
		TArray<UObject*> Assets;
		for (const FPTTraversalAnimEntry* Entry : Candidates)
		{
			Assets.Add(Entry->Montage);
		}

		FPoseSearchBlueprintResult SearchResult;
		UPoseSearchLibrary::MotionMatch(AnimInstance, Assets, PoseHistoryName, FPoseSearchContinuingProperties(), FPoseSearchFutureProperties(), SearchResult);

		if (UAnimMontage* Selected = Cast<UAnimMontage>(SearchResult.SelectedAnim))
		{
			const FPTTraversalAnimEntry* const* Found = Candidates.FindByPredicate([Selected](const FPTTraversalAnimEntry* Entry) { return Entry->Montage == Selected; });
			InOutResult.Montage = Selected;
			InOutResult.MontageStartTime = FMath::Max(0.f, SearchResult.SelectedTime);
			InOutResult.MontagePlayRate = Found ? (*Found)->PlayRate : 1.f;
			return true;
		}
	}

	// 3) 대체: 높이·속도 범위 중앙에 가장 가까운 후보, 처음부터 재생 (같은 점수면 번갈아 선택해 왼발/오른발 변형을 섞음)
	const FPTTraversalAnimEntry* Best = nullptr;
	float BestScore = MAX_flt;
	static int32 TieBreaker = 0;
	++TieBreaker;
	for (int32 i = 0; i < Candidates.Num(); ++i)
	{
		const FPTTraversalAnimEntry* Entry = Candidates[(i + TieBreaker) % Candidates.Num()];
		const float HeightMid = (Entry->MinHeight + Entry->MaxHeight) * 0.5f;
		const float SpeedMid = (Entry->MinSpeed + FMath::Min(Entry->MaxSpeed, 1000.f)) * 0.5f;
		const float Score = FMath::Abs(InOutResult.ObstacleHeight - HeightMid) + 0.1f * FMath::Abs(InOutResult.Speed - SpeedMid);
		if (Score < BestScore - 0.01f)
		{
			BestScore = Score;
			Best = Entry;
		}
	}

	InOutResult.Montage = Best->Montage;
	InOutResult.MontageStartTime = 0.f;
	InOutResult.MontagePlayRate = Best->PlayRate;
	return true;
}
