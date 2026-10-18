import { Injectable, computed, inject, signal } from '@angular/core';
import { Observable, Subject } from 'rxjs';
import { TasksApi } from '../core/tasks-api.service';
import { PushSocketService, TaskPushEvent } from '../core/push-socket.service';
import { PluginTask } from '../core/api/models';

/**
 * Plugin tasks, kept current by /websocket pushes. The full list is fetched at start and after
 * every reconnect, since pushes sent while the socket was down are gone.
 */
@Injectable({ providedIn: 'root' })
export class TasksStore {
  private readonly api = inject(TasksApi);
  private readonly socket = inject(PushSocketService);

  readonly tasks = signal<PluginTask[]>([]);

  /** Prompt ids answered or dismissed in this browser, hidden before the server says so. */
  private readonly handled = signal<ReadonlySet<string>>(new Set());
  readonly openPrompts = computed(() =>
    this.tasks().filter((t) => t.prompt !== undefined && !this.handled().has(t.prompt.id)),
  );

  /** A task the user picked "Answer" on; the prompt dialog shows it first. */
  readonly requested = signal<string | null>(null);

  private readonly failures = new Subject<PluginTask>();
  /** Tasks seen to change into "failed" (not ones that had already failed when loaded). */
  readonly failures$: Observable<PluginTask> = this.failures.asObservable();

  constructor() {
    this.socket.taskEvents$.subscribe((event) => this.apply(event));
    this.socket.connected$.subscribe(() => this.reload());
    this.reload();
  }

  markHandled(promptId: string): void {
    this.handled.update((set) => new Set(set).add(promptId));
  }

  request(taskId: string): void {
    this.requested.set(taskId);
  }

  remove(taskId: string): void {
    this.tasks.update((tasks) => tasks.filter((t) => t.id !== taskId));
  }

  private reload(): void {
    this.api.list().subscribe({
      next: (tasks) => this.tasks.set(tasks),
      error: () => {}, // the next push or reconnect brings it back
    });
  }

  private apply(event: TaskPushEvent): void {
    if (event.type === 'task-removed') {
      this.remove(event.id);
      return;
    }

    const task = event.task;
    const previous = this.tasks().find((t) => t.id === task.id);
    if (task.state === 'failed' && previous?.state !== 'failed') {
      this.failures.next(task);
    }
    this.tasks.update((tasks) =>
      previous ? tasks.map((t) => (t.id === task.id ? task : t)) : [...tasks, task],
    );
  }
}
