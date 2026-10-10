import { TestBed } from '@angular/core/testing';
import { CaptchaSocketService } from './captcha-socket.service';

class FakeWebSocket {
  static instances: FakeWebSocket[] = [];
  onopen: (() => void) | null = null;
  onmessage: ((event: { data: string }) => void) | null = null;
  onclose: (() => void) | null = null;
  onerror: (() => void) | null = null;
  readonly url: string;
  closeCallCount = 0;

  constructor(url: string) {
    this.url = url;
    FakeWebSocket.instances.push(this);
  }

  close(): void {
    this.closeCallCount++;
  }
}

describe('CaptchaSocketService', () => {
  beforeEach(() => {
    FakeWebSocket.instances = [];
    vi.stubGlobal('WebSocket', FakeWebSocket);
    TestBed.configureTestingModule({ providers: [CaptchaSocketService] });
  });

  afterEach(() => {
    vi.unstubAllGlobals();
    vi.useRealTimers();
  });

  it('opens a WebSocket to /websocket on the current host, matching the page protocol', () => {
    TestBed.inject(CaptchaSocketService);

    expect(FakeWebSocket.instances.length).toBe(1);
    const expectedProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    expect(FakeWebSocket.instances[0].url).toBe(`${expectedProtocol}//${window.location.host}/websocket`);
  });

  it('emits a captcha event parsed from an incoming {"type":"captcha",...} message', () => {
    const service = TestBed.inject(CaptchaSocketService);
    const received: { id: number; url: string }[] = [];
    service.captchaEvents$.subscribe((ev) => received.push(ev));

    FakeWebSocket.instances[0].onmessage?.({ data: '{"type":"captcha","id":7,"url":"http://example.com/c.png"}' });

    expect(received).toEqual([{ id: 7, url: 'http://example.com/c.png' }]);
  });

  it('ignores a ping message', () => {
    const service = TestBed.inject(CaptchaSocketService);
    const received: unknown[] = [];
    service.captchaEvents$.subscribe((ev) => received.push(ev));

    FakeWebSocket.instances[0].onmessage?.({ data: '{"type":"ping"}' });

    expect(received).toEqual([]);
  });

  it('ignores an unparseable message instead of throwing', () => {
    const service = TestBed.inject(CaptchaSocketService);
    const received: unknown[] = [];
    service.captchaEvents$.subscribe((ev) => received.push(ev));
    vi.spyOn(console, 'error').mockImplementation(() => {});

    expect(() => FakeWebSocket.instances[0].onmessage?.({ data: 'not json' })).not.toThrow();
    expect(received).toEqual([]);
  });

  it('reconnects after the connection closes, starting with a 1 second backoff', () => {
    vi.useFakeTimers();
    TestBed.inject(CaptchaSocketService);
    expect(FakeWebSocket.instances.length).toBe(1);

    FakeWebSocket.instances[0].onclose?.();
    vi.advanceTimersByTime(999);
    expect(FakeWebSocket.instances.length).toBe(1);
    vi.advanceTimersByTime(1);
    expect(FakeWebSocket.instances.length).toBe(2);
  });

  it('doubles the backoff on repeated failures, up to a 30 second cap', () => {
    vi.useFakeTimers();
    TestBed.inject(CaptchaSocketService);

    FakeWebSocket.instances[0].onclose?.(); // schedules reconnect #1 after 1s
    vi.advanceTimersByTime(1000);
    expect(FakeWebSocket.instances.length).toBe(2);

    FakeWebSocket.instances[1].onclose?.(); // schedules reconnect #2 after 2s
    vi.advanceTimersByTime(1999);
    expect(FakeWebSocket.instances.length).toBe(2);
    vi.advanceTimersByTime(1);
    expect(FakeWebSocket.instances.length).toBe(3);

    FakeWebSocket.instances[2].onclose?.(); // schedules reconnect #3 after 4s
    vi.advanceTimersByTime(3999);
    expect(FakeWebSocket.instances.length).toBe(3);
    vi.advanceTimersByTime(1);
    expect(FakeWebSocket.instances.length).toBe(4);
  });

  it('resets the backoff to 1 second after a successful reconnect', () => {
    vi.useFakeTimers();
    TestBed.inject(CaptchaSocketService);

    FakeWebSocket.instances[0].onclose?.(); // backoff becomes 2s for next attempt
    vi.advanceTimersByTime(1000);
    expect(FakeWebSocket.instances.length).toBe(2);

    FakeWebSocket.instances[1].onopen?.(); // connection succeeds: backoff resets to 1s
    FakeWebSocket.instances[1].onclose?.(); // fails again
    vi.advanceTimersByTime(999);
    expect(FakeWebSocket.instances.length).toBe(2); // not yet — proves backoff reset to 1s, not left at 2s
    vi.advanceTimersByTime(1);
    expect(FakeWebSocket.instances.length).toBe(3);
  });

  it('calls close() on the socket when it errors', () => {
    TestBed.inject(CaptchaSocketService);

    FakeWebSocket.instances[0].onerror?.();

    expect(FakeWebSocket.instances[0].closeCallCount).toBe(1);
  });
});
