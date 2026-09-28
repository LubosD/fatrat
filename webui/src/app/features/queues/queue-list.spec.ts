import { TestBed } from '@angular/core/testing';
import { QueueList } from './queue-list';
import { QueuesStore } from '../../state/queues.store';
import { Queue } from '../../core/api/models';

function makeQueue(uuid: string, name: string): Queue {
  return {
    uuid,
    name,
    defaultDirectory: '/downloads',
    moveDirectory: '',
    upAsDown: false,
    transferLimits: { down: 0, up: 0 },
    speedLimits: { down: 0, up: 0 },
  };
}

describe('QueueList', () => {
  it('renders one entry per queue and marks the selected one', () => {
    const queuesStoreStub = {
      queues: () => [makeQueue('q1', 'Default'), makeQueue('q2', 'Second')],
      selectedQueueUuid: () => 'q2',
      selectQueue: vi.fn(),
    };
    TestBed.configureTestingModule({
      imports: [QueueList],
      providers: [{ provide: QueuesStore, useValue: queuesStoreStub }],
    });
    const fixture = TestBed.createComponent(QueueList);
    fixture.detectChanges();

    const items = fixture.nativeElement.querySelectorAll('[data-testid="queue-item"]');
    expect(items.length).toBe(2);
    expect(items[0].textContent).toContain('Default');
    expect(items[1].textContent).toContain('Second');
    expect(items[1].classList.contains('selected')).toBe(true);
    expect(items[0].classList.contains('selected')).toBe(false);
  });

  it('calls selectQueue() when a queue is clicked', () => {
    const selectQueue = vi.fn();
    const queuesStoreStub = {
      queues: () => [makeQueue('q1', 'Default')],
      selectedQueueUuid: () => null,
      selectQueue,
    };
    TestBed.configureTestingModule({
      imports: [QueueList],
      providers: [{ provide: QueuesStore, useValue: queuesStoreStub }],
    });
    const fixture = TestBed.createComponent(QueueList);
    fixture.detectChanges();

    fixture.nativeElement.querySelector('[data-testid="queue-item"]').click();
    expect(selectQueue).toHaveBeenCalledWith('q1');
  });

  it('renders nothing when there are no queues', () => {
    const queuesStoreStub = { queues: () => [], selectedQueueUuid: () => null, selectQueue: vi.fn() };
    TestBed.configureTestingModule({
      imports: [QueueList],
      providers: [{ provide: QueuesStore, useValue: queuesStoreStub }],
    });
    const fixture = TestBed.createComponent(QueueList);
    fixture.detectChanges();

    expect(fixture.nativeElement.querySelectorAll('[data-testid="queue-item"]').length).toBe(0);
  });
});
