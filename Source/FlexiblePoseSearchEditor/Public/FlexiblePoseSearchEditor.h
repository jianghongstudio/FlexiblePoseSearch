// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FFlexiblePoseSearchEditorModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static inline FFlexiblePoseSearchEditorModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FFlexiblePoseSearchEditorModule>("FlexiblePoseSearchEditor");
	};
private:

	void RegisterCustomClassLayout(FName ClassName, FOnGetDetailCustomizationInstance DetailLayoutDelegate );
	void RegisterCustomPropertyTypeLayout(FName PropertyTypeName, FOnGetPropertyTypeCustomizationInstance DetailLayoutDelegate);

private:

	TArray<FName> RegisteredPropertyCustomizations;
	TArray<FName> RegisteredClassNames;
};
