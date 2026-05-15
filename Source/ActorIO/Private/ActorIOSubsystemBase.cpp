// Copyright 2024-2025 Horizon Games and all contributors at https://github.com/HorizonGamesRoland/ActorIO/graphs/contributors

#include "ActorIOSubsystemBase.h"
#include "ActorIOComponent.h"
#include "ActorIOInterface.h"
#include "ActorIOAction.h"
#include "ActorIOSettings.h"
#include "ActorIO/ActorIOHelperComponent.h"
#include "LogicActors/LogicActorBase.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/CameraBlockingVolume.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextRenderActor.h"
#include "Engine/BlockingVolume.h"
#include "Engine/TriggerVolume.h"
#include "Engine/TriggerBase.h"
#include "Engine/Light.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Particles/Emitter.h"
#include "Sound/AmbientSound.h"
#include "Sound/AudioVolume.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/EngineVersionComparison.h"

#define LOCTEXT_NAMESPACE "ActorIO"

UActorIOSubsystemBase::UActorIOSubsystemBase()
{
	ActionExecContext = FActionExecutionContext();
}

UActorIOSubsystemBase* UActorIOSubsystemBase::Get(UObject* WorldContextObject)
{
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		UActorIOSubsystemBase* IOSubsystem = World->GetSubsystem<UActorIOSubsystemBase>();
		if (IOSubsystem)
		{
			return IOSubsystem;
		}
		else
		{
			// In the case of game worlds, do not exit gracefully with nullptr.
			// This should be treated as a critical failure.
			// The game must have a valid I/O subsystem when actions are being used.
			checkf(!World->IsGameWorld(),
			       TEXT(
				       "Could not get I/O subsystem from game world! Ensure that I/O subsystem class is valid in 'Project Settings -> Actor I/O'."
			       ));
		}
	}

	return nullptr;
}

