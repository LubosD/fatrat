// webui/src/app/shared/hotkeys/hotkeys.ts

/**
 * One key that triggers a toolbar action. For letters, case in `key` means Shift ('n' vs 'N'); the
 * match uses the Shift state, not the produced case, so Caps Lock is ignored. Other character keys
 * are matched on the produced character (`event.key`), so punctuation follows the user's layout.
 * Named keys such as 'Delete' produce the same `key` with or without Shift, so they set `shift`.
 */
export interface Hotkey {
  key: string;
  shift?: boolean;
  /** As shown in the badge and tooltip, e.g. 'N', 'Shift+D', 'Del'. */
  label: string;
  /** Whether holding the key down keeps triggering it (only the move keys). */
  repeat?: boolean;
}

export function matchesHotkey(event: KeyboardEvent, hotkey: Hotkey): boolean {
  if (/^[a-zA-Z]$/.test(hotkey.key)) {
    return event.key.toLowerCase() === hotkey.key.toLowerCase() && event.shiftKey === (hotkey.key !== hotkey.key.toLowerCase());
  }
  if (event.key !== hotkey.key) return false;
  return hotkey.shift === undefined || hotkey.shift === event.shiftKey;
}

/**
 * Ctrl/Meta/Alt combinations belong to the browser, the OS and the table's own Ctrl+A. AltGr is not
 * one: Windows reports it as Ctrl+Alt, and layouts such as Czech and German type [ ] { } with it.
 */
export function hasCommandModifier(event: KeyboardEvent): boolean {
  // The Ctrl+Alt check keeps a lone Alt from reading as AltGr in engines (happy-dom) that alias the two.
  if (event.ctrlKey && event.altKey && event.getModifierState?.('AltGraph')) return false;
  return event.ctrlKey || event.metaKey || event.altKey;
}

// Comboboxes and listboxes count as typing: Optimus UI's Select picks options by typed letters.
const TYPING_SELECTOR =
  'input, textarea, select, [contenteditable]:not([contenteditable="false"]), [role="combobox"], [role="listbox"]';

export function isTypingTarget(target: EventTarget | null): boolean {
  return target instanceof Element && target.closest(TYPING_SELECTOR) !== null;
}

// Optimus UI only renders these while open: modal dialog/drawer masks, popup and context menus,
// and the dropdown of a Select.
const OVERLAY_SELECTOR = '.p-overlay-mask, .p-menu-overlay, .p-select-overlay, .p-contextmenu';

export function isOverlayOpen(doc: Document): boolean {
  return doc.querySelector(OVERLAY_SELECTOR) !== null;
}

/** The value of `aria-keyshortcuts`: alternatives separated by spaces, ARIA key names. */
export function ariaKeyShortcuts(hotkeys: Hotkey[] | undefined): string | null {
  if (!hotkeys?.length) return null;
  return hotkeys
    .map((h) => {
      if (h.key.length > 1) return (h.shift ? 'Shift+' : '') + h.key;
      const upper = h.key.toUpperCase();
      if (upper === h.key.toLowerCase()) return h.key; // punctuation or digit
      return h.key === upper ? `Shift+${upper}` : upper;
    })
    .join(' ');
}
