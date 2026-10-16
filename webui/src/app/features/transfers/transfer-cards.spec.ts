import { TestBed } from '@angular/core/testing';
import { signal } from '@angular/core';
import { TransferCards, LONG_PRESS_MS } from './transfer-cards';
import { TransfersStore } from '../../state/transfers.store';
import { TransferActionsService } from '../../core/transfer-actions.service';
import { Transfer } from '../../core/api/models';

function makeTransfer(overrides: Partial<Transfer> = {}): Transfer {
  return {
    uuid: 't1',
    name: 'file.zip',
    state: 'Active',
    class: 'HttpDownload',
    message: '',
    mode: 'Download',
    primaryMode: 'Download',
    dataPath: '/downloads/file.zip',
    dataPathIsDir: false,
    total: 2048,
    done: 1024,
    comment: '',
    object: '',
    timeRunning: 10,
    speeds: { down: 512, up: 0 },
    userSpeedLimits: { down: 0, up: 0 },
    ...overrides,
  };
}

function makeStore(transfers: Transfer[]) {
  const selected = signal<string[]>([]);
  return {
    transfers: () => transfers,
    selectedUuids: selected,
    isSelected: (uuid: string) => selected().includes(uuid),
    selectOnly: vi.fn((uuid: string) => selected.set([uuid])),
    toggleSelection: vi.fn((uuid: string) =>
      selected.update((s) => (s.includes(uuid) ? s.filter((u) => u !== uuid) : [...s, uuid])),
    ),
    clearSelection: vi.fn(() => selected.set([])),
    selectAll: vi.fn(() => selected.set(transfers.map((t) => t.uuid))),
  };
}

function setup(transfers: Transfer[]) {
  vi.useFakeTimers();
  const store = makeStore(transfers);
  const actions = { pause: vi.fn(), resume: vi.fn(), forceResume: vi.fn(), deleteSelected: vi.fn(), moveSelected: vi.fn() };
  TestBed.configureTestingModule({
    imports: [TransferCards],
    providers: [
      { provide: TransfersStore, useValue: store },
      { provide: TransferActionsService, useValue: actions },
    ],
  });
  const fixture = TestBed.createComponent(TransferCards);
  fixture.detectChanges();
  const el: HTMLElement = fixture.nativeElement;
  const cards = () => Array.from(el.querySelectorAll<HTMLElement>('[data-testid="transfer-card"]'));
  const pointer = (target: Element, type: string, x = 0, y = 0) =>
    target.dispatchEvent(new MouseEvent(type, { bubbles: true, clientX: x, clientY: y }));
  const longPress = (card: HTMLElement) => {
    pointer(card, 'pointerdown');
    vi.advanceTimersByTime(LONG_PRESS_MS);
    pointer(card, 'pointerup');
    card.click(); // the click a real finger-lift produces
    fixture.detectChanges();
  };
  return { fixture, el, store, actions, cards, pointer, longPress };
}

