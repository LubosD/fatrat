// webui/src/app/shell/hotkeys.spec.ts
import { ariaKeyShortcuts, hasCommandModifier, isOverlayOpen, isTypingTarget, matchesHotkey } from './hotkeys';

function key(init: KeyboardEventInit): KeyboardEvent {
  return new KeyboardEvent('keydown', init);
}

describe('hotkeys', () => {
  afterEach(() => (document.body.innerHTML = ''));

  describe('matchesHotkey', () => {
    it('matches character keys case-sensitively, so Shift is encoded in the character', () => {
      expect(matchesHotkey(key({ key: 'n' }), { key: 'n', label: 'N' })).toBe(true);
      expect(matchesHotkey(key({ key: 'N', shiftKey: true }), { key: 'n', label: 'N' })).toBe(false);
      expect(matchesHotkey(key({ key: 'N', shiftKey: true }), { key: 'N', label: 'Shift+N' })).toBe(true);
    });

    it('ignores Caps Lock for letters, deciding by Shift', () => {
      expect(matchesHotkey(key({ key: 'N' }), { key: 'n', label: 'N' })).toBe(true);
      expect(matchesHotkey(key({ key: 'N' }), { key: 'N', label: 'Shift+N' })).toBe(false);
      expect(matchesHotkey(key({ key: 'n', shiftKey: true }), { key: 'N', label: 'Shift+N' })).toBe(true);
      expect(matchesHotkey(key({ key: 'N', shiftKey: true }), { key: 'N', label: 'Shift+N' })).toBe(true);
    });

    it('checks Shift explicitly for named keys such as Delete', () => {
      const plain = { key: 'Delete', shift: false, label: 'Del' };
      const shifted = { key: 'Delete', shift: true, label: 'Shift+Del' };
      expect(matchesHotkey(key({ key: 'Delete' }), plain)).toBe(true);
      expect(matchesHotkey(key({ key: 'Delete' }), shifted)).toBe(false);
      expect(matchesHotkey(key({ key: 'Delete', shiftKey: true }), shifted)).toBe(true);
      expect(matchesHotkey(key({ key: 'Delete', shiftKey: true }), plain)).toBe(false);
    });
  });

  it('hasCommandModifier is false for AltGr, which Windows reports as Ctrl+Alt', () => {
    const event = key({ key: '[', ctrlKey: true, altKey: true });
    event.getModifierState = (name: string) => name === 'AltGraph';
    expect(hasCommandModifier(event)).toBe(false);
  });

  it('hasCommandModifier is true for Ctrl, Meta and Alt but not Shift', () => {
    expect(hasCommandModifier(key({ key: 'n', ctrlKey: true }))).toBe(true);
    expect(hasCommandModifier(key({ key: 'n', metaKey: true }))).toBe(true);
    expect(hasCommandModifier(key({ key: 'n', altKey: true }))).toBe(true);
    expect(hasCommandModifier(key({ key: 'N', shiftKey: true }))).toBe(false);
  });

  it('isTypingTarget recognises text fields, selects, editable content and comboboxes', () => {
    document.body.innerHTML = `
      <input id="i"><textarea id="t"></textarea><select id="s"></select>
      <div contenteditable="true"><span id="ce">x</span></div>
      <span role="combobox" id="cb"></span><button id="b"></button>`;
    for (const id of ['i', 't', 's', 'ce', 'cb']) {
      expect(isTypingTarget(document.getElementById(id))).toBe(true);
    }
    expect(isTypingTarget(document.getElementById('b'))).toBe(false);
    expect(isTypingTarget(document)).toBe(false);
    expect(isTypingTarget(null)).toBe(false);
  });

  it('isOverlayOpen detects modal masks, popup menus and select overlays', () => {
    expect(isOverlayOpen(document)).toBe(false);
    for (const cls of ['p-overlay-mask', 'p-menu-overlay', 'p-select-overlay', 'p-contextmenu']) {
      document.body.innerHTML = `<div class="${cls}"></div>`;
      expect(isOverlayOpen(document)).toBe(true);
    }
  });

  it('ariaKeyShortcuts uses ARIA key names, space-separated', () => {
    expect(ariaKeyShortcuts([{ key: 'n', label: 'N' }])).toBe('N');
    expect(ariaKeyShortcuts([{ key: 'N', label: 'Shift+N' }])).toBe('Shift+N');
    expect(ariaKeyShortcuts([{ key: '[', label: '[' }])).toBe('[');
    expect(ariaKeyShortcuts([{ key: 'd', label: 'D' }, { key: 'Delete', shift: false, label: 'Del' }])).toBe('D Delete');
    expect(ariaKeyShortcuts([{ key: 'Delete', shift: true, label: 'Shift+Del' }])).toBe('Shift+Delete');
    expect(ariaKeyShortcuts(undefined)).toBeNull();
  });
});
