// Copyright Epic Games, Inc. All Rights Reserved.

#include "FlexiblePoseSearchEditor.h"


#define LOCTEXT_NAMESPACE "FFlexiblePoseSearchEditorModule"

void FFlexiblePoseSearchEditorModule::StartupModule()
{
}

void FFlexiblePoseSearchEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

		// Unregister all classes customized by name
		for (auto It = RegisteredClassNames.CreateConstIterator(); It; ++It)
		{
			if (It->IsValid())
			{
				PropertyModule.UnregisterCustomClassLayout(*It);
			}
		}

		for (auto It = RegisteredPropertyCustomizations.CreateConstIterator(); It; ++It)
		{
			if (It->IsValid())
			{
				PropertyModule.UnregisterCustomPropertyTypeLayout(*It);
			}
		}
	
		PropertyModule.NotifyCustomizationModuleChanged();
	}
}

void FFlexiblePoseSearchEditorModule::RegisterCustomClassLayout(FName ClassName,
                                                              FOnGetDetailCustomizationInstance DetailLayoutDelegate)
{
	check(ClassName != NAME_None);

	RegisteredClassNames.Add(ClassName);

	static FName PropertyEditor("PropertyEditor");
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditor);
	PropertyModule.RegisterCustomClassLayout( ClassName, DetailLayoutDelegate );
}

void FFlexiblePoseSearchEditorModule::RegisterCustomPropertyTypeLayout(FName PropertyTypeName,
	FOnGetPropertyTypeCustomizationInstance DetailLayoutDelegate)
{
	check(PropertyTypeName != NAME_None);

	RegisteredPropertyCustomizations.Add(PropertyTypeName);
	
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout(PropertyTypeName, DetailLayoutDelegate);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FFlexiblePoseSearchEditorModule, FlexiblePoseSearchEditor)