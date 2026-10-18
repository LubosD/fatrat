import { JsonValue } from '../../core/api/json-value';
import { PluginFormField } from '../../core/api/models';

/** A field to render, with the title of the group it sits in ('' at the top level). */
export interface FlatField {
  field: PluginFormField;
  group: string;
}

/**
 * Groups as headings: a keyed group is a checkbox (it is "checkable" on the desktop) followed by
 * its items. The web form does not nest.
 */
export function flattenFields(fields: PluginFormField[], group = ''): FlatField[] {
  const flat: FlatField[] = [];
  for (const field of fields) {
    if (field.type === 'group') {
      const title = field.title ?? '';
      if (field.key) flat.push({ field: { type: 'checkbox', key: field.key, title, default: field.default }, group: title });
      flat.push(...flattenFields(field.items ?? [], title));
    } else {
      flat.push({ field, group });
    }
  }
  return flat;
}

/** What each keyed field starts with — the same as FormWidgets on the desktop. */
export function initialValues(fields: PluginFormField[]): Record<string, JsonValue> {
  const values: Record<string, JsonValue> = {};
  for (const { field } of flattenFields(fields)) {
    if (!field.key) continue;
    values[field.key] = initialValue(field);
  }
  return values;
}

function initialValue(field: PluginFormField): JsonValue {
  switch (field.type) {
    case 'checklist':
      return (field.rows ?? []).filter((r) => r.checked).map((r) => r.value);
    case 'checkbox':
      return field.default ?? false;
    case 'list':
      return field.default ?? field.rows?.[0]?.value ?? '';
    case 'combo':
      return field.default ?? field.options?.[0]?.value ?? '';
    case 'number':
      return field.default ?? numberFallback(field);
    default:
      return field.default ?? '';
  }
}

/** What an empty number field becomes: 0, or the nearest value its limits allow. */
export function numberFallback(field: PluginFormField): number {
  let value = 0;
  if (field.min !== undefined) value = Math.max(value, field.min);
  if (field.max !== undefined) value = Math.min(value, field.max);
  return value;
}
