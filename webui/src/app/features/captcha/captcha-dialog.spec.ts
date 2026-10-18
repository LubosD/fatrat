import { TestBed } from '@angular/core/testing';
import { Subject, of } from 'rxjs';
import { CaptchaDialog } from './captcha-dialog';
import { PushSocketService, CaptchaEvent } from '../../core/push-socket.service';
import { CaptchaHttpService } from '../../core/captcha-http.service';

function configure(submitSolution = vi.fn().mockReturnValue(of(undefined))) {
  const captchaEvents$ = new Subject<CaptchaEvent>();
  TestBed.configureTestingModule({
    imports: [CaptchaDialog],
    providers: [
      { provide: PushSocketService, useValue: { captchaEvents$ } },
      { provide: CaptchaHttpService, useValue: { submitSolution } },
    ],
  });
  return { captchaEvents$, submitSolution };
}

describe('CaptchaDialog', () => {
  afterEach(() => vi.useRealTimers());

  it('is hidden when there is no pending captcha event', () => {
    configure();
    const fixture = TestBed.createComponent(CaptchaDialog);
    fixture.detectChanges();

    expect(fixture.componentInstance['visible']()).toBe(false);
  });

  it('becomes visible and shows the event when a captcha event arrives', () => {
    const { captchaEvents$ } = configure();
    const fixture = TestBed.createComponent(CaptchaDialog);
    fixture.detectChanges();

    captchaEvents$.next({ id: 1, url: 'http://example.com/c.png' });
    fixture.detectChanges();

    expect(fixture.componentInstance['visible']()).toBe(true);
    expect(fixture.componentInstance['current']()).toEqual({ id: 1, url: 'http://example.com/c.png' });
  });

  it('submits the typed solution and hides the dialog', () => {
    const { captchaEvents$, submitSolution } = configure();
    const fixture = TestBed.createComponent(CaptchaDialog);
    fixture.detectChanges();
    captchaEvents$.next({ id: 1, url: 'http://example.com/c.png' });
    fixture.detectChanges();

    fixture.componentInstance['solution'] = 'abc123';
    fixture.componentInstance['submit']();

    expect(submitSolution).toHaveBeenCalledWith(1, 'abc123');
    expect(fixture.componentInstance['visible']()).toBe(false);
  });

  it('cancel submits an empty solution and hides the dialog', () => {
    const { captchaEvents$, submitSolution } = configure();
    const fixture = TestBed.createComponent(CaptchaDialog);
    fixture.detectChanges();
    captchaEvents$.next({ id: 2, url: 'http://example.com/c2.png' });
    fixture.detectChanges();

    fixture.componentInstance['cancel']();

    expect(submitSolution).toHaveBeenCalledWith(2, '');
    expect(fixture.componentInstance['visible']()).toBe(false);
  });

  it('closing the dialog (e.g. via its close icon) behaves like cancel', () => {
    const { captchaEvents$, submitSolution } = configure();
    const fixture = TestBed.createComponent(CaptchaDialog);
    fixture.detectChanges();
    captchaEvents$.next({ id: 3, url: 'http://example.com/c3.png' });
    fixture.detectChanges();

    fixture.componentInstance['onVisibleChange'](false);

    expect(submitSolution).toHaveBeenCalledWith(3, '');
    expect(fixture.componentInstance['visible']()).toBe(false);
  });

  it('automatically submits an empty solution and hides when the 30s countdown reaches zero', () => {
    vi.useFakeTimers();
    const { captchaEvents$, submitSolution } = configure();
    const fixture = TestBed.createComponent(CaptchaDialog);
    fixture.detectChanges();
    captchaEvents$.next({ id: 4, url: 'http://example.com/c4.png' });
    fixture.detectChanges();

    vi.advanceTimersByTime(30000);

    expect(submitSolution).toHaveBeenCalledWith(4, '');
    expect(fixture.componentInstance['visible']()).toBe(false);
  });

  it('counts down from 30 seconds, one per second', () => {
    vi.useFakeTimers();
    const { captchaEvents$ } = configure();
    const fixture = TestBed.createComponent(CaptchaDialog);
    fixture.detectChanges();
    captchaEvents$.next({ id: 5, url: 'http://example.com/c5.png' });
    fixture.detectChanges();

    expect(fixture.componentInstance['secondsRemaining']()).toBe(30);
    vi.advanceTimersByTime(1000);
    expect(fixture.componentInstance['secondsRemaining']()).toBe(29);
    vi.advanceTimersByTime(5000);
    expect(fixture.componentInstance['secondsRemaining']()).toBe(24);
  });

  it('queues a second captcha event and shows it after the first is answered, restarting the countdown', () => {
    vi.useFakeTimers();
    const { captchaEvents$, submitSolution } = configure();
    const fixture = TestBed.createComponent(CaptchaDialog);
    fixture.detectChanges();
    captchaEvents$.next({ id: 6, url: 'http://example.com/c6.png' });
    fixture.detectChanges();
    captchaEvents$.next({ id: 7, url: 'http://example.com/c7.png' }); // arrives while #6 is showing
    fixture.detectChanges();

    expect(fixture.componentInstance['current']()).toEqual({ id: 6, url: 'http://example.com/c6.png' }); // #6 still showing, #7 queued

    vi.advanceTimersByTime(10000);
    fixture.componentInstance['solution'] = 'first';
    fixture.componentInstance['submit']();
    fixture.detectChanges();

    expect(submitSolution).toHaveBeenCalledWith(6, 'first');
    expect(fixture.componentInstance['current']()).toEqual({ id: 7, url: 'http://example.com/c7.png' });
    expect(fixture.componentInstance['visible']()).toBe(true);
    expect(fixture.componentInstance['secondsRemaining']()).toBe(30); // countdown restarted for #7

    vi.advanceTimersByTime(30000);
    expect(submitSolution).toHaveBeenCalledWith(7, '');
    expect(fixture.componentInstance['visible']()).toBe(false);
  });

  it('calling submit() twice in a row when a second event is already queued only answers the first event once, leaving the second untouched', () => {
    vi.useFakeTimers();
    const { captchaEvents$, submitSolution } = configure();
    const fixture = TestBed.createComponent(CaptchaDialog);
    fixture.detectChanges();
    captchaEvents$.next({ id: 10, url: 'http://example.com/c10.png' });
    fixture.detectChanges();
    captchaEvents$.next({ id: 11, url: 'http://example.com/c11.png' }); // queued behind #10
    fixture.detectChanges();

    fixture.componentInstance['solution'] = 'first';
    fixture.componentInstance['submit'](); // answers #10, advances to #11
    fixture.componentInstance['submit'](); // stale second call — must NOT also answer #11

    expect(submitSolution).toHaveBeenCalledTimes(1);
    expect(submitSolution).toHaveBeenCalledWith(10, 'first');
    expect(fixture.componentInstance['current']()).toEqual({ id: 11, url: 'http://example.com/c11.png' });
    expect(fixture.componentInstance['visible']()).toBe(true);
  });

  it('clears the typed solution field between captchas', () => {
    vi.useFakeTimers();
    const { captchaEvents$ } = configure();
    const fixture = TestBed.createComponent(CaptchaDialog);
    fixture.detectChanges();
    captchaEvents$.next({ id: 8, url: 'http://example.com/c8.png' });
    captchaEvents$.next({ id: 9, url: 'http://example.com/c9.png' });
    fixture.detectChanges();

    fixture.componentInstance['solution'] = 'leftover text';
    fixture.componentInstance['submit']();

    expect(fixture.componentInstance['solution']).toBe('');
  });
});
