// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FlexiblePoseSearchEditorLibrary.generated.h"

class UPoseSearchDatabase;

/**
 * Editor utilities for synchronizing standard PoseSearch databases that contain
 * FlexiblePoseSearch BranchIn notify windows.
 */
UCLASS()
class FLEXIBLEPOSESEARCHEDITOR_API UFlexiblePoseSearchEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Refreshes external BranchIn entries, including SamplingRange and BranchInId.
	 * Call this before rebuilding an index after a notify class rename.
	 */
	UFUNCTION(BlueprintCallable, Category = "Flexible Pose Search|Editor")
	static bool SynchronizeNativePoseSearchDatabase(UPoseSearchDatabase* Database);
};
