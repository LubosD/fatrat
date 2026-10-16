import { TestBed } from '@angular/core/testing';
import { DeleteQueueDialog } from './delete-queue-dialog';
import { Queue } from '../../core/api/models';

function q(uuid: string, name: string): Queue {
  return { uuid, name, defaultDirectory: '', moveDirectory: '', upAsDown: false, transferLimits: { down: 0, up: 0 }, speedLimits: { down: 0, up: 0 } };
}

function setup(others: Queue[]) {
  TestBed.configureTestingModule({ imports: [DeleteQueueDialog] });
  const fixture = TestBed.createComponent(DeleteQueueDialog);
  fixture.componentRef.setInput('queue', q('q1', 'Movies'));
  fixture.componentRef.setInput('otherQueues', others);
  fixture.componentRef.setInput('transferCount', 3);
  fixture.componentRef.setInput('visible', true);
  fixture.detectChanges();
  const emitted: unknown[] = [];
  fixture.componentInstance.confirmed.subscribe((o) => emitted.push(o));
  return { fixture, emitted };
}

describe('DeleteQueueDialog', () => {
  it('defaults to moving the transfers to the first other queue', () => {
    const { fixture, emitted } = setup([q('q2', 'Two'), q('q3', 'Three')]);
    expect(fixture.nativeElement.textContent).toContain('holds 3 transfers');
    (document.querySelector('[data-testid="delete-queue-confirm"]') as HTMLElement).click();
    expect(emitted).toEqual([{ moveTo: 'q2' }]);
  });

  it('defaults to deleting the transfers, keeping files, when there is no other queue', () => {
    const { emitted } = setup([]);
    (document.querySelector('[data-testid="delete-queue-confirm"]') as HTMLElement).click();
    expect(emitted).toEqual([{ deleteTransfers: true, withData: false }]);
  });

  it('can delete the transfers together with their data', () => {
    const { fixture, emitted } = setup([q('q2', 'Two')]);
    fixture.componentInstance.mode = 'deleteWithData';
    (document.querySelector('[data-testid="delete-queue-confirm"]') as HTMLElement).click();
    expect(emitted).toEqual([{ deleteTransfers: true, withData: true }]);
  });
});