describe('TransferCards', () => {
  afterEach(() => vi.useRealTimers());

  const two = [makeTransfer({ uuid: 'a', name: 'alpha.iso', message: 'Connecting' }), makeTransfer({ uuid: 'b', name: 'beta.iso' })];

  it('renders a card per transfer with name, summary stats and message', () => {
    const { cards } = setup(two);

    expect(cards()).toHaveLength(2);
    expect(cards()[0].textContent).toContain('alpha.iso');
    expect(cards()[0].textContent).toContain('2.0 KB');
    expect(cards()[0].textContent).toContain('512 B/s');
    expect(cards()[0].textContent).toContain('Connecting');
    expect(cards()[1].textContent).not.toContain('Connecting');
  });

  it('shows an empty state without transfers', () => {
    const { el } = setup([]);
    expect(el.textContent).toContain('No transfers');
  });

  it('selects only the tapped card', () => {
    const { cards, store } = setup(two);
    cards()[1].click();
    expect(store.selectOnly).toHaveBeenCalledWith('b');
  });

  it('enters selection mode on a long press and does not also treat the lift as a tap', () => {
    const { cards, store, el, longPress } = setup(two);

    longPress(cards()[0]);

    expect(store.toggleSelection).toHaveBeenCalledWith('a');
    expect(store.selectOnly).not.toHaveBeenCalled();
    expect(el.querySelector('[data-testid="selection-bar"]')).toBeTruthy();
    expect(el.querySelector('[data-testid="selection-count"]')!.textContent).toContain('1 selected');
    expect(el.querySelectorAll('.card-check')).toHaveLength(2);
  });

  it('does not enter selection mode when released before the long-press delay', () => {
    const { cards, el, pointer, fixture } = setup(two);

    pointer(cards()[0], 'pointerdown');
    vi.advanceTimersByTime(LONG_PRESS_MS - 1);
    pointer(cards()[0], 'pointerup');
    vi.advanceTimersByTime(LONG_PRESS_MS);
    fixture.detectChanges();

    expect(el.querySelector('[data-testid="selection-bar"]')).toBeNull();
  });

  it('cancels the long press when the finger moves (scrolling)', () => {
    const { cards, el, pointer, fixture, store } = setup(two);

    pointer(cards()[0], 'pointerdown', 10, 10);
    pointer(cards()[0], 'pointermove', 10, 40);
    vi.advanceTimersByTime(LONG_PRESS_MS);
    fixture.detectChanges();

    expect(store.toggleSelection).not.toHaveBeenCalled();
    expect(el.querySelector('[data-testid="selection-bar"]')).toBeNull();
  });

  it('toggles cards by tap in selection mode and leaves it once nothing is selected', () => {
    const { cards, el, store, fixture, longPress } = setup(two);
    longPress(cards()[0]);

    cards()[1].click();
    fixture.detectChanges();
    expect(store.toggleSelection).toHaveBeenLastCalledWith('b');
    expect(el.querySelector('[data-testid="selection-count"]')!.textContent).toContain('2 selected');

    cards()[0].click();
    cards()[1].click();
    fixture.detectChanges();
    expect(el.querySelector('[data-testid="selection-bar"]')).toBeNull();
  });

  it('selects all, and closing selection mode clears the selection', () => {
    const { cards, el, store, fixture, longPress } = setup(two);
    longPress(cards()[0]);

    (el.querySelector('[data-testid="selection-all"]') as HTMLElement).click();
    fixture.detectChanges();
    expect(store.selectAll).toHaveBeenCalled();
    expect(el.querySelector('[data-testid="selection-count"]')!.textContent).toContain('2 selected');

    (el.querySelector('[data-testid="selection-close"]') as HTMLElement).click();
    fixture.detectChanges();
    expect(store.clearSelection).toHaveBeenCalled();
    expect(el.querySelector('[data-testid="selection-bar"]')).toBeNull();
  });

  it('opens the same actions menu as the table for the card, selecting it first, without counting as a tap', () => {
    const { cards, fixture, store } = setup(two);
    const comp = fixture.componentInstance as unknown as { menuItems: () => { label?: string; disabled?: boolean; command?: () => void }[]; propertiesRequested: { subscribe: (f: () => void) => void } };

    (cards()[1].querySelector('[data-testid="card-menu-button"]') as HTMLElement).click();
    fixture.detectChanges();

    expect(store.selectOnly).toHaveBeenCalledTimes(1);
    expect(store.selectOnly).toHaveBeenCalledWith('b');
    const labels = comp.menuItems().filter((i) => i.label).map((i) => i.label);
    expect(labels).toEqual([
      'Resume', 'Force Resume', 'Pause', 'Delete', 'Delete with Data',
      'Move to Top', 'Move Up', 'Move Down', 'Move to Bottom', 'Properties',
    ]);
    // b is last, so it can't move down; it is Active, so it can pause.
    const byLabel = (l: string) => comp.menuItems().find((i) => i.label === l)!;
    expect(byLabel('Move Down').disabled).toBe(true);
    expect(byLabel('Pause').disabled).toBe(false);
  });

  it('runs menu commands on the selection and emits propertiesRequested', () => {
    const { cards, fixture, actions } = setup(two);
    const spy = vi.fn();
    fixture.componentInstance.propertiesRequested.subscribe(spy);

    (cards()[0].querySelector('[data-testid="card-menu-button"]') as HTMLElement).click();
    const items = (fixture.componentInstance as unknown as { menuItems: () => { label?: string; command?: () => void }[] }).menuItems();
    items.find((i) => i.label === 'Pause')!.command!();
    items.find((i) => i.label === 'Properties')!.command!();

    expect(actions.pause).toHaveBeenCalled();
    expect(spy).toHaveBeenCalled();
  });
});
