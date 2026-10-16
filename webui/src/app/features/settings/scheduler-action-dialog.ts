import { Component, EventEmitter, Input, OnChanges, Output, SimpleChanges } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { Dialog } from 'primeng/dialog';
import { InputText } from 'primeng/inputtext';
import { InputNumber } from 'primeng/inputnumber';
import { Checkbox } from 'primeng/checkbox';
import { RadioButton } from 'primeng/radiobutton';
import { Select } from 'primeng/select';
import { ButtonDirective } from 'primeng/button';
import { Queue, ScheduledAction, ScheduledActionProperties, ScheduledActionType } from '../../core/api/models';
import { WEEKDAYS, nowLocal, toBytes, toKiB } from './scheduler-format';

const ACTIONS: { value: ScheduledActionType; label: string }[] = [
  { value: 'resume', label: 'Resume all' },
  { value: 'stop', label: 'Stop all' },
  { value: 'speedLimit', label: 'Set speed limit' },
];

/**
 * Does not close itself on Save: the parent performs the request and either closes it or sets
 * `error`, so the user keeps what they typed.
 */
@Component({
  selector: 'app-scheduler-action-dialog',
  imports: [FormsModule, Dialog, InputText, InputNumber, Checkbox, RadioButton, Select, ButtonDirective],
  templateUrl: './scheduler-action-dialog.html',
  styleUrl: './scheduler-settings.scss',
})
export class SchedulerActionDialog implements OnChanges {
  /** null: a new action. */
  @Input() action: ScheduledAction | null = null;
  @Input() queues: Queue[] = [];
  @Input() visible = false;
  @Input() error = '';
  @Output() visibleChange = new EventEmitter<boolean>();
  @Output() save = new EventEmitter<ScheduledActionProperties>();

  protected readonly actions = ACTIONS;
  protected readonly weekdays = WEEKDAYS;

  name = '';
  queue = '';
  type: ScheduledActionType = 'resume';
  /** kB/s; 0 is no limit. */
  down: number | null = 0;
  up: number | null = 0;
  repeated = true;
  time = '';
  days: boolean[] = Array(7).fill(true);
  at = '';

  ngOnChanges(changes: SimpleChanges): void {
    if ('action' in changes || changes['visible']?.currentValue === true) this.reset();
  }

  private reset(): void {
    const a = this.action;
    this.name = a?.name ?? '';
    this.queue = a?.queue && this.queues.some((q) => q.uuid === a.queue) ? a.queue : (this.queues[0]?.uuid ?? '');
    this.type = a?.action ?? 'resume';
    this.down = toKiB(a?.speedLimit?.down ?? 0);
    this.up = toKiB(a?.speedLimit?.up ?? 0);
    this.repeated = a?.repeated ?? true;
    const now = nowLocal();
    this.time = a?.time || now.slice(11);
    this.days = a?.days?.length === 7 ? [...a.days] : Array(7).fill(true);
    this.at = a?.at || now;
  }

  protected get header(): string {
    return this.action === null ? 'Add scheduled action' : 'Edit scheduled action';
  }

  protected get valid(): boolean {
    if (this.name.trim() === '' || this.queue === '') return false;
    return this.repeated ? this.time !== '' && this.days.some(Boolean) : this.at !== '';
  }

  protected onSave(): void {
    if (!this.valid) return;

    const value: ScheduledActionProperties = {
      name: this.name.trim(),
      queue: this.queue,
      action: this.type,
      repeated: this.repeated,
    };
    if (this.type === 'speedLimit') value.speedLimit = { down: toBytes(this.down ?? 0), up: toBytes(this.up ?? 0) };
    if (this.repeated) {
      value.time = this.time;
      value.days = [...this.days];
    } else {
      value.at = this.at;
    }
    this.save.emit(value);
  }

  protected onDialogVisibleChange(value: boolean): void {
    this.visible = value;
    this.visibleChange.emit(value);
  }
}
