import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import { getWidgetActionRequiredFields } from './widget-authoring-action-requirements.js';
import {
  applyWidgetParamAliases,
  createWidgetAuthoringContext,
  sendWidgetAuthoringRequest,
  validateWidgetRequiredFields
} from './widget-authoring-context.js';

export async function handleWidgetAuthoringTools(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown>> {
  const requiredFields = getWidgetActionRequiredFields(action);
  if (!requiredFields) {
    return {
      success: false,
      error: 'UNKNOWN_ACTION',
      message: `Unknown widget authoring action: ${action}`
    };
  }

  // Resolve aliases on a copy before building the context, so a widgetPath
  // sourced from the blueprintPath alias still flows through path normalization.
  const aliasedArgs = { ...args };
  applyWidgetParamAliases(aliasedArgs, requiredFields);
  const context = createWidgetAuthoringContext(aliasedArgs, tools);
  validateWidgetRequiredFields(context.argsRecord, requiredFields);
  return await sendWidgetAuthoringRequest(context, action);
}
