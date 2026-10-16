import { DestroyRef, Injectable, inject, signal } from '@angular/core';

export const MOBILE_MEDIA_QUERY = '(max-width: 768px)';

@Injectable({ providedIn: 'root' })
export class ViewportService {
  readonly isMobile = signal(false);

  constructor() {
    if (typeof window === 'undefined' || typeof window.matchMedia !== 'function') {
      return;
    }
    const query = window.matchMedia(MOBILE_MEDIA_QUERY);
    this.isMobile.set(query.matches);
    const onChange = (event: MediaQueryListEvent): void => this.isMobile.set(event.matches);
    query.addEventListener('change', onChange);
    inject(DestroyRef).onDestroy(() => query.removeEventListener('change', onChange));
  }
}
