// Fill out your copyright notice in the Description page of Project Settings.


#include "FlexiblePoseSearchData.h"

#include "AnimNotifyState_FlexiblePoseSearch.h"
#include "Animation/AnimComposite.h"
#include "PoseSearch/PoseSearchAnimNotifies.h"
#include "PoseSearch/PoseSearchFeatureChannel.h"
#include "PoseSearch/PoseSearchIndex.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "UObject/ObjectSaveContext.h"
#if WITH_EDITOR
#include "PoseSearch/PoseSearchDerivedData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#endif //WITH_EDITOR

void UFlexiblePoseSearchData::PostLoad()
{
	Super::PostLoad();
#if WITH_EDITOR
	// Only register the hook here. Do NOT build/sanitize during PostLoad:
	// PoseSearch may delay DDC until dependents load; RequestAsyncBuildIndex(Wait)
	// on a Notstarted task asserts (PoseSearchDerivedData.cpp).
	EnsureWeightSanitizeHook();
#else
	// Cooked index is already serialized; strip Deviation weights if present.
	SanitizeSearchIndexWeights();
#endif
}

void UFlexiblePoseSearchData::BeginDestroy()
{
#if WITH_EDITOR
	TeardownWeightSanitizeHook();
#endif
	Super::BeginDestroy();
}

#if WITH_EDITOR
void UFlexiblePoseSearchData::EnsureWeightSanitizeHook()
{
	if (!bWeightSanitizeHooked)
	{
		RegisterOnDerivedDataRebuild(FOnDerivedDataRebuild::CreateUObject(this, &UFlexiblePoseSearchData::OnSearchIndexRebuilt));
		bWeightSanitizeHooked = true;
	}
}

void UFlexiblePoseSearchData::TeardownWeightSanitizeHook()
{
	if (bWeightSanitizeHooked)
	{
		UnregisterOnDerivedDataRebuild(this);
		bWeightSanitizeHooked = false;
	}
}

void UFlexiblePoseSearchData::OnSearchIndexRebuilt()
{
	// Index was just SetSearchIndex'd by PoseSearch before this broadcast.
	SanitizeSearchIndexWeights();
}
#endif // WITH_EDITOR

void UFlexiblePoseSearchData::SanitizeSearchIndexWeights()
{
	// Intentionally no-op: keep engine WeightsSqrt (Schema weights / small-DB Deviation).
	//
	// Do NOT strip /Deviation (raw cm L2 → multi-million Group/Traj costs).
	// Do NOT soft-clamp or globally rescale to Schema ||W|| — those did not fix the
	// Rewind Traj explosions that were recorded under slomo 0.1 (HistoryInterval=-1
	// collapses history), and they do not pull FlexiblePoseSearch absolute cost toward large PSD pools.
}

void UFlexiblePoseSearchData::PreSave(FObjectPreSaveContext SaveContext)
{
#if WITH_EDITOR
	using namespace UE::PoseSearch;
	EnsureWeightSanitizeHook();
	SynchronizeWithNotifyState();
	const UAnimNotifyState_FlexiblePoseSearch* OwnerNotify =
		Cast<UAnimNotifyState_FlexiblePoseSearch>(GetOuter());
	const bool bIsSmallLibraryData = !OwnerNotify
		|| OwnerNotify->SourceMode == EFlexiblePoseSearchSourceMode::SmallLibrary;
	if (!IsTemplate() && !SaveContext.IsProceduralSave())
	{
		if (bIsSmallLibraryData)
		{
			const EAsyncBuildIndexResult BuildResult = FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(
				this, ERequestAsyncBuildFlag::NewRequest | ERequestAsyncBuildFlag::WaitForCompletion);
			if (BuildResult == EAsyncBuildIndexResult::Success)
			{
				SanitizeSearchIndexWeights();
			}
		}
		else
		{
			// A NativePSD notify still owns an FlexiblePoseSearch data object for deterministic
			// fallback. Its dependency sync must build the narrow BranchIn window,
			// but waiting from the native PSD's save path can race its cache task.
			FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(
				this, ERequestAsyncBuildFlag::NewRequest);
		}
	}
#endif
	Super::PreSave(SaveContext);
}

