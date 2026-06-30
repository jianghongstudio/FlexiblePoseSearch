// Fill out your copyright notice in the Description page of Project Settings.

#include "FlexiblePoseSearchLibrary.h"

#include "AnimNotifyState_FlexiblePoseSearch.h"
#include "AnimNodes/AnimNode_PoseSearchPlayer.h"
#include "Animation/AnimSequenceBase.h"
#include "PoseSearch/PoseSearchDatabase.h"

namespace FlexiblePoseSearchLibrary_Private
{
	struct FNxNotifyWindow
	{
		const UAnimNotifyState_FlexiblePoseSearch* Notify = nullptr;
		const FAnimNotifyEvent* Event = nullptr;
		int32 Count = 0;
	};

	FNxNotifyWindow FindNxNotifyWindow(const UAnimSequenceBase* Sequence)
	{
		FNxNotifyWindow Result;
		if (!Sequence)
		{
			return Result;
		}

		for (const FAnimNotifyEvent& NotifyEvent : Sequence->Notifies)
		{
			const UAnimNotifyState_FlexiblePoseSearch* Candidate =
				Cast<UAnimNotifyState_FlexiblePoseSearch>(NotifyEvent.NotifyStateClass);
			if (!Candidate)
			{
				continue;
			}

			++Result.Count;
			if (!Result.Notify)
			{
				Result.Notify = Candidate;
				Result.Event = &NotifyEvent;
			}
		}
		return Result;
	}

	bool HasMatchingNativeWindow(
		const UAnimSequenceBase* Sequence,
		const FNxNotifyWindow& NotifyWindow,
		const UPoseSearchDatabase* Database)
	{
		if (!Sequence
			|| NotifyWindow.Count != 1
			|| !NotifyWindow.Notify
			|| !NotifyWindow.Event
			|| NotifyWindow.Notify->SourceMode != EFlexiblePoseSearchSourceMode::NativePSD
			|| NotifyWindow.Notify->Database != Database)
		{
			return false;
		}

#if WITH_EDITORONLY_DATA
		const FFloatInterval ExpectedRange(
			NotifyWindow.Event->GetTime(),
			NotifyWindow.Event->GetTime() + NotifyWindow.Event->GetDuration());
		for (int32 AssetIndex = 0; AssetIndex < Database->GetNumAnimationAssets(); ++AssetIndex)
		{
			const FPoseSearchDatabaseAnimationAsset* Asset =
				Database->GetDatabaseAnimationAsset(AssetIndex);
			if (Asset
				&& Asset->GetAnimationAsset() == Sequence
				&& FMath::IsNearlyEqual(Asset->SamplingRange.Min, ExpectedRange.Min)
				&& FMath::IsNearlyEqual(Asset->SamplingRange.Max, ExpectedRange.Max)
				&& Asset->BranchInId == NotifyWindow.Notify->GetBranchInId())
			{
				return true;
			}
		}
		return false;
#else
		return true;
#endif
	}
}

void UFlexiblePoseSearchLibrary::GatherPoseSearchAssets(const UAnimSequenceBase* Seed, TArray<UObject*>& OutAssets)
{
	TArray<UObject*> FallbackAssets;
	GatherPoseSearchAssetTiers(Seed, OutAssets, FallbackAssets);
}

