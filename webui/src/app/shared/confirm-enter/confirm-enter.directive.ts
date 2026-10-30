import { DestroyRef, Directive, DOCUMENT, inject, OnInit } from '@angular/core';
import { ConfirmDialog } from '@openng/optimus-ui/confirmdialog';
import { hasCommandModifier } from '../hotkeys/hotkeys';

/**
 * Makes Enter accept the app's shared confirmation, wherever focus landed, so a hotkey delete is
 * `D` then Enter. Put it on the `<p-confirmDialog>`.
 *
 * Optimus UI lands the initial focus on Reject (the first footer button; its `defaultFocus` selector
 * matches nothing). So focus is moved to Accept on open, and a focused non-accept button only
 * keeps its own Enter once the user moved focus there themselves with Tab.
 */
@Directive({ selector: 'p-confirmDialog[appConfirmEnter], p-confirmdialog[appConfirmEnter]' })
export class ConfirmEnter implements OnInit {
  private readonly dialog = inject(ConfirmDialog);
  private readonly document = inject(DOCUMENT);
  private readonly destroyRef = inject(DestroyRef);

  /** The confirmation during which the user pressed Tab; a new confirmation is a different object. */
  private tabbedIn: unknown = null;
  /** Whether the dialog was already open when the current keydown began. */
  private visibleAtKeydownStart = false;

  ngOnInit(): void {
    this.listen('keydown', () => (this.visibleAtKeydownStart = !!this.dialog.visible), true);
    this.listen('keydown', (e) => this.onKeydown(e as KeyboardEvent));
    this.listen('focusin', (e) => this.onFocusin(e as FocusEvent));
  }

  private listen(type: string, handler: (event: Event) => void, capture = false): void {
    this.document.addEventListener(type, handler, capture);
    this.destroyRef.onDestroy(() => this.document.removeEventListener(type, handler, capture));
  }

  private onKeydown(event: KeyboardEvent): void {
    if (!this.dialog.visible) {
      this.tabbedIn = null;
      return;
    }
    if (event.key === 'Tab') {
      this.tabbedIn = this.dialog.confirmation ?? null;
      return;
    }
    if (event.key !== 'Enter' || hasCommandModifier(event)) return;
    // An Enter that itself opened the dialog (another listener, or a button's Enter-click) must
    // not also accept it.
    if (!this.visibleAtKeydownStart) return;
    const button = event.target instanceof Element ? event.target.closest('button') : null;
    if (this.movedByUser() && button && !button.classList.contains('p-confirmdialog-accept-button')) return;
    // Also stops the focused accept button's own Enter-click, which would accept a second time.
    event.preventDefault();
    if (!event.repeat) this.dialog.onAccept();
  }

  private onFocusin(event: FocusEvent): void {
    if (!this.dialog.visible || !(event.target instanceof Element)) return;
    if (!event.target.classList.contains('p-confirmdialog-reject-button') || this.movedByUser()) return;
    event.target.closest('.p-confirmdialog')?.querySelector<HTMLElement>('.p-confirmdialog-accept-button')?.focus();
  }

  private movedByUser(): boolean {
    return this.tabbedIn !== null && this.tabbedIn === this.dialog.confirmation;
  }
}
