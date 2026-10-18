import { Component, EventEmitter, Input, OnChanges, Output, SimpleChanges, inject, signal } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { Dialog } from 'primeng/dialog';
import { Textarea } from 'primeng/textarea';
import { InputNumber } from 'primeng/inputnumber';
import { ButtonDirective } from 'primeng/button';
import { InputText } from 'primeng/inputtext';
import { InputGroup } from 'primeng/inputgroup';
import { of } from 'rxjs';
import { map, switchMap } from 'rxjs/operators';
import { DirectoryPicker } from '../../shared/directory-picker/directory-picker';
import { PathSuggest } from '../../shared/path-suggest/path-suggest';
import { FormField } from '../../shared/form-field/form-field';
import { PluginFormField, Transfer } from '../../core/api/models';
import { JsonValue } from '../../core/api/json-value';
import { TransferApi } from '../../core/api/transfer-api';
import { ApiError } from '../../core/api/rest-client';
import { TransferActionsService } from '../../core/transfer-actions.service';

type OptionsState = 'none' | 'loading' | 'ready' | 'error';

function messageOf(err: unknown): string {
  if (err instanceof ApiError || err instanceof Error) return err.message;
  return String(err);
}

/**
 * The desktop's Transfer properties dialog: general properties for every selected transfer, plus
 * the options of the transfer's class (a form from the server) when exactly one is selected.
 * Saves itself, so a refused change keeps the dialog open with the server's message.
 */
@Component({
  selector: 'app-transfer-properties-dialog',
  imports: [FormsModule, Dialog, Textarea, InputNumber, ButtonDirective, InputText, InputGroup, DirectoryPicker, PathSuggest, FormField],
  templateUrl: './transfer-properties-dialog.html',
  styleUrl: './transfer-properties-dialog.scss',
})
export class TransferPropertiesDialog implements OnChanges {
  private readonly transferApi = inject(TransferApi);
  private readonly actions = inject(TransferActionsService);

  /** The first selected transfer; its values prefill the form. */
  @Input() transfer: Transfer | null = null;
  /** How many transfers are being edited. Options are per transfer, so only offered for one. */
  @Input() transferCount = 1;
  @Input() visible = false;
  @Output() visibleChange = new EventEmitter<boolean>();

  comment = '';
  speedDown = 0;
  speedUp = 0;
  object = '';
  completionCommand = '';
  protected pickerOpen = false;

  protected readonly optionsState = signal<OptionsState>('none');
  protected readonly optionFields = signal<PluginFormField[]>([]);
  protected readonly optionsError = signal('');
  // A signal, so a value set outside a template event (or the bound FormField) re-renders.
  protected readonly optionValues = signal<Record<string, JsonValue>>({});
  private loadedOptionValues: Record<string, JsonValue> = {};
  // Bumped on every (re)open, so options for an earlier opening are dropped.
  private session = 0;

  protected readonly saving = signal(false);
  protected readonly saveError = signal('');

  protected get isUpload(): boolean {
    return this.transfer?.primaryMode === 'Upload';
  }

  // Also reset when the dialog is (re)opened, not just when the input reference changes —
  // reopening with a reference-equal input (e.g. null again, or the same object) would
  // otherwise leave stale, previously-edited-but-cancelled form values in place.
  ngOnChanges(changes: SimpleChanges): void {
    if ('transfer' in changes || changes['visible']?.currentValue === true) {
      this.resetForm();
      this.loadOptions();
    }
  }

  private resetForm(): void {
    const t = this.transfer;
    this.comment = t?.comment ?? '';
    this.speedDown = Math.round((t?.userSpeedLimits.down ?? 0) / 1024);
    this.speedUp = Math.round((t?.userSpeedLimits.up ?? 0) / 1024);
    this.object = t?.object ?? '';
    this.completionCommand = t?.completionCommand ?? '';
    this.pickerOpen = false;
    this.saving.set(false);
    this.saveError.set('');
  }

  private loadOptions(): void {
    const session = ++this.session;
    const t = this.transfer;

    this.optionFields.set([]);
    this.optionValues.set({});
    this.loadedOptionValues = {};
    this.optionsError.set('');

    if (!this.visible || !t || this.transferCount !== 1) {
      this.optionsState.set('none');
      return;
    }

    this.optionsState.set('loading');
    this.transferApi.getTransferOptions(t.uuid).subscribe({
      next: (options) => {
        if (session !== this.session) return;
        this.optionFields.set(options.fields);
        this.optionValues.set({ ...options.values });
        this.loadedOptionValues = { ...options.values };
        this.optionsState.set(options.fields.length > 0 ? 'ready' : 'none');
      },
      error: (err) => {
        if (session !== this.session) return;
        this.optionsError.set(messageOf(err));
        this.optionsState.set('error');
      },
    });
  }

  protected setOption(key: string | undefined, value: JsonValue): void {
    if (key) this.optionValues.update((values) => ({ ...values, [key]: value }));
  }

  private changedOptions(): Record<string, JsonValue> {
    const changed: Record<string, JsonValue> = {};
    for (const [key, value] of Object.entries(this.optionValues())) {
      if (JSON.stringify(value) !== JSON.stringify(this.loadedOptionValues[key])) changed[key] = value;
    }
    return changed;
  }

  protected onSave(): void {
    if (this.saving()) return;

    const t = this.transfer;
    const properties: { [key: string]: JsonValue } = {
      comment: this.comment,
      userSpeedLimits: { down: (this.speedDown ?? 0) * 1024, up: (this.speedUp ?? 0) * 1024 },
    };
    const object = this.object.trim();
    if (object !== '' && object !== (t?.object ?? '')) properties['object'] = object;
    // Only when changed: with several transfers selected, this would overwrite each one's own command.
    if (this.completionCommand !== (t?.completionCommand ?? '')) properties['completionCommand'] = this.completionCommand;

    const options = this.optionsState() === 'ready' && t ? this.changedOptions() : {};
    const uuid = t?.uuid;

    this.saving.set(true);
    this.saveError.set('');
    this.actions
      .updateProperties(properties)
      .pipe(
        switchMap(() =>
          uuid && Object.keys(options).length > 0
            ? this.transferApi.setTransferOptions(uuid, options).pipe(map(() => undefined))
            : of(undefined),
        ),
      )
      .subscribe({
        next: () => {
          this.saving.set(false);
          this.closeDialog();
        },
        error: (err) => {
          this.saving.set(false);
          this.saveError.set(messageOf(err));
        },
      });
  }

  protected onCancel(): void {
    this.closeDialog();
  }

  protected onDialogVisibleChange(value: boolean): void {
    this.visible = value;
    this.visibleChange.emit(value);
  }

  private closeDialog(): void {
    this.onDialogVisibleChange(false);
  }
}
