// Property of Myrkur Games

#pragma once

#include "ActorIOHelperComponent.h"
#include "ActorIO.h"

UActorIOHelperComponent::UActorIOHelperComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

TArray<FActorIOFunction> UActorIOHelperComponent::GetFunctionNames()
{
	TArray<FActorIOFunction> FunctionNames;
	for (TFieldIterator<UFunction> FuncIt(GetClass(), EFieldIteratorFlags::IncludeSuper); FuncIt; ++FuncIt)
	{
		UFunction* Function = *FuncIt;
		if (Function->HasAnyFunctionFlags(FUNC_BlueprintCallable) && !BlacklistedFunctions.Contains(Function->GetName()))
		{
			// Get Class name without the suffix
			const FString Suffix = TEXT("_C");
			FString ClassName = GetClass()->GetName();
			if (ClassName.EndsWith(Suffix))
			{
				ClassName = ClassName.LeftChop(Suffix.Len());
			}

			FString ClassDisplayName = GetClass()->GetDisplayNameText().ToString().IsEmpty() ? ClassName : GetClass()->GetDisplayNameText().ToString();

			FString ValidName = Function->GetDisplayNameText().ToString().IsEmpty() ? Function->GetName() : Function->GetDisplayNameText().ToString();
			FText DisplayName = FText::FromString(FString::Printf(TEXT("%s::%s"), *ClassDisplayName, *ValidName));

			FActorIOFunction Info = FActorIOFunction()
									.SetId(FName(FString::Printf(TEXT("%s::%s"), *ClassName, *Function->GetName())))
									.SetDisplayName(DisplayName)
									.SetTooltipText(FText::FromString(Function->GetToolTipText().ToString()))
									.SetFunction(*Function->GetName())
									.SetSubobject(*GetName());
			FunctionNames.Add(Info);
		}
	}
	return FunctionNames;
}

TArray<FActorIOEvent> UActorIOHelperComponent::GetEventNames()
{
	return TArray<FActorIOEvent>();
}
