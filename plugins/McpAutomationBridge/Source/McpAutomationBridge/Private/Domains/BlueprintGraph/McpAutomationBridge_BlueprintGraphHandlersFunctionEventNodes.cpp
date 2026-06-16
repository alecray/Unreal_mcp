#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

#if WITH_EDITOR
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

namespace McpBlueprintGraphHandlers
{
static bool TryCreateFunctionNode(
    FActionContext& Context,
    const FString& NodeType,
    float X,
    float Y)
{
    if (NodeType != TEXT("CallFunction") &&
        NodeType != TEXT("K2Node_CallFunction") &&
        NodeType != TEXT("FunctionCall"))
    {
        return false;
    }

    // Accept memberName/memberClass, falling back to the functionName/targetClass
    // aliases (and className) callers reach for. Resolving these avoids the
    // useless "Function '' not found" when only the alias was supplied.
    // See session-notes issue #4.
    FString MemberName;
    FString MemberClass;
    if (!Context.Payload->TryGetStringField(TEXT("memberName"), MemberName) || MemberName.IsEmpty())
    {
        Context.Payload->TryGetStringField(TEXT("functionName"), MemberName);
    }
    if (!Context.Payload->TryGetStringField(TEXT("memberClass"), MemberClass) || MemberClass.IsEmpty())
    {
        if (!Context.Payload->TryGetStringField(TEXT("targetClass"), MemberClass) || MemberClass.IsEmpty())
        {
            Context.Payload->TryGetStringField(TEXT("className"), MemberClass);
        }
    }

    if (MemberName.IsEmpty())
    {
        Context.SendError(
            TEXT("CallFunction node requires a function name. Provide 'memberName' "
                 "(alias 'functionName'); optionally 'memberClass' (alias "
                 "'targetClass') to resolve a function on another class."),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UFunction* Function = nullptr;
    if (!MemberClass.IsEmpty())
    {
        UClass* Class = ResolveUClass(MemberClass);
        if (!Class)
        {
            Context.SendError(
                FString::Printf(
                    TEXT("Class '%s' not found (memberClass/targetClass). Use a class "
                         "name like 'PlayerController' or a full object path."),
                    *MemberClass),
                TEXT("CLASS_NOT_FOUND"));
            return true;
        }
        Function = Class->FindFunctionByName(*MemberName);
    }
    else
    {
        Function =
            Context.Blueprint->GeneratedClass->FindFunctionByName(*MemberName);
        if (!Function)
        {
            Function = UKismetSystemLibrary::StaticClass()
                ->FindFunctionByName(*MemberName);
        }
        if (!Function)
        {
            Function = UGameplayStatics::StaticClass()
                ->FindFunctionByName(*MemberName);
        }
        if (!Function)
        {
            Function = UKismetMathLibrary::StaticClass()
                ->FindFunctionByName(*MemberName);
        }
    }

    if (!Function)
    {
        const FString ClassContext = MemberClass.IsEmpty()
            ? FString(TEXT(" in the Blueprint or the standard Kismet libraries "
                           "(KismetSystemLibrary, GameplayStatics, KismetMathLibrary). "
                           "Pass memberClass/targetClass to target another class."))
            : FString::Printf(TEXT(" on class '%s'"), *MemberClass);
        Context.SendError(
            FString::Printf(
                TEXT("Function '%s' not found%s"),
                *MemberName, *ClassContext),
            TEXT("FUNCTION_NOT_FOUND"));
        return true;
    }

    FGraphNodeCreator<UK2Node_CallFunction> NodeCreator(
        *Context.TargetGraph);
    UK2Node_CallFunction* Node = NodeCreator.CreateNode(false);
    Node->SetFromFunction(Function);
    Context.FinalizeNode(NodeCreator, Node, X, Y);
    return true;
}

static bool TryCreateEventNode(
    FActionContext& Context,
    const FString& NodeType,
    float X,
    float Y)
{
    if (NodeType != TEXT("Event") &&
        NodeType != TEXT("K2Node_Event"))
    {
        return false;
    }

    FString EventName;
    FString MemberClass;
    Context.Payload->TryGetStringField(TEXT("eventName"), EventName);
    Context.Payload->TryGetStringField(TEXT("memberClass"), MemberClass);
    if (EventName.IsEmpty())
    {
        Context.SendError(
            TEXT("eventName required"),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    static const TMap<FString, FString> Aliases = {
        {TEXT("BeginPlay"), TEXT("ReceiveBeginPlay")},
        {TEXT("Tick"), TEXT("ReceiveTick")},
        {TEXT("EndPlay"), TEXT("ReceiveEndPlay")}};
    if (const FString* Alias = Aliases.Find(EventName))
    {
        EventName = *Alias;
    }

    UClass* TargetClass = nullptr;
    UFunction* EventFunction = nullptr;
    if (!MemberClass.IsEmpty())
    {
        TargetClass = ResolveUClass(MemberClass);
        if (TargetClass)
        {
            EventFunction =
                TargetClass->FindFunctionByName(*EventName);
        }
    }
    else
    {
        for (UClass* Class = Context.Blueprint->ParentClass;
             Class && !EventFunction;
             Class = Class->GetSuperClass())
        {
            EventFunction = Class->FindFunctionByName(
                *EventName,
                EIncludeSuperFlag::ExcludeSuper);
            if (EventFunction)
            {
                TargetClass = Class;
            }
        }
    }

    if (!EventFunction || !TargetClass)
    {
        Context.SendError(
            FString::Printf(TEXT("Event '%s' not found"), *EventName),
            TEXT("EVENT_NOT_FOUND"));
        return true;
    }

    FGraphNodeCreator<UK2Node_Event> NodeCreator(*Context.TargetGraph);
    UK2Node_Event* Node = NodeCreator.CreateNode(false);
    Node->EventReference.SetFromField<UFunction>(EventFunction, false);
    Node->bOverrideFunction = true;
    Context.FinalizeNode(NodeCreator, Node, X, Y);
    return true;
}

bool TryCreateFunctionOrEventNode(
    FActionContext& Context,
    const FString& NodeType,
    float X,
    float Y)
{
    return TryCreateFunctionNode(Context, NodeType, X, Y) ||
           TryCreateEventNode(Context, NodeType, X, Y);
}
}
#endif
