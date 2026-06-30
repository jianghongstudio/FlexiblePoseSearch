// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimationModifier_AddPoseSearch.h"

#include "AnimNotifyState_FlexiblePoseSearch.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"


void UAnimationModifier_AddPoseSearch::OnApply_Implementation(UAnimSequence* AnimationSequence)
{
	Super::OnApply_Implementation(AnimationSequence);

	if (NotifyState && AnimationSequence)
	{
		FAnimNotifyEvent* NotifyEventPtr = nullptr;
		for (FAnimNotifyEvent& NotifyEvent : AnimationSequence->Notifies)
		{
			if (NotifyEvent.NotifyStateClass && NotifyEvent.NotifyStateClass->IsA(UAnimNotifyState_FlexiblePoseSearch::StaticClass()))
			{
				NotifyEventPtr = &NotifyEvent;
				break;
			}
		}

		const float StartTime = SampleRange.X > 0? SampleRange.X : 0.f;
		const float EndTime = SampleRange.Y > 0? SampleRange.Y : AnimationSequence->GetPlayLength();
		const float Duration = FMath::Max(EndTime - StartTime, 0.f);
		if (NotifyEventPtr)
		{
			if (NotifyEventPtr->GetTime() != StartTime || NotifyEventPtr->GetDuration() != Duration)
			{
				NotifyEventPtr->SetTime(StartTime);
				NotifyEventPtr->SetDuration(Duration);
				AnimationSequence->RefreshCacheData();
			}
		}
		else
		{
			UAnimNotifyState* NewNotifyState = NewObject<UAnimNotifyState>(AnimationSequence, UAnimNotifyState_FlexiblePoseSearch::StaticClass(), NAME_None, RF_Transactional, NotifyState.Get());
			UAnimationBlueprintLibrary::AddAnimationNotifyTrack(AnimationSequence, TEXT("PoseSearch"));
			UAnimationBlueprintLibrary::AddAnimationNotifyStateEventObject(AnimationSequence,StartTime, Duration, NewNotifyState,TEXT("PoseSearch"));
		}
	}
}

void UAnimationModifier_AddPoseSearch::OnRevert_Implementation(UAnimSequence* AnimationSequence)
{
	Super::OnRevert_Implementation(AnimationSequence);
	if (AnimationSequence)
	{
		AnimationSequence->Notifies.RemoveAll([](const FAnimNotifyEvent& Event)
		{
			return Event.NotifyStateClass && Event.NotifyStateClass->IsA(UAnimNotifyState_FlexiblePoseSearch::StaticClass());
		});
		AnimationSequence->RefreshCacheData();
	}
}
