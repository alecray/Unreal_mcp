#pragma once

#include "Components/SlateWrapperTypes.h"
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace WidgetAuthoringHelpers
{
FLinearColor GetColorFromJsonWidget(const TSharedPtr<FJsonObject>& ColorObject, const FLinearColor& Default = FLinearColor::White);
TSharedPtr<FJsonObject> GetObjectField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName);
const TArray<TSharedPtr<FJsonValue>>* GetArrayField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName);
FString GetSlotName(const TSharedPtr<FJsonObject>& Payload);
// Resolve the name for a newly-created child widget. Honors `name` first (the
// natural param and what add_* action schemas advertise), then slotName/widgetName,
// then the supplied type default. See session-notes issue #2.
FString GetNewWidgetName(const TSharedPtr<FJsonObject>& Payload, const FString& Default);
ESlateVisibility GetVisibility(const FString& VisibilityString);
}
