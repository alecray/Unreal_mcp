#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"

bool UMcpAutomationBridgeSubsystem::HandleBlueprintAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT(">>> HandleBlueprintAction ENTRY: RequestId=%s RawAction='%s'"),
         *RequestId, *Action);

  McpBlueprintHandlers::FBlueprintActionContext Context =
      McpBlueprintHandlers::BuildBlueprintActionContext(
          *this, RequestId, Action, Payload, RequestingSocket);
  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("HandleBlueprintAction sanitized: CleanAction='%s' Lower='%s'"),
         *Context.CleanAction, *Context.Lower);
  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("HandleBlueprintAction invoked: RequestId=%s RawAction=%s CleanAction=%s Lower=%s"),
         *RequestId, *Action, *Context.CleanAction, *Context.Lower);

  if (!Context.bLooksBlueprint) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, VeryVerbose,
           TEXT("HandleBlueprintAction: action does not match prefix check, returning false (CleanAction='%s')"),
           *Context.CleanAction);
    return false;
  }

  // Guard the masked "args didn't arrive" case: when manage_blueprint is invoked
  // but its nested `action` arg is absent/empty, the context falls back to the
  // wrapper name ("manage_blueprint"). Reporting that as an unknown action sends
  // callers hunting for a bad action name that was never the problem — surface
  // the real cause (the action param never arrived) instead of echoing the
  // method name. See session-notes issue #66.
  {
    FString WrapperNormalized = Context.Lower;
    WrapperNormalized.ReplaceInline(TEXT("-"), TEXT("_"));
    WrapperNormalized.ReplaceInline(TEXT(" "), TEXT("_"));
    if (WrapperNormalized == TEXT("manage_blueprint") ||
        WrapperNormalized == TEXT("manageblueprint")) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
             TEXT("HandleBlueprintAction: manage_blueprint invoked without a "
                  "nested 'action' parameter (RequestId=%s). The action arg did "
                  "not arrive in the payload."),
             *RequestId);
      SendAutomationError(
          RequestingSocket, RequestId,
          TEXT("Missing required parameter: action. manage_blueprint requires an "
               "'action' field naming the operation (e.g. set_default, compile, "
               "create_node, add_variable, add_event). The wrapper tool name is "
               "not itself a valid action; if you passed one, the parameters did "
               "not arrive — re-send the call with action + params set explicitly."),
          TEXT("MISSING_PARAMETER"));
      return true;
    }
  }

  McpBlueprintHandlers::DiagnosticPatternChecks(Context);

  using FBlueprintRoute = bool (*)(const McpBlueprintHandlers::FBlueprintActionContext &);
  static const FBlueprintRoute Routes[] = {
      McpBlueprintHandlers::HandleBlueprintModifyScs,
      McpBlueprintHandlers::HandleBlueprintScsWrappers,
      McpBlueprintHandlers::HandleBlueprintSetVariableMetadata,
      McpBlueprintHandlers::HandleBlueprintAddConstructionScript,
      McpBlueprintHandlers::HandleBlueprintAddVariable,
      McpBlueprintHandlers::HandleBlueprintSetDefaultLiteral,
      McpBlueprintHandlers::HandleBlueprintRemoveRenameVariable,
      McpBlueprintHandlers::HandleBlueprintAddEvent,
      McpBlueprintHandlers::HandleBlueprintRemoveEvent,
      McpBlueprintHandlers::HandleBlueprintAddFunction,
      McpBlueprintHandlers::HandleBlueprintRemoveFunction,
      McpBlueprintHandlers::HandleBlueprintSetDefaultObject,
      McpBlueprintHandlers::HandleBlueprintCompile,
      McpBlueprintHandlers::HandleBlueprintProbeCreateExists,
      McpBlueprintHandlers::HandleBlueprintGet,
      McpBlueprintHandlers::HandleBlueprintAddNode,
      McpBlueprintHandlers::HandleBlueprintConnectPins,
      McpBlueprintHandlers::HandleBlueprintEnsureProbe,
      McpBlueprintHandlers::HandleBlueprintSetMetadata,
  };

  for (FBlueprintRoute Route : Routes) {
    if (Route(Context)) {
      return true;
    }
  }

  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("HandleBlueprintAction: checking HandleSCSAction for action='%s' (clean='%s')"),
         *Action, *Context.CleanAction);
  if (HandleSCSAction(RequestId, Context.CleanAction, Payload, RequestingSocket)) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("HandleSCSAction consumed request"));
    return true;
  }

  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("HandleBlueprintAction: Action '%s' not recognized, returning false to continue dispatch."),
         *Action);
  return false;
#else
  UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
         TEXT("HandleBlueprintAction: Editor-only functionality requested in non-editor build (Action=%s)"),
         *Action);
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Blueprint actions require editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}
