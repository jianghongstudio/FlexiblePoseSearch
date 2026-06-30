#pragma once
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "AnimNode_PoseSearchPlayer.generated.h"

struct FAnimUpdateContext;

USTRUCT(BlueprintInternalUseOnly)
struct FLEXIBLEPOSESEARCH_API FAnimNode_PoseSearchPlayer : public FAnimNode_SequencePlayer
{
	GENERATED_BODY()

public:

	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;

	const FAnimNodeFunctionRef& GetPoseSearchInitialFunction() const;

	bool GetUseInertialBlending() const;

	float GetInertialBlendTime() const;

	float GetStartPositionFromPoseMatching(const FAnimationBaseContext& Context) const;

	void SyncPlayingTimeFromPoseMatching(const FAnimationBaseContext& Context);

	void UpdateAnimAssetFromPoseMatching(const FAnimationBaseContext& Context, float BlendTime, const UAnimSequenceBase* NewAnimSequence = nullptr);
protected:
	friend class UAnimGraphNode_PoseSearchPlayer;

private:
	// Helper function to gather pose search assets from the current sequence
	void GatherPoseSearchAssets(const UAnimSequenceBase* CurrentSequence, TArray<const UObject*, TInlineAllocator<128>>& OutChosenAssets) const;

#if WITH_EDITORONLY_DATA
	
	UPROPERTY(meta=(FoldProperty))
	FAnimNodeFunctionRef OnPoseSearchInitial;

	// Use inertial blending when updating animation asset from pose matching
	UPROPERTY(EditAnywhere, Category = PoseMatching, meta = (PinHiddenByDefault, FoldProperty))
	bool bUseInertialBlending = false;

	// Blend time for inertial blending when updating animation asset
	UPROPERTY(EditAnywhere, Category = PoseMatching, meta = (PinHiddenByDefault, FoldProperty, EditCondition = "bUseInertialBlending"))
	float InertialBlendTime = 0.2f;

#endif // WITH_EDITORONLY_DATA
};
