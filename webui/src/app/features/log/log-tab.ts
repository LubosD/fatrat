import { Component, ElementRef, ViewChild, computed, inject, input, signal } from '@angular/core';
import { takeUntilDestroyed, toObservable } from '@angular/core/rxjs-interop';
import { catchError, of } from 'rxjs';
import { LogService } from '../../core/log.service';
import { TransfersStore } from '../../state/transfers.store';
import { PollingService } from '../../core/polling.service';
import { activeGatedPoll } from '../../core/active-gated-poll';

@Component({
  selector: 'app-log-tab',
  templateUrl: './log-tab.html',
})
export class LogTab {
  readonly active = input(false);

  private readonly logService = inject(LogService);
  private readonly transfersStore = inject(TransfersStore);
  private readonly polling = inject(PollingService);

  protected readonly globalLog = signal('');
  protected readonly transferLog = signal('');

  protected readonly selectedTransferUuid = computed(() => {
    const uuids = this.transfersStore.selectedUuids();
    return uuids.length === 1 ? uuids[0] : null;
  });

  @ViewChild('globalLogEl') private globalLogEl?: ElementRef<HTMLElement>;
  @ViewChild('transferLogEl') private transferLogEl?: ElementRef<HTMLElement>;

  constructor() {
    // of(undefined) satisfies activeGatedPoll's "selection$ must replay its current value to new
    // subscribers" precondition here only because it's a true constant: a cold Observable
    // re-executes (and so re-emits undefined) on every fresh subscription, which happens to look
    // like a replay. Don't copy this for a selection that can actually change over time — that
    // needs a genuinely replaying source (BehaviorSubject, ReplaySubject(1), or toObservable()).
    activeGatedPoll(toObservable(this.active), of(undefined), this.polling.ticks$, () =>
      this.logService.getGlobalLog().pipe(
        catchError((err) => {
          console.error('Failed to fetch global log', err);
          return of('');
        }),
      ),
    )
      .pipe(takeUntilDestroyed())
      .subscribe(({ value, isActivation }) => {
        this.globalLog.set(value);
        if (isActivation) {
          this.scrollToBottom();
        }
      });

    activeGatedPoll(
      toObservable(this.active),
      toObservable(this.selectedTransferUuid),
      this.polling.ticks$,
      (uuid: string | null) =>
        uuid
          ? this.logService.getTransferLog(uuid).pipe(
              catchError((err) => {
                console.error('Failed to fetch transfer log', err);
                return of('');
              }),
            )
          : of(''),
    )
      .pipe(takeUntilDestroyed())
      .subscribe(({ value, isActivation }) => {
        this.transferLog.set(value);
        if (isActivation) {
          this.scrollToBottom();
        }
      });
  }

  /**
   * Extracted so tests can spy on it instead of asserting real scrollTop/scrollHeight values
   * (fragile under this project's happy-dom test environment, which has no real layout engine —
   * same rationale as TransferDetails's navigateTo() extraction in Phase 3a). Scrolls whichever
   * of the two panels currently exist in the DOM (the transfer-log panel may be absent when no
   * single transfer is selected).
   */
  private scrollToBottom(): void {
    for (const ref of [this.globalLogEl, this.transferLogEl]) {
      if (ref) {
        ref.nativeElement.scrollTop = ref.nativeElement.scrollHeight;
      }
    }
  }
}
