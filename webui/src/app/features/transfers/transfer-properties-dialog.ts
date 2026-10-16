import { Component, EventEmitter, Input, OnChanges, Output, SimpleChanges } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { Dialog } from 'primeng/dialog';
import { Textarea } from 'primeng/textarea';
import { InputNumber } from 'primeng/inputnumber';
import { ButtonDirective } from 'primeng/button';
import { InputText } from 'primeng/inputtext';
import { InputGroup } from 'primeng/inputgroup';
import { DirectoryPicker } from '../../shared/directory-picker/directory-picker';
import { Transfer } from '../../core/api/models';

export interface TransferPropertiesFormValue {
  comment: string;
  speedLimits: { down: number; up: number };
  /** New data directory; only set when the user changed it. */
  object?: string;
}

@Component({
  selector: 'app-transfer-properties-dialog',
  imports: [FormsModule, Dialog, Textarea, InputNumber, ButtonDirective, InputText, InputGroup, DirectoryPicker],
  templateUrl: './transfer-properties-dialog.html',
})
export class TransferPropertiesDialog implements OnChanges {
  @Input() transfer: Transfer | null = null;
  @Input() visible = false;
  @Output() visibleChange = new EventEmitter<boolean>();
  @Output() save = new EventEmitter<TransferPropertiesFormValue>();

  comment = '';
  speedDown = 0;
  speedUp = 0;
  object = '';
  protected pickerOpen = false;

  /** Only downloads keep their data in a directory that can be relocated. */
  protected get canRelocate(): boolean {
    return this.transfer?.primaryMode === 'Download';
  }

  // Also reset when the dialog is (re)opened, not just when the input reference changes —
  // reopening with a reference-equal input (e.g. null again, or the same object) would
  // otherwise leave stale, previously-edited-but-cancelled form values in place.
  ngOnChanges(changes: SimpleChanges): void {
    if ('transfer' in changes || changes['visible']?.currentValue === true) {
      this.resetForm();
    }
  }

  private resetForm(): void {
    const t = this.transfer;
    this.comment = t?.comment ?? '';
    this.speedDown = Math.round((t?.userSpeedLimits.down ?? 0) / 1024);
    this.speedUp = Math.round((t?.userSpeedLimits.up ?? 0) / 1024);
    this.object = t?.object ?? '';
    this.pickerOpen = false;
  }

  protected onSave(): void {
    const value: TransferPropertiesFormValue = {
      comment: this.comment,
      speedLimits: { down: (this.speedDown ?? 0) * 1024, up: (this.speedUp ?? 0) * 1024 },
    };
    const object = this.object.trim();
    if (this.canRelocate && object !== '' && object !== (this.transfer?.object ?? '')) {
      value.object = object;
    }
    this.save.emit(value);
    this.closeDialog();
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
