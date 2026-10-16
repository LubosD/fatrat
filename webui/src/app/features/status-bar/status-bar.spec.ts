import { TestBed } from '@angular/core/testing';
import { of } from 'rxjs';
import { signal } from '@angular/core';
import { StatusBar } from './status-bar';
import { TransferApi } from '../../core/api/transfer-api';
import { StatsStore } from '../../state/stats.store';

describe('StatusBar', () => {
  let setSettingsValues: ReturnType<typeof vi.fn>;
  let applySettings: ReturnType<typeof vi.fn>;

  function create(limits: unknown[] = [0, 0]) {
    setSettingsValues = vi.fn().mockReturnValue(of(undefined));
    applySettings = vi.fn().mockReturnValue(of(undefined));
    TestBed.configureTestingModule({
      providers: [
        {
          provide: TransferApi,
          useValue: { getSettingsValues: () => of(limits), setSettingsValues, applySettings },
        },
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

  it('shows ∞ for unlimited and a rate for a set limit', () => {
    const el: HTMLElement = create([0, 512000]).nativeElement;
    expect(el.querySelector('[data-testid=limit-down]')!.textContent).toContain('∞');
    expect(el.querySelector('[data-testid=limit-up]')!.textContent).toContain('500.0 KB/s');
  });

  it('saves the edited limit in bytes/s, keeps the other direction, and applies settings', () => {
    const fixture = create([0, 512000]);
    const cmp = fixture.componentInstance as any;
    const pop = { toggle: vi.fn(), hide: vi.fn() };

    cmp.open(pop, new Event('click'), 'down');
    cmp.apply(pop, 250);

    expect(setSettingsValues).toHaveBeenCalledWith(['network/speed_down', 'network/speed_up'], [256000, 512000]);
    expect(applySettings).toHaveBeenCalled();
    expect(cmp.limits()).toEqual({ down: 256000, up: 512000 });
    expect(pop.hide).toHaveBeenCalled();
  });

  it('treats 0 / empty as unlimited', () => {
    const fixture = create([1024, 0]);
    const cmp = fixture.componentInstance as any;
    const pop = { toggle: vi.fn(), hide: vi.fn() };

    cmp.open(pop, new Event('click'), 'down');
    expect(cmp.limitKb).toBe(1);
    cmp.apply(pop, null);

    expect(setSettingsValues).toHaveBeenCalledWith(['network/speed_down', 'network/speed_up'], [0, 0]);
  });
});
