#include "ActorIOEditorTools.h"

bool UActorIOEditorTools::IsEngineClass(const UClass* Class)
{
	if (!Class)
	{
		return false;
	}

	const FString PackageName = Class->GetPackage()->GetName();

	// Engine and CoreUObject packages
	static const FString EnginePrefix = TEXT("/Script/Engine");
	static const FString CorePrefix   = TEXT("/Script/CoreUObject");

	// If the class is in Engine or CoreUObject, it is an engine class.
	return PackageName.StartsWith(EnginePrefix) || PackageName.StartsWith(CorePrefix);
}