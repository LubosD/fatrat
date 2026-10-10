import { Component, computed, inject, signal } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { Dialog } from 'primeng/dialog';
import { InputText } from 'primeng/inputtext';
import { ButtonDirective } from 'primeng/button';
import { CaptchaSocketService, CaptchaEvent } from '../../core/captcha-socket.service';
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
  private readonly socket = inject(CaptchaSocketService);
  private readonly captchaHttp = inject(CaptchaHttpService);

  private readonly queue = signal<CaptchaEvent[]>([]);
  protected readonly current = computed<CaptchaEvent | null>(() => this.queue()[0] ?? null);
  protected readonly visible = computed(() => this.current() !== null);
  protected readonly secondsRemaining = signal(COUNTDOWN_SECONDS);

  solution = '';

  private countdownTimer: ReturnType<typeof setInterval> | null = null;

  constructor() {
    this.socket.captchaEvents$.subscribe((event) => {
      this.queue.update((q) => [...q, event]);
      if (this.queue().length === 1) {
        this.startCountdown();
      }
    });
  }

  protected submit(): void {
    this.finish(this.solution);
  }

  protected cancel(): void {
    this.finish('');
  }

  protected onVisibleChange(value: boolean): void {
    if (!value) {
      this.cancel();
    }
  }

  private finish(solution: string): void {
    const event = this.current();
    if (event === null) {
      return;
    }
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
    this.countdownTimer = setInterval(() => {
      const remaining = this.secondsRemaining() - 1;
      this.secondsRemaining.set(remaining);
      if (remaining <= 0) {
        this.finish('');
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