#if WITH_EDITOR
void UFlexiblePoseSearchData::SynchronizeWithFlexiblePoseSearchExternalDependencies(TConstArrayView<UAnimSequenceBase*> SequencesBase)
{
	TArray<FPoseSearchDatabaseAnimationAsset> NewAnimationAssets;

	// collecting all the database AnimationAsset(s) that don't require synchronization

	const int32 NumOfAnimationAssets = GetNumAnimationAssets();

	for (int i = 0; i < NumOfAnimationAssets; ++i)
	{
		if (FPoseSearchDatabaseAnimationAsset* AnimationAssetBase = GetMutableDatabaseAnimationAsset(i))
		{
			const bool bRequiresSynchronization = AnimationAssetBase->IsSynchronizedWithExternalDependency() && SequencesBase.Contains(AnimationAssetBase->GetAnimationAsset());
			if (!bRequiresSynchronization)
			{
				NewAnimationAssets.Add(*AnimationAssetBase);
			}
		}
	}
	// collecting all the SequencesBase(s) requiring synchronization
	for (UAnimSequenceBase* SequenceBase : SequencesBase)
	{
		if (SequenceBase)
		{
			for (const FAnimNotifyEvent& NotifyEvent : SequenceBase->Notifies)
			{
				UPoseSearchDatabase* TargetDatabase = nullptr;
				uint32 BranchId = 0;
				if (const UAnimNotifyState_PoseSearchBranchIn* PoseSearchBranchIn = Cast<UAnimNotifyState_PoseSearchBranchIn>(NotifyEvent.NotifyStateClass))
				{
					TargetDatabase = PoseSearchBranchIn->Database;
					BranchId = PoseSearchBranchIn->GetBranchInId();
				}
				else if (const UAnimNotifyState_FlexiblePoseSearch* FlexiblePoseSearch = Cast<UAnimNotifyState_FlexiblePoseSearch>(NotifyEvent.NotifyStateClass);
					FlexiblePoseSearch && FlexiblePoseSearch->SourceMode == EFlexiblePoseSearchSourceMode::SmallLibrary)
				{
					TargetDatabase = FlexiblePoseSearch->PoseSearchData;
					BranchId = FlexiblePoseSearch->GetBranchInId();
				}
				
				if (TargetDatabase)
				{
					if (TargetDatabase == this)
					{
						auto GetSamplingRange = [](const FAnimNotifyEvent& NotifyEvent, const UAnimSequenceBase* SequenceBase) -> FFloatInterval
						{
							FFloatInterval SamplingRange(NotifyEvent.GetTime(), NotifyEvent.GetTime() + NotifyEvent.GetDuration());
							if (SamplingRange.Min <= NotifyEvent.TriggerTimeOffset && SamplingRange.Max >= SequenceBase->GetPlayLength() - NotifyEvent.TriggerTimeOffset)
							{
								SamplingRange = FFloatInterval(0.f, 0.f);
							}
							return SamplingRange;
						};

						if (UAnimSequence* Sequence = Cast<UAnimSequence>(SequenceBase))
						{
							FPoseSearchDatabaseAnimationAsset DatabaseAnimationAsset;
							DatabaseAnimationAsset.AnimAsset = Sequence;
							DatabaseAnimationAsset.SamplingRange = GetSamplingRange(NotifyEvent, SequenceBase);
							DatabaseAnimationAsset.BranchInId = BranchId;
							NewAnimationAssets.Add(DatabaseAnimationAsset);
						}
						else if (UAnimComposite* AnimComposite = Cast<UAnimComposite>(SequenceBase))
						{
							FPoseSearchDatabaseAnimationAsset DatabaseAnimationAsset;
							DatabaseAnimationAsset.AnimAsset = AnimComposite;
							DatabaseAnimationAsset.SamplingRange = GetSamplingRange(NotifyEvent, SequenceBase);
							DatabaseAnimationAsset.BranchInId = BranchId;
							NewAnimationAssets.Add(DatabaseAnimationAsset);
						}
						else if (UAnimMontage* AnimMontage = Cast<UAnimMontage>(SequenceBase))
						{
							FPoseSearchDatabaseAnimationAsset DatabaseAnimationAsset;
							DatabaseAnimationAsset.AnimAsset = AnimMontage;
							DatabaseAnimationAsset.SamplingRange = GetSamplingRange(NotifyEvent, SequenceBase);
							DatabaseAnimationAsset.BranchInId = BranchId;
							NewAnimationAssets.Add(DatabaseAnimationAsset);
						}
					}
				}
			}
		}
	}

	// updating AnimationAssets from NewAnimationAssets preserving the original sorting
	bool bModified = false;
	for (int32 AnimationAssetIndex = GetNumAnimationAssets() - 1; AnimationAssetIndex >= 0; --AnimationAssetIndex)
	{
		if (FPoseSearchDatabaseAnimationAsset* AnimationAsset = GetMutableDatabaseAnimationAsset(AnimationAssetIndex))
		{
			int32 FoundIndex = -1;
			for(int i=0; i < NewAnimationAssets.Num(); i++)
			{
				const FPoseSearchDatabaseAnimationAsset& NewAnimationAsset = NewAnimationAssets[i];
				if (AnimationAsset->UpdateFrom(NewAnimationAsset))
				{
					FoundIndex = i;
					break;
				}
			
			}
		
			if (FoundIndex >= 0)
			{
				NewAnimationAssets.RemoveAt(FoundIndex); 
			}
			else
			{
				RemoveAnimationAssetAt(AnimationAssetIndex);
				bModified = true;
			}
		}
		
		
	}

	// adding the remaining AnimationAsset(s) from AnimationAssetsSet
	for (const FPoseSearchDatabaseAnimationAsset& AnimationAsset : NewAnimationAssets)
	{
		AddAnimationAsset(AnimationAsset);
		bModified = true;
	}

	if (bModified)
	{
		Modify();
		NotifySynchronizeWithExternalDependencies();
	}
}

