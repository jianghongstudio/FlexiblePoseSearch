// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimNotifyState_FlexiblePoseSearch.h"
#include "PoseSearch/PoseSearchDerivedData.h"

void UAnimNotifyState_FlexiblePoseSearch::PostLoad()
{
	Super::PostLoad();

	// Existing NativePSD notifies serialized their source in an FlexiblePoseSearch-specific field.
	// Promote that reference once so the engine's BranchIn dependency synchronizer
	// sees this notify as a normal UAnimNotifyState_PoseSearchBranchIn subclass.
	if (SourceMode == EFlexiblePoseSearchSourceMode::NativePSD
		&& !Database
		&& NativePoseSearchDatabase)
	{
		Database = NativePoseSearchDatabase;
		NativePoseSearchDatabase = nullptr;
#if WITH_EDITOR
		if (!IsTemplate())
		{
			MarkPackageDirty();
		}
#endif
	}
}

#if WITH_EDITOR
void UAnimNotifyState_FlexiblePoseSearch::OnAnimNotifyCreatedInEditor(FAnimNotifyEvent& ContainingAnimNotifyEvent)
{
	using namespace UE::PoseSearch;
	PoseSearchData = NewObject<UFlexiblePoseSearchData>(this);
	if (UAnimSequence* AnimSequence = Cast<UAnimSequence>(GetOuter()))
	{
		FPoseSearchDatabaseAnimationAsset NewAsset;
		NewAsset.AnimAsset = AnimSequence;
		NewAsset.SamplingRange = FFloatInterval(0,AnimSequence->GetPlayLength());
		PoseSearchData->AddAnimationAsset(NewAsset);
	}
	else if (UAnimMontage* AnimMontage = Cast<UAnimMontage>(GetOuter()))
	{
		FPoseSearchDatabaseAnimationAsset NewAsset;
		NewAsset.AnimAsset = AnimMontage;
		NewAsset.SamplingRange = FFloatInterval(0,AnimMontage->GetPlayLength());
		PoseSearchData->AddAnimationAsset(NewAsset);
	}

	
	FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(PoseSearchData, ERequestAsyncBuildFlag::NewRequest);
	Super::OnAnimNotifyCreatedInEditor(ContainingAnimNotifyEvent);
}

bool UAnimNotifyState_FlexiblePoseSearch::CanBePlaced(UAnimSequenceBase* Animation) const
{
	if (!Animation)
	{
		return false;
	}

	if (Animation->IsA(UAnimMontage::StaticClass())|| Animation->IsA(UAnimSequence::StaticClass()))
	{
		return true;
	}
	return false;
}

void UAnimNotifyState_FlexiblePoseSearch::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UAnimNotifyState_FlexiblePoseSearch::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent)
{
	using namespace UE::PoseSearch;
	if (SourceMode == EFlexiblePoseSearchSourceMode::SmallLibrary
		&& PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UAnimNotifyState_FlexiblePoseSearch,PoseSearchData))
	{
		if (PropertyChangedEvent.PropertyChain.GetTail()->GetValue()->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimNotifyState_FlexiblePoseSearch,PoseSearchData))
		{
			if (PoseSearchData)
			{
				if (UAnimSequence* AnimSequence = Cast<UAnimSequence>(GetOuter()))
				{
					FPoseSearchDatabaseAnimationAsset NewAsset;
					NewAsset.AnimAsset = AnimSequence;
					NewAsset.SamplingRange = FFloatInterval(0,AnimSequence->GetPlayLength());
					PoseSearchData->AddAnimationAsset(NewAsset);
				}
				else if (UAnimMontage* AnimMontage = Cast<UAnimMontage>(GetOuter()))
				{
					FPoseSearchDatabaseAnimationAsset NewAsset;
					NewAsset.AnimAsset = AnimMontage;
					NewAsset.SamplingRange = FFloatInterval(0,AnimMontage->GetPlayLength());
					PoseSearchData->AddAnimationAsset(NewAsset);
				}

	
				FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(PoseSearchData, ERequestAsyncBuildFlag::NewRequest);
			}
		}
	}
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}
#endif

uint32 UAnimNotifyState_FlexiblePoseSearch::GetBranchInId() const
{
	if (SourceMode == EFlexiblePoseSearchSourceMode::NativePSD)
	{
		// The engine calls this through UAnimNotifyState_PoseSearchBranchIn while
		// synchronizing a standard PSD. Keep our direct callers on identical rules.
		return Super::GetBranchInId();
	}

	// Same PSD ⇒ same BranchInId so Seed∪Set members stay one BranchIn group.
	// Hashing notify FullName made L/R different IDs and broke paired foot search.
	const UObject* SearchSource = PoseSearchData.Get();
	const uint32 BranchInId = SearchSource
		? GetTypeHash(SearchSource)
		: GetTypeHash(GetFullName());
	check(BranchInId != 0);
	return BranchInId;
}

