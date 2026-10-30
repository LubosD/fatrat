import {
  Component,
  EventEmitter,
  Input,
  OnChanges,
  Output,
  SimpleChanges,
  ChangeDetectionStrategy,
} from '@angular/core';
import { FormsModule } from '@angular/forms';
import { Dialog } from '@openng/optimus-ui/dialog';
import { InputText } from '@openng/optimus-ui/inputtext';
import { InputNumber } from '@openng/optimus-ui/inputnumber';
import { Checkbox } from '@openng/optimus-ui/checkbox';
import { ButtonDirective } from '@openng/optimus-ui/button';
import { InputGroup } from '@openng/optimus-ui/inputgroup';
import { DirectoryPicker } from '../../shared/directory-picker/directory-picker';
import { Queue } from '../../core/api/models';
import { PathSuggest } from '../../shared/path-suggest/path-suggest';
import { watchStatusProblem } from './watch-status';

export interface QueueFormValue {
  name: string;
  defaultDirectory: string;
  moveDirectory: string;
  watchDirectory: string;
  speedLimits: { down: number; up: number };
  transferLimits: { down: number; up: number };
  upAsDown: boolean;
}

@Component({
  selector: 'app-queue-properties-dialog',
  imports: [
    FormsModule,
    Dialog,
    InputText,
    InputNumber,
    Checkbox,
    ButtonDirective,
    InputGroup,
    DirectoryPicker,
    PathSuggest,
  ],
  templateUrl: './queue-properties-dialog.html',
  changeDetection: ChangeDetectionStrategy.Eager,
  styles: ['.field > .hint { flex: 1 1 100%; }'],
})
export class QueuePropertiesDialog implements OnChanges {
  @Input() queue: Queue | null = null;
  @Input() visible = false;
  @Output() visibleChange = new EventEmitter<boolean>();
  @Output() save = new EventEmitter<QueueFormValue>();

  name = '';
  defaultDirectory = '';
  moveDirectory = '';
  watchDirectory = '';
  speedDown = 0;
  speedUp = 0;
  transferDown = -1;
  transferUp = -1;
  upAsDown = false;
  protected pickerTarget: 'defaultDirectory' | 'moveDirectory' | 'watchDirectory' | null = null;

  // Also reset when the dialog is (re)opened, not just when the input reference changes —
  // reopening with a reference-equal input (e.g. null again, or the same object) would
  // otherwise leave stale, previously-edited-but-cancelled form values in place.
  ngOnChanges(changes: SimpleChanges): void {
    if ('queue' in changes || changes['visible']?.currentValue === true) {
      this.resetForm();
    }
  }

  private resetForm(): void {
    const q = this.queue;
    this.name = q?.name ?? '';
    this.defaultDirectory = q?.defaultDirectory ?? '';
    this.moveDirectory = q?.moveDirectory ?? '';
    this.watchDirectory = q?.watchDirectory ?? '';
    this.speedDown = Math.round((q?.speedLimits.down ?? 0) / 1024);
    this.speedUp = Math.round((q?.speedLimits.up ?? 0) / 1024);
    this.transferDown = q?.transferLimits.down ?? -1;
    this.transferUp = q?.transferLimits.up ?? -1;
    this.upAsDown = q?.upAsDown ?? false;
    this.pickerTarget = null;
  }

  protected onDirectoryPicked(path: string): void {
    if (this.pickerTarget !== null) this[this.pickerTarget] = path;
  }

  protected get pickerInitialPath(): string {
    return this.pickerTarget === null ? '' : this[this.pickerTarget];
  }

  /** The saved folder's problem; hidden once the path is edited, as the status no longer applies. */
  protected get watchProblem(): string | null {
    const q = this.queue;
    if (q === null || this.watchDirectory !== q.watchDirectory) return null;
    return watchStatusProblem(q.watchStatus);
  }

  protected get dialogHeader(): string {
    return this.queue === null ? 'New Queue' : 'Queue Properties';
  }

  protected onSave(): void {
    this.save.emit({
      name: this.name,
      defaultDirectory: this.defaultDirectory,
      moveDirectory: this.moveDirectory,
      watchDirectory: this.watchDirectory.trim(),
      speedLimits: { down: (this.speedDown ?? 0) * 1024, up: (this.speedUp ?? 0) * 1024 },
      transferLimits: { down: this.transferDown, up: this.transferUp },
      upAsDown: this.upAsDown,
    });
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
