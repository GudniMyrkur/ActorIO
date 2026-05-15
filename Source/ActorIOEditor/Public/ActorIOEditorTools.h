#pragma once

#include "CoreMinimal.h"
#include "ActorIOEditorTools.generated.h"

UCLASS()
class ACTORIOEDITOR_API UActorIOEditorTools : public UObject
{
	GENERATED_BODY()

public:

	/** Returns true if the class is defined in your project (not Engine/CoreUObject). */
	UFUNCTION(BlueprintPure, Category = "Utilities|Class")
	static bool IsEngineClass(const UClass* Class);
};

