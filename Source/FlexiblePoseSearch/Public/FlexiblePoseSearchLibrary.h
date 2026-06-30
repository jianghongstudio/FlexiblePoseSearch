// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SequencePlayerLibrary.h"
#include "Animation/AnimExecutionContext.h"
#include "Animation/AnimNodeReference.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FlexiblePoseSearchLibrary.generated.h"

class UAnimSequenceBase;

/**
 * PoseSearch helpers. Gather matches FAnimNode_PoseSearchPlayer:
 * Seed notify's UFlexiblePoseSearchData only (DB authored/synced as Seed∪PoseSearchSet).
 */
UCLASS()
class FLEXIBLEPOSESEARCH_API UFlexiblePoseSearchLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	UFUNCTION(BlueprintInternalUseOnly, meta = (BlueprintThreadSafe))
	void Prototype_ThreadSafeAnimInitCall(const FAnimInitializationContext& Context, const FAnimNodeReference& Node) {}
#endif

	UFUNCTION(BlueprintCallable, Category = "Animation|Sequences", meta = (BlueprintThreadSafe))
	static FSequencePlayerReference SyncPlayingTimeFromPoseMatching(const FAnimUpdateContext& UpdateContext, const FSequencePlayerReference& SequencePlayer);

	/** Update animation asset from pose matching with optional inertial blending */
	UFUNCTION(BlueprintCallable, Category = "Animation|Sequences", meta = (BlueprintThreadSafe))
	static FSequencePlayerReference UpdateAnimAssetFromPoseMatching(const FAnimUpdateContext& UpdateContext, const FSequencePlayerReference& SequencePlayer, float BlendTime = 0.2f, const UAnimSequenceBase* NewAnimSequence = nullptr);

	/**
	 * Emits the primary candidate source selected by the Seed's FlexiblePoseSearch notify.
	 *
	 * NativePSD defaults to Seed + PoseSearchSet sequences so the engine resolves
	 * their BranchIn windows in the shared database. It emits the standard database
	 * directly only when the notify explicitly selects entire-database search.
	 */
	static void GatherPoseSearchAssets(const UAnimSequenceBase* Seed, TArray<UObject*>& OutAssets);

	/**
	 * Resolves primary and fallback candidate sources for the Seed's FlexiblePoseSearch notify.
	 *
	 * NativePSD is primary (either BranchIn-restricted sequences or the explicitly
	 * requested entire database) and the same notify's SmallLibrary database is
	 * fallback. SmallLibrary only produces a primary tier. Callers that can retry
	 * MotionMatch should use this overload to make fallback deterministic.
	 */
	static void GatherPoseSearchAssetTiers(
		const UAnimSequenceBase* Seed,
		TArray<UObject*>& OutPrimaryAssets,
		TArray<UObject*>& OutFallbackAssets);
};
