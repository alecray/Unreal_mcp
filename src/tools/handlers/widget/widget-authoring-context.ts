import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import { cleanObject } from '../../../utils/serialization/safe-json.js';
import { executeAutomationRequest, getTimeoutMs, normalizePathFields, requireNonEmptyString } from '../foundation/dispatch/common-handlers.js';

export type WidgetAuthoringContext = {
  readonly argsRecord: Record<string, unknown>;
  readonly timeoutMs: number;
  readonly tools: ITools;
};

export function createWidgetAuthoringContext(args: HandlerArgs, tools: ITools): WidgetAuthoringContext {
  const rawArgs: Record<string, unknown> = args;
  return {
    argsRecord: normalizePathFields(rawArgs, ['widgetPath', 'folder', 'path']),
    timeoutMs: getTimeoutMs(),
    tools
  };
}

// Common parameter aliases callers reach for from neighboring tools. A widget
// action's canonical asset-path field is `widgetPath` (but set_default/compile/
// get_graph_details use `blueprintPath`), and its slot target is `slotName` (but
// child-creating actions use `name`). Resolving these avoids a spurious
// "Missing required parameter" on the first call. Keyed by canonical field name.
const WIDGET_PARAM_ALIASES: Readonly<Record<string, readonly string[]>> = {
  widgetPath: ['blueprintPath'],
  slotName: ['name', 'widgetName', 'childName']
};

function isNonEmptyString(value: unknown): boolean {
  return typeof value === 'string' && value.trim().length > 0;
}

// Fill any required canonical field that is still empty from a known alias.
// Only touches required fields, so non-required uses of an alias (e.g.
// add_text_block's child `name`) are left untouched.
export function applyWidgetParamAliases(
  argsRecord: Record<string, unknown>,
  requiredFields: readonly string[]
): void {
  for (const canonical of requiredFields) {
    if (isNonEmptyString(argsRecord[canonical])) {
      continue;
    }
    const aliases = WIDGET_PARAM_ALIASES[canonical];
    if (!aliases) {
      continue;
    }
    for (const alias of aliases) {
      if (isNonEmptyString(argsRecord[alias])) {
        argsRecord[canonical] = argsRecord[alias];
        break;
      }
    }
  }
}

export function validateWidgetRequiredFields(
  argsRecord: Record<string, unknown>,
  fieldNames: readonly string[]
): void {
  for (const fieldName of fieldNames) {
    requireNonEmptyString(argsRecord[fieldName], fieldName, `Missing required parameter: ${fieldName}`);
  }
}

export async function sendWidgetAuthoringRequest(
  context: WidgetAuthoringContext,
  subAction: string
): Promise<Record<string, unknown>> {
  const payload: HandlerArgs = { ...context.argsRecord, subAction };
  if (subAction === 'set_position') {
    normalizeVector2Alias(payload, 'position', 'positionX', 'positionY');
  }
  if (subAction === 'set_size') {
    normalizeVector2Alias(payload, 'size', 'sizeX', 'sizeY');
  }

  const result = await executeAutomationRequest(
    context.tools,
    'manage_widget_authoring',
    payload,
    `Automation bridge not available for widget authoring action: ${subAction}`,
    { timeoutMs: context.timeoutMs }
  );
  return cleanRecord(result);
}

function normalizeVector2Alias(payload: Record<string, unknown>, objectKey: string, xKey: string, yKey: string): void {
  if (payload[objectKey] !== undefined) {
    return;
  }

  const x = finiteNumber(payload[xKey]);
  const y = finiteNumber(payload[yKey]);
  if (x === undefined && y === undefined) {
    return;
  }

  const value: Record<string, number> = {};
  if (x !== undefined) {
    value.x = x;
  }
  if (y !== undefined) {
    value.y = y;
  }
  payload[objectKey] = value;
  delete payload[xKey];
  delete payload[yKey];
}

function finiteNumber(value: unknown): number | undefined {
  return typeof value === 'number' && Number.isFinite(value) ? value : undefined;
}

function cleanRecord(value: unknown): Record<string, unknown> {
  const cleaned = cleanObject(value);
  if (cleaned !== null && typeof cleaned === 'object' && !Array.isArray(cleaned)) {
    const record: Record<string, unknown> = {};
    for (const [key, entry] of Object.entries(cleaned)) {
      record[key] = entry;
    }
    return record;
  }
  return {};
}
