// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimGraphNode_PoseSearchPlayer.h"

#include "AnimGraphCommands.h"
#include "DetailLayoutBuilder.h"
#include "FindInBlueprintManager.h"
#include "IAnimBlueprintNodeOverrideAssetsContext.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimPoseSearchProvider.h"
#include "Animation/AnimRootMotionProvider.h"

#define LOCTEXT_NAMESPACE "UAnimGraphNode_PoseSearchPlayer"

UAnimGraphNode_PoseSearchPlayer::UAnimGraphNode_PoseSearchPlayer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAnimGraphNode_PoseSearchPlayer::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);

	if(Ar.IsLoading() && Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::AnimNodeConstantDataRefactorPhase0)
	{
		Node.PlayRateScaleBiasClampConstants.CopyFromLegacy(Node.PlayRateScaleBiasClamp_DEPRECATED);
	}
}

FLinearColor UAnimGraphNode_PoseSearchPlayer::GetNodeTitleColor() const
{
	UAnimSequenceBase* Sequence = Node.GetSequence();
	if ((Sequence != NULL) && Sequence->IsValidAdditive())
	{
		return FLinearColor(0.10f, 0.60f, 0.12f);
	}
	else
	{
		return FColor(200, 100, 100);
	}
}

FSlateIcon UAnimGraphNode_PoseSearchPlayer::GetIconAndTint(FLinearColor& OutColor) const
{
	return FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.AnimSequence");
}

FText UAnimGraphNode_PoseSearchPlayer::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	UEdGraphPin* SequencePin = FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_PoseSearchPlayer, Sequence));
	return GetNodeTitleHelper(TitleType, SequencePin, LOCTEXT("PlayerDesc", "PoseSearch Player"),
		[](UAnimationAsset* InAsset)
		{
			UAnimSequenceBase* SequenceBase = CastChecked<UAnimSequenceBase>(InAsset);
			const bool bAdditive = SequenceBase->IsValidAdditive();
			return bAdditive ? LOCTEXT("AdditivePostFix", "(additive)") : FText::GetEmpty();
		});
}

FText UAnimGraphNode_PoseSearchPlayer::GetMenuCategory() const
{
	return LOCTEXT("MenuCategory", "Animation|Sequences");
}

void UAnimGraphNode_PoseSearchPlayer::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None);

	// Reconstruct node to show updates to PinFriendlyNames.
	if ((PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_PoseSearchPlayer, PlayRateBasis))
		|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClampConstants, bMapRange))
		|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputRange, Min))
		|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputRange, Max))
		|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClampConstants, Scale))
		|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClampConstants, Bias))
		|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClampConstants, bClampResult))
		|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClampConstants, ClampMin))
		|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClampConstants, ClampMax))
		|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClampConstants, bInterpResult))
		|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClampConstants, InterpSpeedIncreasing))
		|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClampConstants, InterpSpeedDecreasing)))
	{
		ReconstructNode();
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UAnimGraphNode_PoseSearchPlayer, PoseSearchInitialFunction))
	{
		Node.OnPoseSearchInitial.SetFromFunction(PoseSearchInitialFunction.ResolveMember<UFunction>(GetBlueprintClassFromNode()));
		GetAnimBlueprint()->RequestRefreshExtensions();
		GetSchema()->ReconstructNode(*this);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UAnimGraphNode_PoseSearchPlayer::AddSearchMetaDataInfo(TArray<struct FSearchTagDataPair>& OutTaggedMetaData) const
{
	Super::AddSearchMetaDataInfo(OutTaggedMetaData);

	const auto ConditionallyTagNodeFuncRef = [&OutTaggedMetaData](const FMemberReference& FuncMember, const FText& LocText)
	{
		if (IsPotentiallyBoundFunction(FuncMember))
		{
			const FText FunctionName = FText::FromName(FuncMember.GetMemberName());
			OutTaggedMetaData.Add(FSearchTagDataPair(LocText, FunctionName));
		}
	};

	// Conditionally include anim node function references as part of the node's search metadata
	ConditionallyTagNodeFuncRef(PoseSearchInitialFunction, LOCTEXT("PoseSearchInitialFunctionName", "On PoseSearch Initial"));
}

void UAnimGraphNode_PoseSearchPlayer::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	Super::CustomizeDetails(DetailBuilder);

	if (!UE::Anim::IPoseSearchProvider::IsAvailable())
	{
		DetailBuilder.HideCategory(TEXT("PoseMatching"));
	}
}

