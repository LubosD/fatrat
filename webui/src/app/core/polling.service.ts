import { Injectable, signal } from '@angular/core';
import { BehaviorSubject, Observable, interval } from 'rxjs';
import { map, shareReplay, switchMap } from 'rxjs/operators';

const STORAGE_KEY = 'fatrat.webui.pollIntervalSeconds';
const ALLOWED_INTERVALS = [2, 5, 15, 30] as const;
const DEFAULT_INTERVAL_SECONDS = 5;

function readStoredInterval(): number {
  const raw = window.localStorage.getItem(STORAGE_KEY);
  const parsed = raw === null ? NaN : Number(raw);
  return ALLOWED_INTERVALS.includes(parsed as (typeof ALLOWED_INTERVALS)[number])
    ? parsed
    : DEFAULT_INTERVAL_SECONDS;
}

@Injectable({ providedIn: 'root' })
export class PollingService {
  readonly allowedIntervals = ALLOWED_INTERVALS;
  readonly intervalSeconds = signal(readStoredInterval());

  // A RxJS BehaviorSubject (not the `intervalSeconds` signal) drives ticks$, so
  // interval changes propagate synchronously through switchMap with no
  // dependency on Angular's effect/change-detection scheduling — using
  // toObservable(this.intervalSeconds) here instead would tie the first
  // emission and every restart to an effect flush, which is not guaranteed
  // to happen synchronously with a plain `.set()` call (particularly under
  // fake timers in tests, where nothing drives Angular's own scheduler).
  // BehaviorSubject (rather than a plain Subject) also replays its current
  // value to every new subscriber, so a subscriber that joins ticks$ after
  // the interval has already changed starts from the current interval
  // instead of the construction-time default.
  private readonly intervalChanges$ = new BehaviorSubject<number>(this.intervalSeconds());

  // Multicast with shareReplay({ bufferSize: 1, refCount: false }) (not plain share()) so every
  // subscriber — regardless of how many, or when they join — observes the SAME underlying timer
  // in lockstep, rather than each .subscribe() re-running the pipe and starting its own fresh
  // interval(). This matters because consumers like activeGatedPoll() subscribe to ticks$ fresh
  // on every selection change: with a cold source each such resubscription would reset that
  // stream's timer independently of any other stream subscribed to ticks$ elsewhere. refCount:
  // false (rather than true) keeps the shared interval() alive even when the subscriber count
  // drops to zero (e.g. a tab closed and reopened), instead of tearing it down and restarting it
  // from scratch on the next subscription — appropriate here because PollingService is a
  // `providedIn: 'root'` singleton for the lifetime of the app, so there's no real "fully torn
  // down" state to economize for, and refCount: true would reintroduce a milder version of the
  // same resync bug this fix is for. bufferSize: 1 means a late subscriber immediately replays
  // the most recent tick instead of waiting for the next one — this IS significant for
  // activeGatedPoll(): a replayed tick arrives synchronously right after its `startWith(null)`
  // sentinel on every resubscribe, so activeGatedPoll can no longer tell "this is the activation
  // emission" from tick identity alone (`tick === null`). It now tracks activation with a
  // per-subscription "has this selection emitted yet" flag instead — see active-gated-poll.ts —
  // specifically because of this replay behavior, not despite it.
  /** Emits an increasing tick count every `intervalSeconds()` seconds; switching intervals cancels the pending wait and restarts immediately. Multicast — all subscribers share one timer. */
  readonly ticks$: Observable<number> = this.intervalChanges$.pipe(
    switchMap((seconds) => interval(seconds * 1000).pipe(map((n) => n + 1))),
    shareReplay({ bufferSize: 1, refCount: false }),
  );

  setIntervalSeconds(seconds: number): void {
    if (!ALLOWED_INTERVALS.includes(seconds as (typeof ALLOWED_INTERVALS)[number])) {
      return;
    }
    if (seconds === this.intervalSeconds()) {
      return;
    }
    this.intervalSeconds.set(seconds);
    this.intervalChanges$.next(seconds);
    window.localStorage.setItem(STORAGE_KEY, String(seconds));
  }
}