void UFlexiblePoseSearchData::SynchronizeWithNotifyState()
{
	TArray<UAnimSequenceBase*> SequencesBase;

	const UAnimNotifyState_FlexiblePoseSearch* OwnerNotify =
		Cast<UAnimNotifyState_FlexiblePoseSearch>(GetOuter());
	if (!OwnerNotify)
	{
		return;
	}

	if (UAnimSequenceBase* SequenceBase = Cast<UAnimSequenceBase>(OwnerNotify->GetOuter()))
	{
		SequencesBase.AddUnique(SequenceBase);
	}

	// PoseSearchSet partners' FlexiblePoseSearch notifies point at *their* PSD, not this one — still pull
	// their BranchIn windows into THIS DB so Seed∪Set share one searchable pair.
	for (const TObjectPtr<UAnimSequenceBase>& SetAnim : OwnerNotify->PoseSearchSet)
	{
		if (UAnimSequenceBase* Sequence = SetAnim.Get())
		{
			SequencesBase.AddUnique(Sequence);
		}
	}

	if (SequencesBase.IsEmpty())
	{
		return;
	}

	// Rebuild sync-managed entries: for each sequence, prefer the FlexiblePoseSearch notify whose
	// PoseSearchData == this (seed), else that sequence's first FlexiblePoseSearch window (set partner).
	// This applies to NativePSD too: its standard Database is primary, while this
	// per-notify database remains the deterministic narrow-window fallback.
	TArray<FPoseSearchDatabaseAnimationAsset> NewAnimationAssets;
	const uint32 SharedBranchId = OwnerNotify->GetBranchInId();

	auto GetSamplingRange = [](const FAnimNotifyEvent& NotifyEvent, const UAnimSequenceBase* SequenceBase) -> FFloatInterval
	{
		FFloatInterval SamplingRange(NotifyEvent.GetTime(), NotifyEvent.GetTime() + NotifyEvent.GetDuration());
		if (SamplingRange.Min <= NotifyEvent.TriggerTimeOffset
			&& SamplingRange.Max >= SequenceBase->GetPlayLength() - NotifyEvent.TriggerTimeOffset)
		{
			SamplingRange = FFloatInterval(0.f, 0.f);
		}
		return SamplingRange;
	};

	for (UAnimSequenceBase* SequenceBase : SequencesBase)
	{
		if (!SequenceBase)
		{
			continue;
		}

		const FAnimNotifyEvent* ChosenNotify = nullptr;
		for (const FAnimNotifyEvent& NotifyEvent : SequenceBase->Notifies)
		{
			const UAnimNotifyState_FlexiblePoseSearch* FlexiblePoseSearch =
				Cast<UAnimNotifyState_FlexiblePoseSearch>(NotifyEvent.NotifyStateClass);
			if (!FlexiblePoseSearch)
			{
				continue;
			}
			if (FlexiblePoseSearch->PoseSearchData == this)
			{
				ChosenNotify = &NotifyEvent;
				break;
			}
			if (!ChosenNotify)
			{
				ChosenNotify = &NotifyEvent;
			}
		}

		if (!ChosenNotify)
		{
			continue;
		}

		FPoseSearchDatabaseAnimationAsset DatabaseAnimationAsset;
		DatabaseAnimationAsset.AnimAsset = SequenceBase;
		DatabaseAnimationAsset.SamplingRange = GetSamplingRange(*ChosenNotify, SequenceBase);
		DatabaseAnimationAsset.BranchInId = SharedBranchId;
		NewAnimationAssets.Add(DatabaseAnimationAsset);
	}

	// Preserve non-sync assets, replace sync-managed ones for SequencesBase.
	bool bModified = false;
	for (int32 AnimationAssetIndex = GetNumAnimationAssets() - 1; AnimationAssetIndex >= 0; --AnimationAssetIndex)
	{
		if (FPoseSearchDatabaseAnimationAsset* AnimationAsset = GetMutableDatabaseAnimationAsset(AnimationAssetIndex))
		{
			const bool bIsSyncedMember = SequencesBase.Contains(
				Cast<UAnimSequenceBase>(AnimationAsset->GetAnimationAsset()));
			if (!bIsSyncedMember)
			{
				continue;
			}

			int32 FoundIndex = -1;
			for (int32 i = 0; i < NewAnimationAssets.Num(); ++i)
			{
				if (AnimationAsset->UpdateFrom(NewAnimationAssets[i]))
				{
					FoundIndex = i;
					break;
				}
			}
			if (FoundIndex >= 0)
			{
				NewAnimationAssets.RemoveAt(FoundIndex);
			}
			else
			{
				RemoveAnimationAssetAt(AnimationAssetIndex);
				bModified = true;
			}
		}
	}

	for (const FPoseSearchDatabaseAnimationAsset& AnimationAsset : NewAnimationAssets)
	{
		AddAnimationAsset(AnimationAsset);
		bModified = true;
	}

	if (bModified)
	{
		Modify();
		NotifySynchronizeWithExternalDependencies();
	}
}
#endif