void UAnimGraphNode_PoseSearchPlayer::CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName,
	int32 ArrayIndex) const
{
	Super::CustomizePinData(Pin, SourcePropertyName, ArrayIndex);

	if (Pin->PinName == GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_PoseSearchPlayer, PlayRate))
	{
		if (!Pin->bHidden)
		{
			// Draw value for PlayRateBasis if the pin is not exposed
			UEdGraphPin* PlayRateBasisPin = FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_PoseSearchPlayer, PlayRateBasis));
			if (!PlayRateBasisPin || PlayRateBasisPin->bHidden)
			{
				if (Node.GetPlayRateBasis() != 1.f)
				{
					FFormatNamedArguments Args;
					Args.Add(TEXT("PinFriendlyName"), Pin->PinFriendlyName);
					Args.Add(TEXT("PlayRateBasis"), FText::AsNumber(Node.GetPlayRateBasis()));
					Pin->PinFriendlyName = FText::Format(LOCTEXT("FAnimNode_PoseSearchPlayer_PlayRateBasis_Value", "({PinFriendlyName} / {PlayRateBasis})"), Args);
				}
			}
			else // PlayRateBasisPin is visible
			{
				FFormatNamedArguments Args;
				Args.Add(TEXT("PinFriendlyName"), Pin->PinFriendlyName);
				Pin->PinFriendlyName = FText::Format(LOCTEXT("FAnimNode_PoseSearchPlayer_PlayRateBasis_Name", "({PinFriendlyName} / PlayRateBasis)"), Args);
			}

			Pin->PinFriendlyName = Node.GetPlayRateScaleBiasClampConstants().GetFriendlyName(Pin->PinFriendlyName);
		}
	}
}

void UAnimGraphNode_PoseSearchPlayer::ValidateAnimNodeDuringCompilation(class USkeleton* ForSkeleton,
	class FCompilerResultsLog& MessageLog)
{
	Super::ValidateAnimNodeDuringCompilation(ForSkeleton, MessageLog);

	ValidateAnimNodeDuringCompilationHelper(ForSkeleton, MessageLog, Node.GetSequence(), UAnimSequenceBase::StaticClass(), FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_PoseSearchPlayer, Sequence)), GET_MEMBER_NAME_CHECKED(FAnimNode_PoseSearchPlayer, Sequence));
	ValidateFunctionRef(GET_MEMBER_NAME_CHECKED(UAnimGraphNode_PoseSearchPlayer, PoseSearchInitialFunction), PoseSearchInitialFunction, LOCTEXT("PoseSearchInitialFunctionName", "On PoseSearch Initial"), MessageLog);
}

void UAnimGraphNode_PoseSearchPlayer::PreloadRequiredAssets()
{
	PreloadRequiredAssetsHelper(Node.GetSequence(), FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_PoseSearchPlayer, Sequence)));

	Super::PreloadRequiredAssets();
}

void UAnimGraphNode_PoseSearchPlayer::BakeDataDuringCompilation(class FCompilerResultsLog& MessageLog)
{
	UAnimBlueprint* AnimBlueprint = GetAnimBlueprint();
	AnimBlueprint->FindOrAddGroup(Node.GetGroupName());
}

bool UAnimGraphNode_PoseSearchPlayer::DoesSupportTimeForTransitionGetter() const
{
	return true;
}

