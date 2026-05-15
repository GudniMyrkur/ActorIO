// // Property of Myrkur Games

#pragma once

#include "CoreMinimal.h"
#include "ActorIO.h"
#include "Components/ActorComponent.h"
#include "ActorIOHelperComponent.generated.h"

UCLASS(Blueprintable)
class ACTORIO_API UActorIOHelperComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UActorIOHelperComponent();

	TArray<FActorIOFunction> GetFunctionNames();
	TArray<FActorIOEvent> GetEventNames();

	TArray<FName> BlacklistedFunctions =
	{
		"ConditionalAbortIOAction",
		"K2_GetLocalNamedArguments",
		"K2_RegisterIOEvents",
		"K2_RegisterIOFunctions",
		"ToggleActive",
		"SetTickGroup",
		"SetTickableWhenPaused",
		"SetIsReplicated",
		"SetComponentTickIntervalAndCooldown",
		"SetComponentTickInterval",
		"SetAutoActivate",
		"SetActive",
		"RemoveTickPrerequisiteComponent",
		"RemoveTickPrerequisiteActor",
		"IsComponentTickEnabled",
		"IsBeingDestroyed",
		"IsActive",
		"GetOwner",
		"GetComponentTickInterval",
		"Deactivate",
		"ComponentHasTag",
		"AddTickPrerequisiteComponent",
		"AddTickPrerequisiteActor",
		"Activate",
	};
};
