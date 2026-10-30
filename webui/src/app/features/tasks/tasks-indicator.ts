import { Component, ElementRef, effect, inject, viewChild, ChangeDetectionStrategy } from '@angular/core';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';
import { ButtonDirective } from '@openng/optimus-ui/button';
import { Popover } from '@openng/optimus-ui/popover';
import { ProgressBar } from '@openng/optimus-ui/progressbar';
import { MessageService } from '@openng/optimus-ui/api';
import { TasksStore } from '../../state/tasks.store';
import { TasksApi } from '../../core/tasks-api.service';
import { ApiError } from '../../core/api/rest-client';
import { PluginTask } from '../../core/api/models';

/** Plugin tasks in the status bar: a count, and a popover listing them. */
@Component({
  selector: 'app-tasks-indicator',
  imports: [ButtonDirective, Popover, ProgressBar],
  templateUrl: './tasks-indicator.html',
  changeDetection: ChangeDetectionStrategy.Eager,
  styleUrl: './tasks-indicator.scss',
})
export class TasksIndicator {
  protected readonly store = inject(TasksStore);
  private readonly api = inject(TasksApi);
  private readonly messages = inject(MessageService);

  private readonly popover = viewChild.required(Popover);
  private readonly button = viewChild<ElementRef<HTMLElement>>('indicator');

  constructor() {
    // The popover is anchored to the button, which goes away with the last task.
    effect(() => {
      if (this.store.tasks().length === 0) this.popover().hide();
    });

    // A task that just started would otherwise go unnoticed behind a small icon.
    this.store.added$.pipe(takeUntilDestroyed()).subscribe(() =>
      // Let the button render first when this is the first task.
      setTimeout(() => {
        const target = this.button()?.nativeElement;
        if (target) this.popover().show(new Event('click'), target);
      }),
    );

    this.store.failures$
      .pipe(takeUntilDestroyed())
      .subscribe((task) =>
        this.messages.add({
          severity: 'error',
          summary: task.title,
          detail: task.message ?? 'Failed',
        }),
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
      error: (err: ApiError) =>
        this.messages.add({ severity: 'error', summary: task.title, detail: err.message }),
    });
  }
}
