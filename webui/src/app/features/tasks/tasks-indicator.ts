import { Component, inject } from '@angular/core';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';
import { ButtonDirective } from 'primeng/button';
import { Popover } from 'primeng/popover';
import { ProgressBar } from 'primeng/progressbar';
import { MessageService } from 'primeng/api';
import { TasksStore } from '../../state/tasks.store';
import { TasksApi } from '../../core/tasks-api.service';
import { ApiError } from '../../core/api/rest-client';
import { PluginTask } from '../../core/api/models';

/** Plugin tasks in the status bar: a count, and a popover listing them. */
@Component({
  selector: 'app-tasks-indicator',
  imports: [ButtonDirective, Popover, ProgressBar],
  templateUrl: './tasks-indicator.html',
  styleUrl: './tasks-indicator.scss',
})
export class TasksIndicator {
  protected readonly store = inject(TasksStore);
  private readonly api = inject(TasksApi);
  private readonly messages = inject(MessageService);

  constructor() {
    this.store.failures$.pipe(takeUntilDestroyed()).subscribe((task) =>
      this.messages.add({ severity: 'error', summary: task.title, detail: task.message ?? 'Failed' }),
    );
  }

  protected isFinished(task: PluginTask): boolean {
    return task.state === 'done' || task.state === 'failed' || task.state === 'cancelled';
  }

  protected statusText(task: PluginTask): string {
    switch (task.state) {
      case 'waiting':
        return 'Waiting for an answer';
      case 'done':
        return task.message ?? 'Done';
      case 'failed':
        return task.message ?? 'Failed';
      case 'cancelled':
        return task.message ?? 'Cancelled';
      default:
        return task.status || (task.progress >= 0 ? `${task.progress}%` : 'Working');
    }
  }

  protected answer(task: PluginTask): void {
    this.store.request(task.id);
  }

  /** Cancel a running task, or take a finished one off the list. */
  protected stop(task: PluginTask): void {
    const finished = this.isFinished(task);
    this.api.cancelOrDismiss(task.id).subscribe({
      next: () => {
        if (finished) this.store.remove(task.id);
      },
      error: (err: ApiError) => this.messages.add({ severity: 'error', summary: task.title, detail: err.message }),
    });
  }
}