bool UActorIOSubsystemBase::ExecuteCommand(UObject* Target, const TCHAR* Str, FOutputDevice& Ar, UObject* Executor)
{
	/**
	 * THIS IS A MODIFIED VERSION OF UObject::CallFunctionByNameWithString
	 *
	 * Always review the original function when a new engine version is released and update this code if needed.
	 * If everything is working update the UE version comparison below. Current value represents the latest reviewed engine version.
	 *
	 * List of changes:
	 *
	 *   - Skip importing value for 'out' properties that are not passed by 'ref'.
	 *   - Skip CPP param default value initialization because it only works in editor and not packaged games.
	 *   - FindFunction and ProcessEvent are called on Target.
	 *   - Add IsValid check for Target before finding UFunction.
	 *   - Use Ar.Logf instead of UE_LOG(LogScriptCore) because LogScriptCore is static and its verbosity cannot be changed.
	 *   - Return success/failure properly.
	 */

#if UE_VERSION_NEWER_THAN(5, 7, 999) // <- patch version doesn't matter so use 999 to pass the check
#error "Review latest implementation of UObject::CallFunctionByNameWithString then update UE version comparison."
#endif

	// Find an exec function.
	FString MsgStr;
	if (!FParse::Token(Str, MsgStr, true))
	{
		Ar.Logf(TEXT("ExecuteCommand: Not Parsed '%s'"), Str);
		return false;
	}
	const FName Message = FName(*MsgStr, FNAME_Find);
	if (Message == NAME_None)
	{
		Ar.Logf(TEXT("ExecuteCommand: Name not found '%s'"), Str);
		return false;
	}
	if (!IsValid(Target))
	{
		Ar.Logf(TEXT("ExecuteCommand: Target not found"));
		return false;
	}
	UFunction* Function = Target->FindFunction(Message);
	if (nullptr == Function)
	{
		Ar.Logf(TEXT("ExecuteCommand: Function not found '%s'"), Str);
		return false;
	}

	FProperty* LastParameter = nullptr;

	// find the last parameter
	for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & (CPF_Parm | CPF_ReturnParm)) == CPF_Parm; ++
	     It)
	{
		LastParameter = *It;
	}

	// Parse all function parameters.
	uint8* Parms = (uint8*)FMemory_Alloca_Aligned(Function->ParmsSize, Function->GetMinAlignment());
	FMemory::Memzero(Parms, Function->ParmsSize);

	for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		FProperty* LocalProp = *It;
		checkSlow(LocalProp);
		if (!LocalProp->HasAnyPropertyFlags(CPF_ZeroConstructor))
		{
			LocalProp->InitializeValue_InContainer(Parms);
		}
	}

	const uint32 ExportFlags = PPF_None;
	bool bFailed = 0;
	int32 NumParamsEvaluated = 0;
	for (TFieldIterator<FProperty> It(Function); It && (It->PropertyFlags & (CPF_Parm | CPF_ReturnParm)) == CPF_Parm; ++
	     It, NumParamsEvaluated++)
	{
		FProperty* PropertyParam = *It;
		checkSlow(PropertyParam); // Fix static analysis warning
		if (NumParamsEvaluated == 0 && Executor)
		{
			FObjectPropertyBase* Op = CastField<FObjectPropertyBase>(*It);
			if (Op && Executor->IsA(Op->PropertyClass))
			{
				// First parameter is implicit reference to object executing the command.
				Op->SetObjectPropertyValue(Op->ContainerPtrToValuePtr<uint8>(Parms), Executor);
				continue;
			}
		}

		/*
		 * SKIP IMPORTING VALUE FOR 'OUT' PROPERTIES THAT ARE NOT PASSED BY 'REF'
		 * 
		 * In Unreal's reflection system, out properties and reference properties are differentiated.
		 * Out params that are NOT passed by ref only appear on return nodes.
		 * They do not have an input value, instead they are initialized to zero/null and the function itself will give it a value when returning it.
		 * However, if the property is passed by ref then it will also appear as an input, and we will have to initialize it ourselves.
		 * 
		 * In C++ this differentiation translates to:
		 *  - "FString& OutString" <- an out param that is not passed by ref and will only appear on return nodes (even though the value is passed by ref in C++).
		 *  - "UPARAM(Ref) FString& OutString" <- an out param that is passed by ref, and will behave the same as regular C++ code.
		 * 
		 * So to properly support out params, we need to skip importing values for these params if they are NOT passed by ref.
		 * At this point we've already initialized a value for these params above.
		 */
		if (PropertyParam->HasAnyPropertyFlags(CPF_OutParm) && !PropertyParam->HasAnyPropertyFlags(CPF_ReferenceParm))
		{
			continue;
		}

		// Keep old string around in case we need to pass the whole remaining string
		const TCHAR* RemainingStr = Str;

		// Parse a new argument out of Str
		FString ArgStr;
		FParse::Token(Str, ArgStr, true);

		// if ArgStr is empty but we have more params to read parse the function to see if these have defaults, if so set them
		bool bFoundDefault = false;
		bool bFailedImport = true;

		/*
		 * SKIP INITIALIZE CPP FUNCTION PARAM DEFAULT VALUE
		 *
		 * This only works in the editor because values are being read from Function->GetMetaData (which is editor only).
		 * We need to skip this because it would lead to different outcomes in editor vs packaged game.
		 * I'm not sure how blueprint nodes do this.. I suspect they read this value at edit time and store it for execution.
		 * In theory we could cache these values at cook time for use at runtime (?)
		 *
#if WITH_EDITOR
		if (!FCString::Strcmp(*ArgStr, TEXT("")))
		{
		    const FName DefaultPropertyKey(*(FString(TEXT("CPP_Default_")) + PropertyParam->GetName()));
		    const FString& PropertyDefaultValue = Function->GetMetaData(DefaultPropertyKey);
		    if (!PropertyDefaultValue.IsEmpty())
		    {
		        bFoundDefault = true;

		        const TCHAR* Result = It->ImportText_InContainer(*PropertyDefaultValue, Parms, nullptr, ExportFlags);
		        bFailedImport = (Result == nullptr);
		    }
		}
#endif
		*/

		if (!bFoundDefault)
		{
			// if this is the last string property and we have remaining arguments to process, we have to assume that this
			// is a sub-command that will be passed to another exec (like "cheat giveall weapons", for example). Therefore
			// we need to use the whole remaining string as an argument, regardless of quotes, spaces etc.
			if (PropertyParam == LastParameter && PropertyParam->IsA<FStrProperty>() && FCString::Strcmp(Str, TEXT(""))
				!= 0)
			{
				ArgStr = FString(RemainingStr).TrimStart();
			}

			const TCHAR* Result = It->ImportText_InContainer(*ArgStr, Parms, nullptr, ExportFlags);
			bFailedImport = (Result == nullptr);
		}

		if (bFailedImport)
		{
			FFormatNamedArguments Arguments;
			Arguments.Add(TEXT("Message"), FText::FromName(Message));
			Arguments.Add(TEXT("PropertyName"), FText::FromName(It->GetFName()));
			Arguments.Add(TEXT("FunctionName"), FText::FromName(Function->GetFName()));
			Ar.Logf(TEXT("%s"), *FText::Format(NSLOCTEXT("Core", "BadProperty",
			                                             "'{Message}': Bad or missing property '{PropertyName}' when trying to call {FunctionName}"), Arguments)
			        .ToString());
			bFailed = true;

			break;
		}
	}

	if (!bFailed)
	{
		Target->ProcessEvent(Function, Parms);
	}

	//!!destructframe see also UObject::ProcessEvent
	for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		It->DestroyValue_InContainer(Parms);
	}

	return !bFailed;
}

