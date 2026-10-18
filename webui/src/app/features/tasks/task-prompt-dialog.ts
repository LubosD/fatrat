import { Component, DestroyRef, computed, inject, linkedSignal, signal, untracked } from '@angular/core';
import { Dialog } from 'primeng/dialog';
import { ButtonDirective } from 'primeng/button';
import { MessageService } from 'primeng/api';
import { TasksStore } from '../../state/tasks.store';
import { TasksApi } from '../../core/tasks-api.service';
import { ApiError } from '../../core/api/rest-client';
import { JsonValue } from '../../core/api/json-value';
import { FormField } from '../../shared/form-field/form-field';
import { flattenFields, initialValues } from '../../shared/form-field/form-values';

/**
 * Plugin questions, one at a time. Mounted once in Shell, like the captcha dialog. The desktop may
 * answer first; the question then disappears from TasksStore and the next one (if any) shows.
 */
@Component({
  selector: 'app-task-prompt-dialog',
  imports: [Dialog, ButtonDirective, FormField],
  templateUrl: './task-prompt-dialog.html',
})
export class TaskPromptDialog {
  private readonly store = inject(TasksStore);
  private readonly api = inject(TasksApi);
  private readonly messages = inject(MessageService);

  protected readonly current = computed(() => {
    const open = this.store.openPrompts();
    return open.find((t) => t.id === this.store.requested()) ?? open[0] ?? null;
  });
  /** Changes only when another question comes up, not when its task reports progress. */
  private readonly promptId = computed(() => this.current()?.prompt?.id ?? null);

  protected readonly header = computed(() => {
    const task = this.current();
    return task?.prompt?.form.title || task?.title || '';
  });
  protected readonly fields = computed(() => flattenFields(this.current()?.prompt?.form.fields ?? []));
  protected readonly values = linkedSignal<string | null, Record<string, JsonValue>>({
    source: this.promptId,
    computation: () => initialValues(untracked(this.current)?.prompt?.form.fields ?? []),
  });
  protected readonly error = linkedSignal<string | null, string>({ source: this.promptId, computation: () => '' });
  protected readonly busy = signal(false);

  private readonly now = signal(Date.now());
  protected readonly secondsLeft = computed(() => {
    const expires = this.current()?.prompt?.expiresAt;
    return expires ? Math.max(0, Math.round((Date.parse(expires) - this.now()) / 1000)) : 0;
  });

  constructor() {
    const timer = setInterval(() => this.now.set(Date.now()), 1000);
    inject(DestroyRef).onDestroy(() => clearInterval(timer));
  }

  setValue(key: string, value: JsonValue): void {
    this.values.update((values) => ({ ...values, [key]: value }));
  }

  submit(): void {
    const task = this.current();
    const prompt = task?.prompt;
    if (!task || !prompt || this.busy()) return;

    this.busy.set(true);
    this.api.answer(task.id, prompt.id, this.values()).subscribe({
      next: () => {
        this.busy.set(false);
        this.store.markHandled(prompt.id);
      },
      error: (err: ApiError) => {
        this.busy.set(false);
        // Only these mean the question is gone (answered elsewhere, expired); anything else,
        // such as a dropped connection, leaves it open to try again.
        if (![404, 409, 410].includes(err.status)) {
          this.error.set(err.message);
          return;
        }
        this.store.markHandled(prompt.id);
        this.messages.add({ severity: 'warn', summary: task.title, detail: err.message });
      },
    });
  }

  cancel(): void {
    const task = this.current();
    const prompt = task?.prompt;
    if (!task || !prompt) return;

    this.store.markHandled(prompt.id);
    this.api.dismissPrompt(task.id, prompt.id).subscribe({ error: () => {} }); // already gone is fine
  }

  protected onVisibleChange(visible: boolean): void {
    if (!visible) this.cancel();
  }
}
