import { Injectable, signal } from '@angular/core';
import { BehaviorSubject, Observable, interval } from 'rxjs';
import { map, switchMap } from 'rxjs/operators';

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

  /** Emits an increasing tick count every `intervalSeconds()` seconds; switching intervals cancels the pending wait and restarts immediately. */
  readonly ticks$: Observable<number> = this.intervalChanges$.pipe(
    switchMap((seconds) => interval(seconds * 1000).pipe(map((n) => n + 1))),
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
