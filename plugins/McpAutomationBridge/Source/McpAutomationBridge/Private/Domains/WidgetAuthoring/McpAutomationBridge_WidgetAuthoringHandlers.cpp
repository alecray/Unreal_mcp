#include "Domains/WidgetAuthoring/McpAutomationBridge_WidgetAuthoringActions.h"

#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

namespace
{
// Widget authoring mutations don't get the engine's CDO-write protection that
// set_default relies on, so during PIE they silently "succeed" and leave the
// widget corrupt (Slate leaks, duplicated/wrong labels). Deny mutations by
// default while PIE is active; only read-only introspection is allowed through.
// See session-notes issue #1.
bool IsReadOnlyWidgetAuthoringAction(const FString& SubAction)
{
    return SubAction.StartsWith(TEXT("get_")) ||
           SubAction.StartsWith(TEXT("list_")) ||
           SubAction.StartsWith(TEXT("query_")) ||
           SubAction.StartsWith(TEXT("find_")) ||
           SubAction.StartsWith(TEXT("preview_")) ||
           SubAction.StartsWith(TEXT("inspect_")) ||
           SubAction.StartsWith(TEXT("describe_"));
}
} // namespace

bool UMcpAutomationBridgeSubsystem::HandleManageWidgetAuthoringAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_widget_authoring"))
    {
        return false;
    }

    FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));
    if (SubAction.IsEmpty())
    {
        SubAction = GetJsonStringField(Payload, TEXT("action"));
    }

#if WITH_EDITOR
    if (GEditor && GEditor->PlayWorld && !IsReadOnlyWidgetAuthoringAction(SubAction))
    {
        SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Refusing widget authoring action '%s' while the editor is in play mode (PIE). "
                "Authoring mutations during PIE silently corrupt the widget (Slate leaks, wrong/duplicated labels). "
                "Stop PIE and retry."), *SubAction),
            TEXT("EDITOR_IN_PLAY_MODE"));
        return true;
    }
#endif

    TSharedPtr<FJsonObject> ResultJson = McpHandlerUtils::CreateResultObject();
    using namespace WidgetAuthoringHandlers;
    static constexpr FWidgetAuthoringActionHandler Handlers[] = {
        HandleWidgetAuthoringCreation,
        HandleWidgetAuthoringPanelBasics,
        HandleWidgetAuthoringBasicVisuals,
        HandleWidgetAuthoringValueWidgets,
        HandleWidgetAuthoringInfo,
        HandleWidgetAuthoringGridPanels,
        HandleWidgetAuthoringScrollScalePanels,
        HandleWidgetAuthoringBorderPanel,
        HandleWidgetAuthoringInputWidgets,
        HandleWidgetAuthoringCollectionWidgets,
        HandleWidgetAuthoringCanvasSlotGeometry,
        HandleWidgetAuthoringSlotAppearance,
        HandleWidgetAuthoringStyleClipping,
        HandleWidgetAuthoringPropertyBindings,
        HandleWidgetAuthoringEventBindings,
        HandleWidgetAuthoringAnimationCore,
        HandleWidgetAuthoringMenuTemplates,
        HandleWidgetAuthoringHudElements,
        HandleWidgetAuthoringPreview,
        HandleWidgetAuthoringGenericComponent,
        HandleWidgetAuthoringUnifiedBinding,
        HandleWidgetAuthoringStyleVariables,
        HandleWidgetAuthoringSettingsTemplate,
        HandleWidgetAuthoringLoadingMinimapTemplates,
        HandleWidgetAuthoringObjectiveDamageTemplates,
        HandleWidgetAuthoringInventoryTemplate,
        HandleWidgetAuthoringDialogRadialTemplates,
        HandleWidgetAuthoringManipulation,
        HandleWidgetAuthoringAdditionalPanels,
        HandleWidgetAuthoringAdvancedStyling,
        HandleWidgetAuthoringAnimationQueries,
        HandleWidgetAuthoringLocalization,
        HandleWidgetAuthoringCreditsTemplate,
        HandleWidgetAuthoringShopTemplate,
        HandleWidgetAuthoringQuestTemplate
    };

    for (FWidgetAuthoringActionHandler Handler : Handlers)
    {
        if (Handler(*this, RequestId, SubAction, Payload, RequestingSocket, ResultJson))
        {
            return true;
        }
    }

    return false;
}
