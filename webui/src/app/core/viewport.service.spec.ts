import { TestBed } from '@angular/core/testing';
import { ViewportService } from './viewport.service';

describe('ViewportService', () => {
  let matches: boolean;
  let listeners: Array<(event: MediaQueryListEvent) => void>;
  const originalMatchMedia = window.matchMedia;

  beforeEach(() => {
    listeners = [];
    window.matchMedia = vi.fn().mockImplementation(() => ({
      get matches() {
        return matches;
      },
      addEventListener: (_: string, cb: (event: MediaQueryListEvent) => void) => listeners.push(cb),
      removeEventListener: (_: string, cb: (event: MediaQueryListEvent) => void) => {
        listeners = listeners.filter((l) => l !== cb);
      },
    })) as unknown as typeof window.matchMedia;
  });

  afterEach(() => {
    window.matchMedia = originalMatchMedia;
  });

  it('starts with the current media query state', () => {
    matches = true;
    expect(TestBed.inject(ViewportService).isMobile()).toBe(true);
  });

  it('is not mobile on a wide viewport', () => {
    matches = false;
    expect(TestBed.inject(ViewportService).isMobile()).toBe(false);
  });

  it('follows media query change events', () => {
    matches = false;
    const service = TestBed.inject(ViewportService);
    listeners.forEach((l) => l({ matches: true } as MediaQueryListEvent));
    expect(service.isMobile()).toBe(true);
  });

  it('stops listening when destroyed', () => {
    matches = false;
    TestBed.inject(ViewportService);
    TestBed.resetTestingModule();
    expect(listeners).toHaveLength(0);
  });

  it('defaults to desktop when matchMedia is unavailable', () => {
    (window as { matchMedia?: unknown }).matchMedia = undefined;
    expect(TestBed.inject(ViewportService).isMobile()).toBe(false);
  });
});
