import { TestBed } from '@angular/core/testing';
import { TransferTable } from './transfer-table';
import { TransfersStore } from '../../state/transfers.store';
import { TransferActionsService } from '../../core/transfer-actions.service';
import { Transfer } from '../../core/api/models';
import { of } from 'rxjs';
import { PluginActions } from './plugin-actions';

function makeTransfer(overrides: Partial<Transfer> = {}): Transfer {
  return {
    uuid: 't1',
    name: 'file.zip',
    state: 'Active',
    class: 'HttpDownload',
    message: 'Downloading',
    mode: 'Download',
    primaryMode: 'Download',
    dataPath: '/downloads/file.zip',
    dataPathIsDir: false,
    total: 1000,
    done: 250,
    comment: '',
    completionCommand: '',
    object: '',
    timeRunning: 10,
    speeds: { down: 2048, up: 0 },
    userSpeedLimits: { down: 0, up: 0 },
    ...overrides,
  };
}

function makeTransfersStoreStub(transfers: Transfer[], selected: string[] = []) {
  return {
    transfers: () => transfers,
    selectedUuids: () => selected,
    isSelected: (uuid: string) => selected.includes(uuid),
    currentUuid: () => null as string | null,
    toggleSelection: vi.fn(),
    selectOnly: vi.fn(),
    select: vi.fn(),
    selectAll: vi.fn(),
    moveCurrent: vi.fn(),
  };
}

function createTable(store: ReturnType<typeof makeTransfersStoreStub>) {
  TestBed.configureTestingModule({
    imports: [TransferTable],
    providers: [
      { provide: TransfersStore, useValue: store },
      { provide: TransferActionsService, useValue: makeActionsStub() },
      { provide: PluginActions, useValue: { menuItems: () => of([]) } },
    ],
  });
  const fixture = TestBed.createComponent(TransferTable);
  fixture.detectChanges();
  return fixture;
}

function rows(fixture: ReturnType<typeof createTable>): HTMLElement[] {
  return Array.from(fixture.nativeElement.querySelectorAll('[data-testid="transfer-row"]'));
}

function pressKey(fixture: ReturnType<typeof createTable>, init: KeyboardEventInit): KeyboardEvent {
  const event = new KeyboardEvent('keydown', { bubbles: true, cancelable: true, ...init });
  fixture.nativeElement.querySelector('[data-testid="transfer-table"]').dispatchEvent(event);
  return event;
}

function createTableWithActions(store: ReturnType<typeof makeTransfersStoreStub>) {
  const actions = makeActionsStub();
  TestBed.configureTestingModule({
    imports: [TransferTable],
    providers: [
      { provide: TransfersStore, useValue: store },
      { provide: TransferActionsService, useValue: actions },
      { provide: PluginActions, useValue: { menuItems: () => of([]) } },
    ],
  });
  const fixture = TestBed.createComponent(TransferTable);
  fixture.detectChanges();
  return { fixture, actions };
}

function dragEvent(type: string, clientY = 0): DragEvent {
  const e = new Event(type, { bubbles: true, cancelable: true }) as unknown as { clientY: number; dataTransfer: unknown };
  e.clientY = clientY;
  e.dataTransfer = { setData: vi.fn(), effectAllowed: '', dropEffect: '' };
  return e as unknown as DragEvent;
}

function makeActionsStub() {
  return {
    resume: vi.fn(),
    forceResume: vi.fn(),
    pause: vi.fn(),
    deleteSelected: vi.fn(),
    deleteCompleted: vi.fn(),
    moveSelected: vi.fn(),
    moveBefore: vi.fn(),
  };
}

