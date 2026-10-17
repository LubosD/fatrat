import { TestBed } from '@angular/core/testing';
import { of } from 'rxjs';
import { signal } from '@angular/core';
import { StatusBar } from './status-bar';
import { TransferApi } from '../../core/api/transfer-api';
import { StatsStore } from '../../state/stats.store';

describe('StatusBar', () => {
  let api: { getSettingsValues: ReturnType<typeof vi.fn>; setSettingsValues: ReturnType<typeof vi.fn> };

  function create() {
    api = { getSettingsValues: vi.fn().mockReturnValue(of([])), setSettingsValues: vi.fn().mockReturnValue(of(undefined)) };
    TestBed.configureTestingModule({
      providers: [
        { provide: TransferApi, useValue: api },
        {
          provide: StatsStore,
          useValue: {
            total: signal({ down: 2048, up: 1024 }),
            selectedQueue: signal({ down: 512, up: 0 }),
          },
        },
      ],
    });
    const fixture = TestBed.createComponent(StatusBar);
    fixture.detectChanges();
    return fixture;
  }

  it('shows total and selected-queue rates', () => {
    const el: HTMLElement = create().nativeElement;
    expect(el.querySelector('[data-testid=total-rate]')!.textContent).toContain('2.0 KB/s');
    expect(el.querySelector('[data-testid=total-rate]')!.textContent).toContain('1.0 KB/s');
    expect(el.querySelector('[data-testid=queue-rate]')!.textContent).toContain('512 B/s');
  });

  // FatRat has no global speed limit; network/speed_* is the desktop app's "line speed", which only
  // scales its bandwidth bars, so offering it as a limit would do nothing.
  it('offers no global limit and leaves the line-speed setting alone', () => {
    const el: HTMLElement = create().nativeElement;
    expect(el.querySelector('[data-testid=limit-down]')).toBeNull();
    expect(el.querySelector('[data-testid=limit-up]')).toBeNull();
    expect(el.textContent).not.toContain('Global limit');
    expect(api.getSettingsValues).not.toHaveBeenCalled();
    expect(api.setSettingsValues).not.toHaveBeenCalled();
  });
});