void UActorIOSubsystemBase::RegisterNativeEventsForObject(AActor* InObject, FActorIOEventList& EventRegistry)
{
	for (auto Event : FindEventsForObject(InObject))
	{
		EventRegistry.RegisterEvent(Event);
	}

	for (auto Component : InObject->GetComponents())
	{
		for (auto Event : FindEventsForObject(Component))
		{
			EventRegistry.RegisterEvent(Event);
		}
	}
}

void UActorIOSubsystemBase::RegisterNativeFunctionsForObject(AActor* InObject, FActorIOFunctionList& FunctionRegistry)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UActorIOSubsystemBase::RegisterNativeFunctionsForObject);
	for (auto Function : FindFunctionsForObject(InObject))
	{
		FunctionRegistry.RegisterFunction(Function);
	}
	
	for (auto Component : InObject->GetComponents())
	{
		for (auto ComponentFunction : FindFunctionsForObject(Component))
		{
			FunctionRegistry.RegisterFunction(ComponentFunction);
		}
	}
}

void UActorIOSubsystemBase::GetGlobalNamedArguments(FActionExecutionContext& ExecutionContext)
{
	// Make the local player pawn always accessible as an argument for all functions.
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	ExecutionContext.SetNamedArgument(TEXT("$Player"), IsValid(PlayerPawn) ? PlayerPawn->GetPathName() : FString());

	// Give blueprint layer a chance to add global named arguments.
	K2_GetGlobalNamedArguments();
}

TArray<FActorIOEvent> UActorIOSubsystemBase::FindEventsForObject(UObject* Object)
{
	if (Object->IsEditorOnly() || Object->IsA<UActorIOComponent>())
		return TArray<FActorIOEvent>();
		
	TArray<FActorIOEvent> Events;
	for (TFieldIterator<FProperty> PropIt(Object->GetClass(), EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;

		// Only multicast delegate properties
		if (FMulticastDelegateProperty* DelegateProp = CastField<FMulticastDelegateProperty>(Property))
		{
			// Check if it is BlueprintAssignable
			if (DelegateProp->HasAnyPropertyFlags(CPF_BlueprintAssignable) && !IsBlacklistedName(DelegateProp->GetName()))
			{
				FString Id, DisplayName;
				GetIdAndDisplayName(Id, DisplayName, Object);
				Id = FString::Printf(TEXT("%s::%s"), *Id, *DelegateProp->GetName());
				DisplayName = FString::Printf(TEXT("%s::%s"), *DisplayName, *DelegateProp->GetName());
				FActorIOEvent Event = FActorIOEvent()
									  .SetId(FName(Id))
									  .SetDisplayName(FText::FromString(DisplayName))
									  .SetTooltipText(FText::FromString(DelegateProp->GetToolTipText().ToString()))
									  .SetBlueprintDelegate(Object, FName(DelegateProp->GetName()));
				Events.Add(Event);
			}
		}
	}
	return Events;
}

TArray<FActorIOFunction> UActorIOSubsystemBase::FindFunctionsForObject(UObject* Object)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UActorIOSubsystemBase::FindFunctionsForObject);

	if (Object->IsEditorOnly() || Object->IsA<UActorIOComponent>())
		return TArray<FActorIOFunction>();
	
	TArray<FActorIOFunction> FunctionNames;
	for (TFieldIterator<UFunction> FuncIt(Object->GetClass(), EFieldIteratorFlags::IncludeSuper); FuncIt; ++FuncIt)
	{
		const UFunction* Function = *FuncIt;
		FString FunctionName = Function->GetName();
		if (Function->HasAnyFunctionFlags(FUNC_BlueprintCallable) && !Function->HasAnyFunctionFlags(FUNC_Const | FUNC_BlueprintPure) && !IsBlacklistedName(FunctionName))
		{
			FString Id, DisplayName;
			GetIdAndDisplayName(Id, DisplayName, Object);
			Id = FString::Printf(TEXT("%s::%s"), *Id, *FunctionName);
			DisplayName = FString::Printf(TEXT("%s::%s"), *DisplayName, *FunctionName);
			
			FActorIOFunction Info = FActorIOFunction()
									.SetId(FName(Id))
									.SetDisplayName(FText::FromString(DisplayName))
									.SetTooltipText(FText::FromString(Function->GetToolTipText().ToString()))
									.SetFunction(*FunctionName)
									.SetSubobject(*Object->GetName())
									.SetFunctionPtr(Function)
									.SetOwnerClassPtr(Object->GetClass());
			
			FunctionNames.Add(Info);
		}
	}
	return FunctionNames;
}

