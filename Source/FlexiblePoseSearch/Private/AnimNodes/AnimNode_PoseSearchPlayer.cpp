#include "AnimNodes/AnimNode_PoseSearchPlayer.h"

#include "AnimNotifyState_FlexiblePoseSearch.h"
#include "Animation/AnimPoseSearchProvider.h"
#include "Animation/AnimTrace.h"
#include "Animation/AnimExecutionContext.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_Inertialization.h"
#include "FlexiblePoseSearchLibrary.h"

void FAnimNode_PoseSearchPlayer::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Initialize_AnyThread);

	UE::Anim::FNodeFunctionCaller::CallFunction(GetPoseSearchInitialFunction(), Context, *this);
	
	FAnimNode_AssetPlayerBase::Initialize_AnyThread(Context);

	GetEvaluateGraphExposedInputs().Execute(Context);

	UAnimSequenceBase* CurrentSequence = GetSequence();
	if (CurrentSequence && !ensureMsgf(!CurrentSequence->IsA<UAnimMontage>(), TEXT("Sequence players do not support anim montages.")))
	{
		CurrentSequence = nullptr;
	}

	InternalTimeAccumulator = GetStartPosition();
	PlayRateScaleBiasClampState.Reinitialize();

	               
	if (CurrentSequence != nullptr)
	{
		const float EffectiveStartPosition = GetStartPositionFromPoseMatching(Context);
		const float CurrentPlayRate = GetPlayRate();
		const float CurrentPlayRateBasis = GetPlayRateBasis();

		InternalTimeAccumulator = FMath::Clamp(EffectiveStartPosition, 0.f, CurrentSequence->GetPlayLength());
		const float AdjustedPlayRate = PlayRateScaleBiasClampState.ApplyTo(GetPlayRateScaleBiasClampConstants(), FMath::IsNearlyZero(CurrentPlayRateBasis) ? 0.f : (CurrentPlayRate / CurrentPlayRateBasis), 0.f);
		const float EffectivePlayrate = CurrentSequence->RateScale * AdjustedPlayRate;
		if ((EffectiveStartPosition == 0.f) && (EffectivePlayrate < 0.f))
		{
			InternalTimeAccumulator = CurrentSequence->GetPlayLength();
		}
	}
}

const FAnimNodeFunctionRef& FAnimNode_PoseSearchPlayer::GetPoseSearchInitialFunction() const
{
	return GET_ANIM_NODE_DATA(FAnimNodeFunctionRef, OnPoseSearchInitial);
}

bool FAnimNode_PoseSearchPlayer::GetUseInertialBlending() const
{
	return GET_ANIM_NODE_DATA(bool, bUseInertialBlending);
}

float FAnimNode_PoseSearchPlayer::GetInertialBlendTime() const
{
	return GET_ANIM_NODE_DATA(float, InertialBlendTime);
}

void FAnimNode_PoseSearchPlayer::GatherPoseSearchAssets(const UAnimSequenceBase* CurrentSequence, TArray<const UObject*, TInlineAllocator<128>>& OutChosenAssets) const
{
	TArray<UObject*> Assets;
	UFlexiblePoseSearchLibrary::GatherPoseSearchAssets(CurrentSequence, Assets);
	for (UObject* Asset : Assets)
	{
		OutChosenAssets.AddUnique(Asset);
	}
}

float FAnimNode_PoseSearchPlayer::GetStartPositionFromPoseMatching(const FAnimationBaseContext& Context) const
{
	using namespace UE::Anim;

	// Override the start position if pose matching is enabled
	const UAnimSequenceBase* CurrentSequence = GetSequence();
	if (CurrentSequence != nullptr && GetStartFromMatchingPose())
	{
		if (const IPoseSearchProvider* PoseSearchProvider = IPoseSearchProvider::Get())
		{
			TArray<const UObject*, TInlineAllocator<128>> ChosenAssets;
			GatherPoseSearchAssets(CurrentSequence, ChosenAssets);
			
			const IPoseSearchProvider::FSearchResult Result = PoseSearchProvider->Search(Context, ChosenAssets,
				IPoseSearchProvider::FSearchPlayingAsset(), IPoseSearchProvider::FSearchFutureAsset());
			if (Result.SelectedAsset != nullptr)
			{
				return Result.TimeOffsetSeconds;
			}
		}
	}

	return GetStartPosition();
}

