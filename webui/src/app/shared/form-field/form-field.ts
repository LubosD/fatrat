import { Component, computed, input, model } from '@angular/core';
import { NgTemplateOutlet } from '@angular/common';
import { FormsModule } from '@angular/forms';
import { ButtonDirective } from 'primeng/button';
import { Checkbox } from 'primeng/checkbox';
import { InputNumber } from 'primeng/inputnumber';
import { InputText } from 'primeng/inputtext';
import { Select } from 'primeng/select';
import { TableModule } from 'primeng/table';
import { JsonValue } from '../../core/api/json-value';
import { PluginFormField, PluginFormRow } from '../../core/api/models';
import { numberFallback } from './form-values';
import { DirectoryPicker } from '../directory-picker/directory-picker';
import { PathSuggest } from '../path-suggest/path-suggest';

/**
 * One field of a plugin form (a task's question, or a plugin's settings), any type but group —
 * see flattenFields(). A directory is one on the server, so it gets the server-side picker.
 */
@Component({
  selector: 'app-form-field',
  imports: [NgTemplateOutlet, FormsModule, ButtonDirective, Checkbox, InputNumber, InputText, Select, TableModule, DirectoryPicker, PathSuggest],
  templateUrl: './form-field.html',
  styleUrl: './form-field.scss',
})
export class FormField {
  readonly field = input.required<PluginFormField>();
  readonly value = model<JsonValue>(null);
  readonly inputId = input('');
  /** Shown instead of the field's title, e.g. with the plugin's name in front. */
  readonly label = input('');

  protected readonly title = computed(() => this.label() || this.field().title || '');
  protected readonly options = computed(() =>
    (this.field().options ?? []).map((o) => ({ label: o.title ?? o.value, value: o.value })),
  );
  protected readonly rows = computed(() => this.field().rows ?? []);
  protected readonly selection = computed<PluginFormRow | PluginFormRow[] | null>(() => {
    const value = this.value();
    if (this.field().type === 'checklist') {
      const chosen = Array.isArray(value) ? value : [];
      return this.rows().filter((r) => chosen.includes(r.value));
    }
    return this.rows().find((r) => r.value === value) ?? null;
  });

  protected pickerOpen = false;

  protected text(): string {
    const value = this.value();
    return typeof value === 'string' ? value : '';
  }

  protected num(): number | null {
    const value = this.value();
    return typeof value === 'number' ? value : null;
  }

  /**
   * With allowEmpty off, clearing gives 0 and blur clamps it to the limits; null only comes from
   * a lone '-'. A form value is always a number.
   */
  protected onNumber(value: number | null): void {
    this.value.set(value ?? numberFallback(this.field()));
  }

  protected cellsOf(row: PluginFormRow): string[] {
    return row.cells?.length ? row.cells : [row.value];
  }

  protected onSelection(selection: PluginFormRow | PluginFormRow[] | null): void {
    if (Array.isArray(selection)) {
      this.value.set(selection.map((r) => r.value));
    } else {
      this.value.set(selection?.value ?? '');
    }
  }
}