UAnimationAsset* UAnimGraphNode_PoseSearchPlayer::GetAnimationAsset() const
{
	UAnimSequenceBase* Sequence = Node.GetSequence();
	UEdGraphPin* SequencePin = FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_PoseSearchPlayer, Sequence));
	if (SequencePin != nullptr && Sequence == nullptr)
	{
		Sequence = Cast<UAnimSequenceBase>(SequencePin->DefaultObject);
	}

	return Sequence;
}

const TCHAR* UAnimGraphNode_PoseSearchPlayer::GetTimePropertyName() const
{
	return TEXT("InternalTimeAccumulator");
}

UScriptStruct* UAnimGraphNode_PoseSearchPlayer::GetTimePropertyStruct() const
{
	return FAnimNode_PoseSearchPlayer::StaticStruct();
}

void UAnimGraphNode_PoseSearchPlayer::GetAllAnimationSequencesReferred(TArray<UAnimationAsset*>& AnimationAssets) const
{
	if(Node.GetSequence())
	{
		HandleAnimReferenceCollection(Node.Sequence, AnimationAssets);
	}
}

void UAnimGraphNode_PoseSearchPlayer::ReplaceReferredAnimations(
	const TMap<UAnimationAsset*, UAnimationAsset*>& AnimAssetReplacementMap)
{
	HandleAnimReferenceReplacement(Node.Sequence, AnimAssetReplacementMap);
}

void UAnimGraphNode_PoseSearchPlayer::GetMenuActions(FBlueprintActionDatabaseRegistrar& InActionRegistrar) const
{
	GetMenuActionsHelper(
		InActionRegistrar,
		GetClass(),
		{ UAnimSequence::StaticClass(), UAnimComposite::StaticClass() },
		{ },
		[](const FAssetData& InAssetData, UClass* InClass)
		{
			if(InAssetData.IsValid())
			{
				const FString TagValue = InAssetData.GetTagValueRef<FString>(GET_MEMBER_NAME_CHECKED(UAnimSequence, AdditiveAnimType));
				if(const bool bKnownToBeAdditive = (!TagValue.IsEmpty() && !TagValue.Equals(TEXT("AAT_None"))))
				{
					return FText::Format(LOCTEXT("MenuDescFormat_PlayAdditive", "Play '{0}' (additive)"), FText::FromName(InAssetData.AssetName));
				}
				else
				{
					return FText::Format(LOCTEXT("MenuDescFormat_Play1", "Play '{0}'"), FText::FromName(InAssetData.AssetName));
				}
			}
			else
			{
				return LOCTEXT("PlayerDesc", "PoseSearch Player");
			}
		},
		[](const FAssetData& InAssetData, UClass* InClass)
		{
			if(InAssetData.IsValid())
			{
				const FString TagValue = InAssetData.GetTagValueRef<FString>(GET_MEMBER_NAME_CHECKED(UAnimSequence, AdditiveAnimType));
				if(const bool bKnownToBeAdditive = (!TagValue.IsEmpty() && !TagValue.Equals(TEXT("AAT_None"))))
				{
					return FText::Format(LOCTEXT("MenuDescTooltipFormat_PlayAdded", "Play (additive)\n'{0}'"), FText::FromString(InAssetData.GetObjectPathString()));
				}
				else
				{
					return FText::Format(LOCTEXT("MenuDescTooltipFormat_Play", "Play\n'{0}'"), FText::FromString(InAssetData.GetObjectPathString()));
				}
			}
			else
			{
				return LOCTEXT("PlayerDescTooltip", "PoseSearch Player");
			}
		},
		[](UEdGraphNode* InNewNode, bool bInIsTemplateNode, const FAssetData InAssetData)
		{
			UAnimGraphNode_AssetPlayerBase::SetupNewNode(InNewNode, bInIsTemplateNode, InAssetData);
		});	
}

