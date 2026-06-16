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

  const context = createWidgetAuthoringContext(args, tools);
  applyWidgetParamAliases(context.argsRecord, requiredFields);
  validateWidgetRequiredFields(context.argsRecord, requiredFields);
  return await sendWidgetAuthoringRequest(context, action);
}