void UFlexiblePoseSearchLibrary::GatherPoseSearchAssetTiers(
	const UAnimSequenceBase* Seed,
	TArray<UObject*>& OutPrimaryAssets,
	TArray<UObject*>& OutFallbackAssets)
{
	OutPrimaryAssets.Reset();
	OutFallbackAssets.Reset();
	if (!Seed)
	{
		return;
	}

	const FlexiblePoseSearchLibrary_Private::FNxNotifyWindow SeedWindow =
		FlexiblePoseSearchLibrary_Private::FindNxNotifyWindow(Seed);
	if (SeedWindow.Count != 1)
	{
		ensureMsgf(
			false,
			TEXT("FlexiblePoseSearch seed '%s' has %d FlexiblePoseSearch BranchIn windows; rejecting NativePSD and using the first small library."),
			*GetNameSafe(Seed),
			SeedWindow.Count);
		if (SeedWindow.Notify && SeedWindow.Notify->PoseSearchData)
		{
			OutPrimaryAssets.Add(SeedWindow.Notify->PoseSearchData);
		}
		else
		{
			OutPrimaryAssets.Add(const_cast<UAnimSequenceBase*>(Seed));
		}
		return;
	}

	const UAnimNotifyState_FlexiblePoseSearch* FlexiblePoseSearch = SeedWindow.Notify;
	if (FlexiblePoseSearch->SourceMode == EFlexiblePoseSearchSourceMode::NativePSD)
	{
		const UPoseSearchDatabase* Database = FlexiblePoseSearch->Database;
		const bool bNativeWindowMatches =
			FlexiblePoseSearchLibrary_Private::HasMatchingNativeWindow(Seed, SeedWindow, Database);
		if (!bNativeWindowMatches)
		{
			ensureMsgf(
				false,
				TEXT("FlexiblePoseSearch NativePSD contract mismatch for seed '%s': missing Database, window, or BranchInId. Using SmallLibrary fallback."),
				*GetNameSafe(Seed));
			if (FlexiblePoseSearch->PoseSearchData)
			{
				OutPrimaryAssets.Add(FlexiblePoseSearch->PoseSearchData);
			}
			return;
		}

		if (FlexiblePoseSearch->NativeSearchScope == EFlexiblePoseSearchNativeSearchScope::EntireDatabase)
		{
			OutPrimaryAssets.Add(const_cast<UPoseSearchDatabase*>(Database));
		}
		else
		{
			// Pass Sequences rather than the database so the engine's BranchIn path
			// resolves them into Database while retaining this explicit candidate set.
			OutPrimaryAssets.Add(const_cast<UAnimSequenceBase*>(Seed));
			for (const TObjectPtr<UAnimSequenceBase>& SetMember : FlexiblePoseSearch->PoseSearchSet)
			{
				const UAnimSequenceBase* Candidate = SetMember.Get();
				if (!Candidate || Candidate == Seed)
				{
					continue;
				}

				const FlexiblePoseSearchLibrary_Private::FNxNotifyWindow CandidateWindow =
					FlexiblePoseSearchLibrary_Private::FindNxNotifyWindow(Candidate);
				if (!FlexiblePoseSearchLibrary_Private::HasMatchingNativeWindow(Candidate, CandidateWindow, Database))
				{
					ensureMsgf(
						false,
						TEXT("FlexiblePoseSearch NativePSD Set member '%s' is not a valid BranchIn candidate for seed '%s'. Using SmallLibrary fallback."),
						*GetNameSafe(Candidate),
						*GetNameSafe(Seed));
					OutPrimaryAssets.Reset();
					if (FlexiblePoseSearch->PoseSearchData)
					{
						OutPrimaryAssets.Add(FlexiblePoseSearch->PoseSearchData);
					}
					return;
				}

				OutPrimaryAssets.AddUnique(const_cast<UAnimSequenceBase*>(Candidate));
			}
		}

		if (FlexiblePoseSearch->PoseSearchData)
		{
			OutFallbackAssets.Add(FlexiblePoseSearch->PoseSearchData);
		}
		return;
	}
	else if (FlexiblePoseSearch->PoseSearchData)
	{
		OutPrimaryAssets.Add(FlexiblePoseSearch->PoseSearchData);
		return;
	}

	// An incomplete authoring setup has no valid SmallLibrary fallback; preserve the
	// existing full-clip behavior only for this non-NativePSD case.
	OutPrimaryAssets.Add(const_cast<UAnimSequenceBase*>(Seed));
}

FSequencePlayerReference UFlexiblePoseSearchLibrary::SyncPlayingTimeFromPoseMatching(
	const FAnimUpdateContext& UpdateContext,
	const FSequencePlayerReference& SequencePlayer)
{
	using namespace UE::Anim;
	SequencePlayer.CallAnimNodeFunction<FAnimNode_PoseSearchPlayer>(
		TEXT("SyncPlayingTimeFromPoseMatching"),
		[&UpdateContext](FAnimNode_PoseSearchPlayer& InPoseSearchPlayer)
		{
			InPoseSearchPlayer.SyncPlayingTimeFromPoseMatching(*UpdateContext.GetContext());
		});
	return SequencePlayer;
}

FSequencePlayerReference UFlexiblePoseSearchLibrary::UpdateAnimAssetFromPoseMatching(
	const FAnimUpdateContext& UpdateContext,
	const FSequencePlayerReference& SequencePlayer,
	float BlendTime,
	const UAnimSequenceBase* NewAnimSequence)
{
	using namespace UE::Anim;
	SequencePlayer.CallAnimNodeFunction<FAnimNode_PoseSearchPlayer>(
		TEXT("UpdateAnimAssetFromPoseMatching"),
		[&UpdateContext, BlendTime, NewAnimSequence](FAnimNode_PoseSearchPlayer& InPoseSearchPlayer)
		{
			InPoseSearchPlayer.UpdateAnimAssetFromPoseMatching(
				*UpdateContext.GetContext(), BlendTime, NewAnimSequence);
		});
	return SequencePlayer;
}