describe('TransferTable', () => {
  it('renders one row per transfer with name, progress, size, speeds, ETA and message', () => {
    TestBed.configureTestingModule({
      imports: [TransferTable],
      providers: [
        { provide: TransfersStore, useValue: makeTransfersStoreStub([makeTransfer()]) },
        { provide: TransferActionsService, useValue: makeActionsStub() },
        { provide: PluginActions, useValue: { menuItems: () => of([]) } },
      ],
    });
    const fixture = TestBed.createComponent(TransferTable);
    fixture.detectChanges();

    const text = fixture.nativeElement.textContent as string;
    expect(text).toContain('file.zip');
    expect(text).toContain('25%');
    expect(text).toContain('1000 B');
    expect(text).toContain('2.0 KB/s');
    expect(text).toContain('Downloading');
  });

  it('renders an empty table when there are no transfers', () => {
    TestBed.configureTestingModule({
      imports: [TransferTable],
      providers: [
        { provide: TransfersStore, useValue: makeTransfersStoreStub([]) },
        { provide: TransferActionsService, useValue: makeActionsStub() },
        { provide: PluginActions, useValue: { menuItems: () => of([]) } },
      ],
    });
    const fixture = TestBed.createComponent(TransferTable);
    fixture.detectChanges();

    expect(fixture.nativeElement.querySelectorAll('tbody tr').length).toBe(0);
  });

  it('trackByUuid returns the transfer uuid for row identity tracking', () => {
    TestBed.configureTestingModule({
      imports: [TransferTable],
      providers: [
        { provide: TransfersStore, useValue: makeTransfersStoreStub([]) },
        { provide: TransferActionsService, useValue: makeActionsStub() },
        { provide: PluginActions, useValue: { menuItems: () => of([]) } },
      ],
    });
    const fixture = TestBed.createComponent(TransferTable);
    const transfer = makeTransfer();
    expect(fixture.componentInstance['trackByUuid'](0, transfer)).toBe('t1');
  });

  it('renders no checkboxes; selection is shown via aria-selected on the row', () => {
    const fixture = createTable(
      makeTransfersStoreStub([makeTransfer({ uuid: 't1' }), makeTransfer({ uuid: 't2', name: 'other.zip' })], ['t2']),
    );

    expect(fixture.nativeElement.querySelectorAll('input[type="checkbox"]').length).toBe(0);
    const [r1, r2] = rows(fixture);
    expect(r1.getAttribute('aria-selected')).toBe('false');
    expect(r2.getAttribute('aria-selected')).toBe('true');
    expect(r2.classList).toContain('selected-row');
  });

  it('clicking a row calls TransfersStore.select() with the held modifiers', () => {
    const store = makeTransfersStoreStub([makeTransfer({ uuid: 't1' })]);
    const fixture = createTable(store);
    const [row] = rows(fixture);

    row.dispatchEvent(new MouseEvent('click', { bubbles: true }));
    expect(store.select).toHaveBeenLastCalledWith('t1', { ctrl: false, shift: false });

    row.dispatchEvent(new MouseEvent('click', { bubbles: true, ctrlKey: true }));
    expect(store.select).toHaveBeenLastCalledWith('t1', { ctrl: true, shift: false });

    row.dispatchEvent(new MouseEvent('click', { bubbles: true, metaKey: true, shiftKey: true }));
    expect(store.select).toHaveBeenLastCalledWith('t1', { ctrl: true, shift: true });
  });

  it('arrow keys move the current row, extending the selection with Shift', () => {
    const store = makeTransfersStoreStub([makeTransfer({ uuid: 't1' })]);
    const fixture = createTable(store);

    expect(pressKey(fixture, { key: 'ArrowDown' }).defaultPrevented).toBe(true);
    expect(store.moveCurrent).toHaveBeenLastCalledWith(1, { shift: false });
    pressKey(fixture, { key: 'ArrowUp', shiftKey: true });
    expect(store.moveCurrent).toHaveBeenLastCalledWith(-1, { shift: true });
    pressKey(fixture, { key: 'End' });
    expect(store.moveCurrent).toHaveBeenLastCalledWith(Infinity, { shift: false });
  });

  it('Ctrl+A selects all transfers, a bare "a" does nothing', () => {
    const store = makeTransfersStoreStub([makeTransfer({ uuid: 't1' })]);
    const fixture = createTable(store);

    expect(pressKey(fixture, { key: 'a' }).defaultPrevented).toBe(false);
    expect(store.selectAll).not.toHaveBeenCalled();
    pressKey(fixture, { key: 'a', ctrlKey: true });
    expect(store.selectAll).toHaveBeenCalled();
  });

  it('renders a state icon per row, using the upload variant for uploads', () => {
    const fixture = createTable(
      makeTransfersStoreStub([
        makeTransfer({ uuid: 't1', state: 'Paused' }),
        makeTransfer({ uuid: 't2', state: 'ForcedActive', mode: 'Upload', primaryMode: 'Download' }),
        makeTransfer({ uuid: 't3', state: 'Completed', mode: 'Upload', primaryMode: 'Download' }),
      ]),
    );

    const icons = rows(fixture).map((r) => r.querySelector('.state-cell img') as HTMLImageElement);
    expect(icons[0].getAttribute('src')).toBe('img/states/paused.png');
    expect(icons[0].getAttribute('alt')).toBe('Paused');
    expect(icons[1].getAttribute('src')).toBe('img/states/forcedactive_upload.png');
    expect(icons[1].getAttribute('alt')).toBe('Forced active');
    // A download that finished and is now seeding still shows as a completed download.
    expect(icons[2].getAttribute('src')).toBe('img/states/completed.png');
  });

  it('right-clicking an unselected row replaces the selection with just that row', () => {
    const store = makeTransfersStoreStub([makeTransfer({ uuid: 't1' })]);
    const table = TestBed.configureTestingModule({
      imports: [TransferTable],
      providers: [
        { provide: TransfersStore, useValue: store },
        { provide: TransferActionsService, useValue: makeActionsStub() },
        { provide: PluginActions, useValue: { menuItems: () => of([]) } },
      ],
    }).createComponent(TransferTable);
    table.detectChanges();

    table.componentInstance['onContextMenuSelect']({ data: makeTransfer({ uuid: 't1' }), originalEvent: new Event('contextmenu'), index: 0 });
    expect(store.selectOnly).toHaveBeenCalledWith('t1');
  });

  it('right-clicking an already-selected row does not change the selection', () => {
    const store = makeTransfersStoreStub([makeTransfer({ uuid: 't1' })], ['t1']);
    const table = TestBed.configureTestingModule({
      imports: [TransferTable],
      providers: [
        { provide: TransfersStore, useValue: store },
        { provide: TransferActionsService, useValue: makeActionsStub() },
        { provide: PluginActions, useValue: { menuItems: () => of([]) } },
      ],
    }).createComponent(TransferTable);
    table.detectChanges();

    table.componentInstance['onContextMenuSelect']({ data: makeTransfer({ uuid: 't1' }), originalEvent: new Event('contextmenu'), index: 0 });
    expect(store.selectOnly).not.toHaveBeenCalled();
  });

  it('opening the context menu disables the items that do not apply to the selection', () => {
    const store = makeTransfersStoreStub(
      [makeTransfer({ uuid: 't1', state: 'Paused' }), makeTransfer({ uuid: 't2' })],
      ['t1', 't2'],
    );
    const table = createTable(store);
    table.componentInstance['onContextMenuSelect']({ data: makeTransfer({ uuid: 't1' }), originalEvent: new Event('contextmenu'), index: 0 });

    const items = table.componentInstance['contextMenuItems'] as { label?: string; disabled?: boolean }[];
    const disabled = (label: string) => items.find((i) => i.label === label)!.disabled;
    expect(disabled('Resume')).toBe(false); // t1 is Paused
    expect(disabled('Pause')).toBe(false); // t2 is Active
    expect(disabled('Move Up')).toBe(false); // t2 is not first
    expect(disabled('Properties')).toBe(false); // two selected: still editable
  });

  it('the context menu items call the corresponding TransferActionsService methods', () => {
    const actions = makeActionsStub();
    const table = TestBed.configureTestingModule({
      imports: [TransferTable],
      providers: [
        { provide: TransfersStore, useValue: makeTransfersStoreStub([]) },
        { provide: TransferActionsService, useValue: actions },
        { provide: PluginActions, useValue: { menuItems: () => of([]) } },
      ],
    }).createComponent(TransferTable);
    table.detectChanges();

    const items = table.componentInstance['contextMenuItems'] as { label?: string; icon?: string; separator?: boolean; command: () => void }[];
    // Every non-separator item carries an icon, like the legacy menu.
    expect(items.filter((i) => !i.separator).every((i) => i.icon?.startsWith('pi pi-'))).toBe(true);
    const propertiesRequested = vi.fn();
    table.componentInstance.propertiesRequested.subscribe(propertiesRequested);
    const byLabel = (label: string) => items.find((i) => i.label === label)!;

    byLabel('Resume').command();
    expect(actions.resume).toHaveBeenCalled();
    byLabel('Force Resume').command();
    expect(actions.forceResume).toHaveBeenCalled();
    byLabel('Pause').command();
    expect(actions.pause).toHaveBeenCalled();
    byLabel('Delete').command();
    expect(actions.deleteSelected).toHaveBeenCalledWith(false);
    byLabel('Delete with Data').command();
    expect(actions.deleteSelected).toHaveBeenCalledWith(true);
    byLabel('Move to Top').command();
    expect(actions.moveSelected).toHaveBeenCalledWith('top');
    byLabel('Move Up').command();
    expect(actions.moveSelected).toHaveBeenCalledWith('up');
    byLabel('Move Down').command();
    expect(actions.moveSelected).toHaveBeenCalledWith('down');
    byLabel('Properties').command();
    expect(propertiesRequested).toHaveBeenCalled();
    byLabel('Move to Bottom').command();
    expect(actions.moveSelected).toHaveBeenCalledWith('bottom');
  });

  describe('drag and drop reordering', () => {
    const three = () => [makeTransfer({ uuid: 'a' }), makeTransfer({ uuid: 'b' }), makeTransfer({ uuid: 'c' })];

    function drag(fixture: ReturnType<typeof createTable>, from: number, to: number, clientY: number): void {
      const r = rows(fixture);
      r[from].dispatchEvent(dragEvent('dragstart'));
      // happy-dom rects are all zero: clientY < 0 is the top half, > 0 the bottom half.
      r[to].dispatchEvent(dragEvent('dragover', clientY));
      r[to].dispatchEvent(dragEvent('drop', clientY));
    }

    it('drops on the top half of a row: moves in front of that row', () => {
      const { fixture, actions } = createTableWithActions(makeTransfersStoreStub(three(), ['c']));
      drag(fixture, 2, 0, -1);
      expect(actions.moveBefore).toHaveBeenCalledWith(['c'], 'a');
    });

    it('drops on the bottom half of a row: moves in front of the next row', () => {
      const { fixture, actions } = createTableWithActions(makeTransfersStoreStub(three(), ['a']));
      drag(fixture, 0, 1, 1);
      expect(actions.moveBefore).toHaveBeenCalledWith(['a'], 'c');
    });

    it('drops on the bottom half of the last row: moves to the end', () => {
      const { fixture, actions } = createTableWithActions(makeTransfersStoreStub(three(), ['a']));
      drag(fixture, 0, 2, 1);
      expect(actions.moveBefore).toHaveBeenCalledWith(['a'], null);
    });

    it('moves the whole selection when a selected row is dragged, skipping moved rows as the anchor', () => {
      const { fixture, actions } = createTableWithActions(makeTransfersStoreStub(three(), ['a', 'b']));
      drag(fixture, 0, 1, 1);
      expect(actions.moveBefore).toHaveBeenCalledWith(['a', 'b'], 'c');
    });

    it('selects an unselected row before dragging it', () => {
      const store = makeTransfersStoreStub(three(), ['a']);
      const { fixture } = createTableWithActions(store);
      rows(fixture)[1].dispatchEvent(dragEvent('dragstart'));
      expect(store.selectOnly).toHaveBeenCalledWith('b');
    });

    it('ignores drops that did not start on a row', () => {
      const { fixture, actions } = createTableWithActions(makeTransfersStoreStub(three()));
      rows(fixture)[0].dispatchEvent(dragEvent('drop'));
      expect(actions.moveBefore).not.toHaveBeenCalled();
    });
  });

  it('adds the plugin actions for a single selected transfer', () => {
    const store = makeTransfersStoreStub([makeTransfer({ uuid: 't1' })], ['t1']);
    const menuItems = vi.fn().mockReturnValue(of([{ separator: true }, { label: 'Unpack' }]));
    TestBed.configureTestingModule({
      imports: [TransferTable],
      providers: [
        { provide: TransfersStore, useValue: store },
        { provide: TransferActionsService, useValue: makeActionsStub() },
        { provide: PluginActions, useValue: { menuItems } },
      ],
    });
    const table = TestBed.createComponent(TransferTable);
    table.detectChanges();

    table.componentInstance['onContextMenuSelect']({ data: makeTransfer({ uuid: 't1' }), originalEvent: new Event('contextmenu'), index: 0 });

    expect(menuItems).toHaveBeenCalledWith('t1');
    const items = table.componentInstance['contextMenuItems'] as { label?: string }[];
    expect(items.at(-1)!.label).toBe('Unpack');
  });

  it('adds no plugin actions for a multiple selection', () => {
    const store = makeTransfersStoreStub([makeTransfer({ uuid: 't1' }), makeTransfer({ uuid: 't2' })], ['t1', 't2']);
    const menuItems = vi.fn().mockReturnValue(of([]));
    TestBed.configureTestingModule({
      imports: [TransferTable],
      providers: [
        { provide: TransfersStore, useValue: store },
        { provide: TransferActionsService, useValue: makeActionsStub() },
        { provide: PluginActions, useValue: { menuItems } },
      ],
    });
    const table = TestBed.createComponent(TransferTable);
    table.detectChanges();

    table.componentInstance['onContextMenuSelect']({ data: makeTransfer({ uuid: 't1' }), originalEvent: new Event('contextmenu'), index: 0 });
    expect(menuItems).not.toHaveBeenCalled();
  });
});
