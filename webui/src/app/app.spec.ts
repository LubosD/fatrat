import { TestBed } from '@angular/core/testing';
import { App } from './app';
import { QueuesStore } from './state/queues.store';
import { TransfersStore } from './state/transfers.store';
import { PollingService } from './core/polling.service';

describe('App', () => {
  beforeEach(() => {
    TestBed.configureTestingModule({
      imports: [App],
      providers: [
        { provide: QueuesStore, useValue: { queues: () => [], selectedQueueUuid: () => null, selectQueue: () => {} } },
        { provide: TransfersStore, useValue: { transfers: () => [] } },
        { provide: PollingService, useValue: { intervalSeconds: () => 5, allowedIntervals: [2, 5, 15, 30], setIntervalSeconds: () => {} } },
      ],
    });
  });

  it('creates the app and renders the shell', () => {
    const fixture = TestBed.createComponent(App);
    fixture.detectChanges();
    expect(fixture.nativeElement.querySelector('app-shell')).toBeTruthy();
  });
});