EAnimAssetHandlerType UAnimGraphNode_PoseSearchPlayer::SupportsAssetClass(const UClass* AssetClass) const
{
	if (AssetClass->IsChildOf(UAnimSequence::StaticClass()) || AssetClass->IsChildOf(UAnimComposite::StaticClass()))
	{
		return EAnimAssetHandlerType::PrimaryHandler;
	}
	else
	{
		return EAnimAssetHandlerType::NotSupported;
	}
}

void UAnimGraphNode_PoseSearchPlayer::OnOverrideAssets(IAnimBlueprintNodeOverrideAssetsContext& InContext) const
{
	if(InContext.GetAssets().Num() > 0)
	{
		if (UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(InContext.GetAssets()[0]))
		{
			FAnimNode_PoseSearchPlayer& AnimNode = InContext.GetAnimNode<FAnimNode_PoseSearchPlayer>();
			AnimNode.SetSequence(Sequence);
		}
	}
}

void UAnimGraphNode_PoseSearchPlayer::GetOutputLinkAttributes(FNodeAttributeArray& OutAttributes) const
{
	Super::GetOutputLinkAttributes(OutAttributes);

	if (UE::Anim::IAnimRootMotionProvider::Get())
	{
		OutAttributes.Add(UE::Anim::IAnimRootMotionProvider::AttributeName);
	}
}

void UAnimGraphNode_PoseSearchPlayer::OnProcessDuringCompilation(IAnimBlueprintCompilationContext& InCompilationContext,
	IAnimBlueprintGeneratedClassCompiledData& OutCompiledData)
{
	Super::OnProcessDuringCompilation(InCompilationContext, OutCompiledData);
	Node.OnPoseSearchInitial.SetFromFunction(PoseSearchInitialFunction.ResolveMember<UFunction>(GetBlueprintClassFromNode()));
}

void UAnimGraphNode_PoseSearchPlayer::GetBoundFunctionsInfo(TArray<TPair<FName, FName>>& InOutBindingsInfo)
{
	Super::GetBoundFunctionsInfo(InOutBindingsInfo);

	const FName CategoryName = TEXT("Functions|Pose Search");

	if (PoseSearchInitialFunction.ResolveMember<UFunction>(GetBlueprintClassFromNode()) != nullptr)
	{
		InOutBindingsInfo.Emplace(CategoryName, GET_MEMBER_NAME_CHECKED(UAnimGraphNode_PoseSearchPlayer, PoseSearchInitialFunction));
	}
}

bool UAnimGraphNode_PoseSearchPlayer::ReferencesFunction(const FName& InFunctionName, const UStruct* InScope) const
{
	return Super::ReferencesFunction(InFunctionName, InScope)
		|| Node.OnPoseSearchInitial.GetFunctionName() == InFunctionName;
}

void UAnimGraphNode_PoseSearchPlayer::GetNodeContextMenuActions(class UToolMenu* Menu,
                                                                class UGraphNodeContextMenuContext* Context) const
{
	if (!Context->bIsDebugging)
	{
		// add an option to convert to single frame
		{
			FToolMenuSection& Section = Menu->AddSection("AnimGraphNodePoseSearchPlayer", LOCTEXT("PoseSearchPlayerHeading", "PoseSearch Player"));
			Section.AddMenuEntry(FAnimGraphCommands::Get().OpenRelatedAsset);
			Section.AddMenuEntry(FAnimGraphCommands::Get().ConvertToSeqEvaluator);
		}
	}
}

void UAnimGraphNode_PoseSearchPlayer::SetAnimationAsset(UAnimationAsset* Asset)
{
	if (UAnimSequenceBase* Seq = Cast<UAnimSequenceBase>(Asset))
	{
		Node.SetSequence(Seq);
	}
}

void UAnimGraphNode_PoseSearchPlayer::CopySettingsFromAnimationAsset(UAnimationAsset* Asset)
{
	if (UAnimSequenceBase* Seq = Cast<UAnimSequenceBase>(Asset))
	{
		Node.SetLoopAnimation(Seq->bLoop);
	}
}

#undef LOCTEXT_NAMESPACE