void UActorIOSubsystemBase::GetIdAndDisplayName(FString& OutId, FString& OutDisplayName, UObject* Object)
{
	if (AActor* ActorObject = Cast<AActor>(Object))
	{
		OutId = FString::Printf(TEXT("%s"), *ActorObject->GetActorNameOrLabel());
		OutDisplayName = FString::Printf(TEXT("%s"), *ActorObject->GetActorNameOrLabel());
	}
	else if (UActorComponent* ComponentObject = Cast<UActorComponent>(Object))
	{
		OutId = FString::Printf(TEXT("%s::%s"), *ComponentObject->GetOwner()->GetActorNameOrLabel(), *Object->GetName());
		OutDisplayName = FString::Printf(TEXT("%s"), *Object->GetName());
	}
}

bool UActorIOSubsystemBase::IsBlacklistedName(FString Name)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UActorIOSubsystemBase::IsBlacklistedName);

	// Ignore Getters
	bool bGetterFunction = Name.StartsWith(TEXT("Get"), ESearchCase::IgnoreCase);
	// Ignore IsSomething 
	bool bIsFunction = Name.StartsWith(TEXT("Is"), ESearchCase::IgnoreCase);
	// Ignore K2_ functions
	bool bK2 = Name.StartsWith(TEXT("K2"), ESearchCase::CaseSensitive);
	//Except add offset
	bool Exception = false;
	if (bK2)
	{
		bool Offset = Name.Contains(TEXT("Offset"), ESearchCase::IgnoreCase);
		bool Destroy = Name.Contains(TEXT("Destroy"), ESearchCase::IgnoreCase);
		Exception = Offset || Destroy;
	}
	// Ignore Finders
	bool bFinder = Name.StartsWith(TEXT("Find"), ESearchCase::IgnoreCase);
	if (bFinder || (bK2 && !Exception) || bGetterFunction || bIsFunction)
	{
		return true;
	}
	const UActorIOSettings* IOSettings = UActorIOSettings::Get();
	return IOSettings->BlackListedNames.Contains(FName(Name));
}

void UActorIOSubsystemBase::ProcessEvent_OnActorOverlap(AActor* OverlappedActor, AActor* OtherActor)
{
	ActionExecContext.SetNamedArgument(TEXT("$Actor"), IsValid(OtherActor) ? OtherActor->GetPathName() : FString());
}

void UActorIOSubsystemBase::ProcessEvent_OnActorDestroyed(AActor* Actor, EEndPlayReason::Type EndPlayReason)
{
	ActionExecContext.SetNamedArgument(TEXT("$Actor"), IsValid(Actor) ? Actor->GetPathName() : FString());
	if (EndPlayReason != EEndPlayReason::Destroyed)
	{
		// Abort the action if end play was not caused by destroying the actor.
		ActionExecContext.AbortAction();
	}
}

bool UActorIOSubsystemBase::ShouldCreateSubsystem(UObject* Outer) const
{
	// Determine whether this specific subsystem should be created or not.
	// Subsystems are registered with the engine automatically, but we only want one specific subsystem.

	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	UClass* ThisClass = GetClass();

	const UActorIOSettings* IOSettings = UActorIOSettings::Get();
	if (IOSettings->ActorIOSubsystemClass != nullptr)
	{
		// A subsystem class is provided so only create if we are that class.
		return ThisClass == IOSettings->ActorIOSubsystemClass;
	}
	else
	{
		// No subsystem class was provided so use the base implementation.
		UE_LOG(LogActorIO, Error,
		       TEXT("No Actor I/O Subsystem is specified in Actor I/O settings! Reverting to default implementation."));
		return ThisClass == UActorIOSubsystemBase::StaticClass();
	}
}

void UActorIOSubsystemBase::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

#undef LOCTEXT_NAMESPACE
