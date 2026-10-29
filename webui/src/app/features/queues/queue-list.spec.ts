import { TestBed } from '@angular/core/testing';
import { QueueList } from './queue-list';
import { QueuesStore } from '../../state/queues.store';
import { QueueActionsService } from '../../core/queue-actions.service';
import { Queue } from '../../core/api/models';

function makeQueue(uuid: string, name: string): Queue {
  return {
    uuid,
    name,
    defaultDirectory: '/downloads',
    moveDirectory: '',
    watchDirectory: '',
    watchStatus: 'off',
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
      providers: [
        { provide: QueuesStore, useValue: queuesStoreStub },
        { provide: QueueActionsService, useValue: { resumeAll: vi.fn(), stopAll: vi.fn() } },
      ],
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

  it('flags only the queues whose watch folder has a problem', () => {
    const queuesStoreStub = {
      queues: () => [
        { ...makeQueue('q1', 'Default'), watchDirectory: '/w1', watchStatus: 'not_writable' as const },
        { ...makeQueue('q2', 'Second'), watchDirectory: '/w2', watchStatus: 'ok' as const },
      ],
      selectedQueueUuid: () => null,
      selectQueue: vi.fn(),
    };
    TestBed.configureTestingModule({
      imports: [QueueList],
      providers: [
        { provide: QueuesStore, useValue: queuesStoreStub },
        { provide: QueueActionsService, useValue: { resumeAll: vi.fn(), stopAll: vi.fn() } },
      ],
    });
    const fixture = TestBed.createComponent(QueueList);
    fixture.detectChanges();

    const flags = fixture.nativeElement.querySelectorAll('[data-testid="queue-watch-problem"]');
    expect(flags.length).toBe(1);
    expect(flags[0].getAttribute('title')).toContain("can't write");
    expect(flags[0].getAttribute('aria-hidden')).toBe('true');
    expect(flags[0].closest('li')?.textContent).toContain("Watch folder problem: FatRat can't write");
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
      providers: [
        { provide: QueuesStore, useValue: queuesStoreStub },
        { provide: QueueActionsService, useValue: { resumeAll: vi.fn(), stopAll: vi.fn() } },
      ],
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
      providers: [
        { provide: QueuesStore, useValue: queuesStoreStub },
        { provide: QueueActionsService, useValue: { resumeAll: vi.fn(), stopAll: vi.fn() } },
      ],
    });
    const fixture = TestBed.createComponent(QueueList);
    fixture.detectChanges();

    expect(fixture.nativeElement.querySelectorAll('[data-testid="queue-item"]').length).toBe(0);
  });

  it('calls selectQueue() when Enter is pressed on a queue item', () => {
    const selectQueue = vi.fn();
    const queuesStoreStub = {
      queues: () => [makeQueue('q1', 'Default')],
      selectedQueueUuid: () => null,
      selectQueue,
    };
    TestBed.configureTestingModule({
      imports: [QueueList],
      providers: [
        { provide: QueuesStore, useValue: queuesStoreStub },
        { provide: QueueActionsService, useValue: { resumeAll: vi.fn(), stopAll: vi.fn() } },
      ],
    });
    const fixture = TestBed.createComponent(QueueList);
    fixture.detectChanges();

    const item = fixture.nativeElement.querySelector('[data-testid="queue-item"]');
    item.dispatchEvent(new KeyboardEvent('keydown', { key: 'Enter' }));
    expect(selectQueue).toHaveBeenCalledWith('q1');
  });

  it('marks the selected item with aria-current', () => {
    const queuesStoreStub = {
      queues: () => [makeQueue('q1', 'Default'), makeQueue('q2', 'Second')],
      selectedQueueUuid: () => 'q2',
      selectQueue: vi.fn(),
    };
    TestBed.configureTestingModule({
      imports: [QueueList],
      providers: [
        { provide: QueuesStore, useValue: queuesStoreStub },
        { provide: QueueActionsService, useValue: { resumeAll: vi.fn(), stopAll: vi.fn() } },
      ],
    });
    const fixture = TestBed.createComponent(QueueList);
    fixture.detectChanges();

    const items = fixture.nativeElement.querySelectorAll('[data-testid="queue-item"]');
    expect(items[0].getAttribute('aria-current')).toBeNull();
    expect(items[1].getAttribute('aria-current')).toBe('true');
  });

  it('selects the queue on right-click and offers Resume All, Stop All and Properties', () => {
    const selectQueue = vi.fn();
    const resumeAll = vi.fn();
    const stopAll = vi.fn();
    const queuesStoreStub = {
      queues: () => [makeQueue('q1', 'Default'), makeQueue('q2', 'Second')],
      selectedQueueUuid: () => 'q2',
      selectQueue,
    };
    TestBed.configureTestingModule({
      imports: [QueueList],
      providers: [
        { provide: QueuesStore, useValue: queuesStoreStub },
        { provide: QueueActionsService, useValue: { resumeAll, stopAll } },
      ],
    });
    const fixture = TestBed.createComponent(QueueList);
    fixture.detectChanges();

    const item = fixture.nativeElement.querySelectorAll('[data-testid="queue-item"]')[1];
    item.dispatchEvent(new MouseEvent('contextmenu', { bubbles: true, cancelable: true }));
    expect(selectQueue).toHaveBeenCalledWith('q2');

    const items = fixture.componentInstance['contextMenuItems'] as { label?: string; command?: () => void }[];
    expect(items.map((i) => i.label)).toEqual(['Resume All', 'Stop All', undefined, 'Properties', undefined, 'Delete Queue']);
    const onProperties = vi.fn();
    const onDelete = vi.fn();
    fixture.componentInstance.propertiesRequested.subscribe(onProperties);
    fixture.componentInstance.deleteRequested.subscribe(onDelete);
    items[0].command!();
    items[1].command!();
    items[3].command!();
    items[5].command!();
    expect(resumeAll).toHaveBeenCalledWith('q2');
    expect(stopAll).toHaveBeenCalledWith('q2');
    expect(onProperties).toHaveBeenCalled();
    expect(onDelete).toHaveBeenCalled();
  });
});
