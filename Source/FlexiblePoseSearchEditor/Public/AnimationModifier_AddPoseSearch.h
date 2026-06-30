// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AnimationModifier.h"
#include "AnimationModifier_AddPoseSearch.generated.h"

/**
 * 
 */
UCLASS()
class FLEXIBLEPOSESEARCHEDITOR_API UAnimationModifier_AddPoseSearch : public UAnimationModifier
{
	GENERATED_BODY()
	
public:

	virtual void OnApply_Implementation(UAnimSequence* AnimationSequence) override;
	virtual void OnRevert_Implementation(UAnimSequence* AnimationSequence) override;
	
protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PoseSearch", Instanced)
	TObjectPtr<class UAnimNotifyState_FlexiblePoseSearch> NotifyState;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PoseSearch")
	FVector2D SampleRange = FVector2D(-1.f, -1.f);
};
