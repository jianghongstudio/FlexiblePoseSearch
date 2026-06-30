// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FlexiblePoseSearchData.h"
#include "PoseSearch/PoseSearchAnimNotifies.h"
#include "AnimNotifyState_FlexiblePoseSearch.generated.h"

UENUM(BlueprintType)
enum class EFlexiblePoseSearchSourceMode : uint8
{
	SmallLibrary UMETA(DisplayName = "Small Library"),
	NativePSD UMETA(DisplayName = "Native Pose Search Database"),
};

/**
 * Defines the NativePSD candidate scope selected by a FlexiblePoseSearch BranchIn notify.
 *
 * A restricted scope mirrors the engine's Sequence + BranchIn route: the Seed
 * and its authored PoseSearchSet are resolved into the shared database, then
 * only those sequences' BranchIn windows can be selected.
 */
UENUM(BlueprintType)
enum class EFlexiblePoseSearchNativeSearchScope : uint8
{
	SeedAndPoseSearchSet UMETA(DisplayName = "Seed + Pose Search Set"),
	EntireDatabase UMETA(DisplayName = "Entire Database"),
};

/**
 * Selects the Motion Matching candidate source for this seed.
 *
 * SmallLibrary searches the per-notify UFlexiblePoseSearchData database authored from
 * PoseSearchSet. NativePSD either resolves the Seed/PoseSearchSet through standard
 * BranchIn semantics or explicitly searches the full standard database; both can
 * fall back to the SmallLibrary database when their search fails.
 */
UCLASS()
class FLEXIBLEPOSESEARCH_API UAnimNotifyState_FlexiblePoseSearch : public UAnimNotifyState_PoseSearchBranchIn
{
	GENERATED_BODY()
	
public:
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void OnAnimNotifyCreatedInEditor(FAnimNotifyEvent& ContainingAnimNotifyEvent) override;
	virtual bool CanBePlaced(UAnimSequenceBase* Animation) const override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

	uint32 GetBranchInId() const;

	UPROPERTY(EditAnywhere, Category = Settings)
	EFlexiblePoseSearchSourceMode SourceMode = EFlexiblePoseSearchSourceMode::SmallLibrary;
	
	/** NativePSD is restricted to Seed + PoseSearchSet unless full-database search is explicitly requested. */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (EditCondition = "SourceMode == EFlexiblePoseSearchSourceMode::NativePSD", EditConditionHides))
	EFlexiblePoseSearchNativeSearchScope NativeSearchScope = EFlexiblePoseSearchNativeSearchScope::SeedAndPoseSearchSet;

	UPROPERTY(EditAnywhere, Category = Settings, Instanced, meta = (EditCondition = "SourceMode == EFlexiblePoseSearchSourceMode::SmallLibrary", EditConditionHides));
	TObjectPtr<UFlexiblePoseSearchData> PoseSearchData;

	/**
	 * Legacy NativePSD reference. PostLoad migrates it to the inherited Database
	 * property, which is the sole BranchIn source for the native engine synchronizer.
	 */
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use the inherited Database property."))
	TObjectPtr<UPoseSearchDatabase> NativePoseSearchDatabase;
	
	UPROPERTY(EditAnywhere, Category = Settings);
	TArray<TObjectPtr<UAnimSequenceBase>> PoseSearchSet;
};
