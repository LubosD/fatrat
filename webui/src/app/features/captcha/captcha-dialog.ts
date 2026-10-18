import { Component, computed, inject, signal } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { Dialog } from 'primeng/dialog';
import { InputText } from 'primeng/inputtext';
import { ButtonDirective } from 'primeng/button';
import { PushSocketService, CaptchaEvent } from '../../core/push-socket.service';
import { CaptchaHttpService } from '../../core/captcha-http.service';

const COUNTDOWN_SECONDS = 30;

/**
 * Self-managed, mounted once (unconditionally) in Shell. Queues incoming CaptchaEvents — the
 * backend can have multiple outstanding captchas at once, each with its own independent 30s
 * server-side timeout — and shows one at a time, with a client-side 30s countdown mirroring the
 * backend's own timeout.
 */
@Component({
  selector: 'app-captcha-dialog',
  imports: [FormsModule, Dialog, InputText, ButtonDirective],
  templateUrl: './captcha-dialog.html',
})
export class CaptchaDialog {
  private readonly socket = inject(PushSocketService);
  private readonly captchaHttp = inject(CaptchaHttpService);

  private readonly queue = signal<CaptchaEvent[]>([]);
  protected readonly current = computed<CaptchaEvent | null>(() => this.queue()[0] ?? null);
  protected readonly visible = computed(() => this.current() !== null);
  protected readonly secondsRemaining = signal(COUNTDOWN_SECONDS);

  solution = '';

  private countdownTimer: ReturnType<typeof setInterval> | null = null;

  /**
   * Guards against a second submit()/cancel() call landing in the same synchronous turn as a
   * first one (a double-click, or Enter-key autorepeat) before anything has had a chance to
   * re-render. Without this, the second call would read the ALREADY-ADVANCED `current()` (the
   * next queued captcha) fresh and answer it with stale/empty input the user never saw — the
   * reference check in finish() alone doesn't catch this, since a zero-arg submit()/cancel() call
   * captures `current()` live at call time, not from an earlier render. Cleared via a zero-delay
   * timer (not a microtask) so it reliably outlives the remainder of the current synchronous
   * script, including any immediately-following call, while still being gone well before any
   * later, separate click/keypress/timer tick.
   */
  private locked = false;

  constructor() {
    this.socket.captchaEvents$.subscribe((event) => {
      this.queue.update((q) => [...q, event]);
      if (this.queue().length === 1) {
        this.startCountdown();
      }
    });
  }

  protected submit(): void {
    const event = this.current();
    if (event !== null) {
      this.finish(event, this.solution);
    }
  }

  protected cancel(): void {
    const event = this.current();
    if (event !== null) {
      this.finish(event, '');
    }
  }

  protected onVisibleChange(value: boolean): void {
    if (!value) {
      this.cancel();
    }
  }

  /**
   * `event` is the specific captcha this call is meant to answer, captured by the caller at the
   * moment it decided to act (a click, an Enter keypress, or a countdown tick) — NOT re-read from
   * `current()` here. A second, stale call for an already-answered event (e.g. a double-click,
   * or Enter firing in the same instant the countdown expires) must bail out rather than silently
   * operating on whatever the queue has advanced to since, which would skip the next captcha with
   * an empty solution before the user ever saw it.
   */
  private finish(event: CaptchaEvent, solution: string): void {
    if (this.locked || event !== this.current()) {
      return;
    }
    this.locked = true;
    setTimeout(() => {
      this.locked = false;
    }, 0);
    this.captchaHttp.submitSolution(event.id, solution).subscribe({
      error: (err) => console.error('Failed to submit captcha solution', err),
    });
    this.stopCountdown();
    this.solution = '';
    this.queue.update((q) => q.slice(1));
    if (this.queue().length > 0) {
      this.startCountdown();
    }
  }

  private startCountdown(): void {
    this.secondsRemaining.set(COUNTDOWN_SECONDS);
    const event = this.current();
    this.countdownTimer = setInterval(() => {
      const remaining = this.secondsRemaining() - 1;
      this.secondsRemaining.set(remaining);
      if (remaining <= 0 && event !== null) {
        this.finish(event, '');
      }
    }, 1000);
  }

  private stopCountdown(): void {
    if (this.countdownTimer !== null) {
      clearInterval(this.countdownTimer);
      this.countdownTimer = null;
    }
  }
}