void FAnimNode_PoseSearchPlayer::SyncPlayingTimeFromPoseMatching(const FAnimationBaseContext& Context)
{
	using namespace UE::Anim;

	// Override the start position if pose matching is enabled
	const UAnimSequenceBase* CurrentSequence = GetSequence();
	if (CurrentSequence != nullptr && GetStartFromMatchingPose())
	{
		if (const IPoseSearchProvider* PoseSearchProvider = IPoseSearchProvider::Get())
		{
			TArray<const UObject*, TInlineAllocator<128>> ChosenAssets;
			GatherPoseSearchAssets(CurrentSequence, ChosenAssets);
			
			const IPoseSearchProvider::FSearchResult Result = PoseSearchProvider->Search(Context, ChosenAssets,
				IPoseSearchProvider::FSearchPlayingAsset(), IPoseSearchProvider::FSearchFutureAsset());
			if (Result.SelectedAsset != nullptr)
			{
				InternalTimeAccumulator = FMath::Clamp(Result.TimeOffsetSeconds, 0.f, CurrentSequence->GetPlayLength());
			}
		}
	}
}

void FAnimNode_PoseSearchPlayer::UpdateAnimAssetFromPoseMatching(const FAnimationBaseContext& Context, float BlendTime, const UAnimSequenceBase* NewAnimSequence)
{
	using namespace UE::Anim;

	// Update animation asset based on pose matching
	const UAnimSequenceBase* CurrentSequence = GetSequence();
	const UAnimSequenceBase* TargetSequence = NewAnimSequence? NewAnimSequence : CurrentSequence;
	if (TargetSequence != nullptr && GetStartFromMatchingPose())
	{
		if (const IPoseSearchProvider* PoseSearchProvider = IPoseSearchProvider::Get())
		{
			TArray<const UObject*, TInlineAllocator<128>> ChosenAssets;
			GatherPoseSearchAssets(TargetSequence, ChosenAssets);
			
			const IPoseSearchProvider::FSearchResult Result = PoseSearchProvider->Search(Context, ChosenAssets,
				IPoseSearchProvider::FSearchPlayingAsset(), IPoseSearchProvider::FSearchFutureAsset());
			if (Result.SelectedAsset != nullptr)
			{
				// Update the animation asset based on the search result
				if (const UAnimSequenceBase* SelectedSequence = Cast<UAnimSequenceBase>(Result.SelectedAsset))
				{
					const bool bAnimSequenceChanged = (CurrentSequence != SelectedSequence);
					
					// Set the new sequence
					SetSequence(const_cast<UAnimSequenceBase*>(SelectedSequence));
					InternalTimeAccumulator = FMath::Clamp(Result.TimeOffsetSeconds, 0.f, SelectedSequence->GetPlayLength());
					UE_VLOG(Context.GetAnimInstanceObject(),TEXT("FlexiblePoseSearch"),Log,TEXT("Previous Anim: %s, New Anim: %s, AnimPose:  %f"),*GetNameSafe(CurrentSequence),*GetNameSafe(SelectedSequence),InternalTimeAccumulator);
					
					// Apply inertial blending if sequence changed and blend time is valid
					if (bAnimSequenceChanged && BlendTime > 0.0f)
					{
						// Inertial blending requires FAnimUpdateContext
						{
							if (IInertializationRequester* InertializationRequester = Context.GetMessage<IInertializationRequester>())
							{
								FInertializationRequest Request;
								Request.Duration = BlendTime;
#if ANIM_TRACE_ENABLED
								Request.NodeId = Context.GetCurrentNodeId();
								Request.AnimInstance = Context.AnimInstanceProxy->GetAnimInstanceObject();
#endif
								InertializationRequester->RequestInertialization(Request);
							}
						}
					}
				}
			}
		}
	}
}