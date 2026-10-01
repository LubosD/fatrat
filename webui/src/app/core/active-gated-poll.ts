import { EMPTY, Observable } from 'rxjs';
import { map, startWith, switchMap } from 'rxjs/operators';

export interface PollEmission<T> {
  value: T;
  /** True for the emission immediately triggered by activation or a selection change, before any poll tick has fired for it. */
  isActivation: boolean;
}

/**
 * Composes "fetch `selection`'s data while `active` is true, refetching on every poll tick and
 * immediately whenever `selection` changes" into one Observable. Every layer is a switchMap, so
 * an in-flight request that's no longer relevant (selection changed, or went inactive) is
 * cancelled outright — there's nothing left to apply late, which is what makes the "stale
 * response overwrites current state" bug class structurally impossible here, rather than
 * something each caller has to guard against by hand with a captured-uuid comparison.
 *
 * `selection$` must replay its current value to new subscribers. Each time `active$` re-emits,
 * the outer switchMap unsubscribes from and re-subscribes to `selection$.pipe(...)` from
 * scratch; a plain hot `Subject` with no replay would give that fresh subscription nothing
 * until the *next* selection change, so reactivating (inactive -> active again) would silently
 * produce no fetch at all. Use a source that replays its latest value to new subscribers, such
 * as a signal-derived observable via Angular's `toObservable()` (which is backed internally by
 * a `ReplaySubject(1)`), or a `BehaviorSubject`/`ReplaySubject(1)` directly.
 */
export function activeGatedPoll<S, T>(
  active$: Observable<boolean>,
  selection$: Observable<S>,
  ticks$: Observable<number>,
  fetch: (selection: S) => Observable<T>,
): Observable<PollEmission<T>> {
  return active$.pipe(
    switchMap((active) =>
      !active
        ? EMPTY
        : selection$.pipe(
            switchMap((selection) =>
              ticks$.pipe(
                startWith(null as number | null),
                switchMap((tick) =>
                  fetch(selection).pipe(map((value) => ({ value, isActivation: tick === null }))),
                ),
              ),
            ),
          ),
    ),
  );
}
