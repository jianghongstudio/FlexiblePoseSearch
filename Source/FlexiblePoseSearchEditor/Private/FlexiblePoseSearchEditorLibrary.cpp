// Copyright Epic Games, Inc. All Rights Reserved.

#include "FlexiblePoseSearchEditorLibrary.h"

#include "PoseSearch/PoseSearchDatabase.h"

bool UFlexiblePoseSearchEditorLibrary::SynchronizeNativePoseSearchDatabase(UPoseSearchDatabase* Database)
{
#if WITH_EDITOR
	if (!Database)
	{
		return false;
	}

	Database->Modify();
	Database->SynchronizeWithExternalDependencies();
	return true;
#else
	return false;
#endif
}
