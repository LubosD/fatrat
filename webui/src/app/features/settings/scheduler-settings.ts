import { Component, computed, inject, signal, ChangeDetectionStrategy } from '@angular/core';
import { ConfirmationService } from '@openng/optimus-ui/api';
import { TableModule } from '@openng/optimus-ui/table';
import { ButtonDirective } from '@openng/optimus-ui/button';
import { SchedulerApi } from '../../core/scheduler-api.service';
import { ApiError } from '../../core/api/rest-client';
import { QueuesStore } from '../../state/queues.store';
import { ViewportService } from '../../core/viewport.service';
import { ScheduledAction, ScheduledActionProperties } from '../../core/api/models';
import { SchedulerActionDialog } from './scheduler-action-dialog';
import { actionLabel, whenLabel } from './scheduler-format';

/**
 * Unlike the other pages nothing here is staged for the OK button: like the RSS feeds, every change
 * is saved as soon as it is made (the server applies it to the running scheduler right away).
 */
@Component({
  selector: 'app-scheduler-settings',
  imports: [TableModule, ButtonDirective, SchedulerActionDialog],
  templateUrl: './scheduler-settings.html',
  changeDetection: ChangeDetectionStrategy.Eager,
  styleUrl: './scheduler-settings.scss',
})
export class SchedulerSettings {
  private readonly api = inject(SchedulerApi);
  private readonly confirmation = inject(ConfirmationService);
  private readonly queuesStore = inject(QueuesStore);
  protected readonly viewport = inject(ViewportService);

  protected readonly actions = signal<ScheduledAction[]>([]);
  protected readonly queues = this.queuesStore.queues;
  protected readonly message = signal('');

  protected readonly dialogOpen = signal(false);
  protected readonly edited = signal<ScheduledAction | null>(null);
  protected readonly dialogError = signal('');

  protected readonly canAdd = computed(() => this.queues().length > 0);

  protected readonly actionLabel = actionLabel;
  protected readonly whenLabel = whenLabel;

  constructor() {
    this.reload();
  }

  protected queueName(uuid: string): string {
    return this.queues().find((q) => q.uuid === uuid)?.name ?? '(deleted queue)';
  }

  protected reload(): void {
    this.api.list().subscribe({
      next: (actions) => this.actions.set(actions),
      error: (err: ApiError) => this.message.set(err.message),
    });
  }

  protected add(): void {
    if (!this.canAdd()) return;
    this.open(null);
  }

  protected edit(action: ScheduledAction): void {
    this.open(action);
  }

  private open(action: ScheduledAction | null): void {
    this.edited.set(action);
    this.dialogError.set('');
    this.dialogOpen.set(true);
  }

  protected saveAction(value: ScheduledActionProperties): void {
    const edited = this.edited();
    const request = edited === null ? this.api.create(value) : this.api.update(edited.id, value);
    request.subscribe({
      next: () => {
        this.dialogOpen.set(false);
        this.reload();
      },
      error: (err: ApiError) => this.dialogError.set(err.message),
    });
  }

  protected remove(action: ScheduledAction): void {
    this.confirmation.confirm({
      header: 'Confirm Delete',
      message: `Do you really want to remove the scheduled action "${action.name}"?`,
      accept: () =>
        this.api.delete(action.id).subscribe({
          next: () => this.reload(),
          error: (err: ApiError) => this.message.set(err.message),
        }),
    });
  }
}
