// Copyright 2024-2025 Horizon Games and all contributors at https://github.com/HorizonGamesRoland/ActorIO/graphs/contributors

#pragma once

#include "ActorIO.h"
#include "Engine/DeveloperSettings.h"
#include "Templates/SubclassOf.h"
#include "ActorIOSettings.generated.h"

/**
 * Settings for the Actor I/O level scripting system.
 */
UCLASS(Config = Game, DefaultConfig)
class ACTORIO_API UActorIOSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:

	/** Default constructor. */
	UActorIOSettings();

	/**
	 * Get the global actor IO plugin settings object.
	 * This returns the class's CDO so every property is read only!
	 */
	static const UActorIOSettings* Get();

public:

	/**
	 * Class to use as the Actor I/O Subsystem.
	 * If none, then the base implementation of the class is used.
	 * Requires a level reload to fully take effect.
	 */
	UPROPERTY(Config, NoClear, EditAnywhere, Category = "Settings", DisplayName = "Actor I/O Subsystem Class")
	TSubclassOf<class UActorIOSubsystemBase> ActorIOSubsystemClass;
	
	UPROPERTY(Config, EditAnywhere, Category = "Settings", DisplayName = "Actor I/O Blacklist")
	TSet<FName> BlackListedNames = {
		"AddComponent",
		"AddComponentByClass",
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
		"ActorHasTag",
		"CanTriggerResimulation",
		"CreateInputComponent",
		"DetachRootComponentFromParent",
		"FinishAddComponent",
		"FlushNetDormancy",
		"ForceNetUpdate",
		"HasAuthority",
		"PrestreamTextures",
		"RegisterAsFocalPointInPhysicsReplicationLOD",
		"SetAutoDestroyWhenFinished",
		"SetFolderPath",
		"SetIsTemporarilyHiddenInEditor",
		"SetMinNetUpdateFrequency",
		"SetNetCullDistanceSquared",
		"SetNetDormancy",
		"SetNetUpdateFrequency",
		"OnInputTouchBegin",
		"OnInputTouchEnd",
		"OnInputTouchEnter",
		"OnInputTouchLeave",
	};

public:

#if WITH_EDITOR
	//~ Begin UDeveloperSettings Interface
	virtual FText GetSectionText() const override { return INVTEXT("Actor I/O"); }
	//~ End UDeveloperSettings Interface
#endif
};
