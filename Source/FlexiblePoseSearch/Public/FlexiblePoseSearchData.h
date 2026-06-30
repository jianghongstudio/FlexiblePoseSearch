// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "FlexiblePoseSearchData.generated.h"

/**
 * PoseSearchDatabase specialized for FlexiblePoseSearch BranchIn / PoseSearchSet windows.
 *
 * Tiny per-notify pools still run Schema DataPreprocessor::Normalize against their own
 * feature Deviation (小库内归一化). After each index build, FlexiblePoseSearch rescales WeightsSqrt so
 * ||W|| matches Schema-only magnitude — preserving within-DB whitening without requiring
 * a project NormalizationSet, a larger search pool, or a custom Schema.
 *
 * If a project assigns NormalizationSet, sanitization is skipped (project owns variance).
 */
UCLASS(editinlinenew)
class FLEXIBLEPOSESEARCH_API UFlexiblePoseSearchData : public UPoseSearchDatabase
{
	GENERATED_BODY()

public:
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;

#if WITH_EDITOR
	/** Sync database assets from FlexiblePoseSearch notify states. Base SynchronizeWithExternalDependencies is non-virtual, so this is a plugin-side replacement. */
	void SynchronizeWithNotifyState();
#endif

private:
	/** Hook after index rebuild; intentionally leaves engine WeightsSqrt unchanged. */
	void SanitizeSearchIndexWeights();

#if WITH_EDITOR
	void EnsureWeightSanitizeHook();
	void TeardownWeightSanitizeHook();
	void OnSearchIndexRebuilt();

	bool bWeightSanitizeHooked = false;

	void SynchronizeWithFlexiblePoseSearchExternalDependencies(TConstArrayView<UAnimSequenceBase*> SequencesBase);
#endif
};
