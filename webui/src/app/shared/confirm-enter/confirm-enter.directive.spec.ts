import { Component } from '@angular/core';
import { TestBed } from '@angular/core/testing';
import { ConfirmationService } from 'primeng/api';
import { ConfirmDialog } from 'primeng/confirmdialog';
import { ConfirmEnter } from './confirm-enter.directive';

@Component({
  imports: [ConfirmDialog, ConfirmEnter],
  template: '<p-confirmdialog appConfirmEnter></p-confirmdialog>',
})
class Host {}

describe('ConfirmEnter', () => {
  function press(key: string, init: KeyboardEventInit = {}, target: EventTarget = document.body): KeyboardEvent {
    const event = new KeyboardEvent('keydown', { key, bubbles: true, cancelable: true, ...init });
    target.dispatchEvent(event);
    return event;
  }

  async function open(message = 'Delete 1 transfer?') {
    const fixture = TestBed.createComponent(Host);
    fixture.detectChanges();
    return { fixture, ...(await confirm(fixture, message)) };
  }

  async function confirm(fixture: { detectChanges(): void; whenStable(): Promise<unknown> }, message: string) {
    const accept = vi.fn();
    TestBed.inject(ConfirmationService).confirm({ header: 'Confirm', message, accept });
    fixture.detectChanges();
    await fixture.whenStable();
    fixture.detectChanges();
    return { accept };
  }

  const rejectButton = () => document.querySelector<HTMLButtonElement>('.p-confirmdialog-reject-button')!;
  const acceptButton = () => document.querySelector<HTMLButtonElement>('.p-confirmdialog-accept-button')!;
  const focusin = (el: HTMLElement) => {
    el.focus();
    el.dispatchEvent(new FocusEvent('focusin', { bubbles: true }));
  };

  beforeEach(() => TestBed.configureTestingModule({ providers: [ConfirmationService] }));

  afterEach(() => document.querySelectorAll('.p-overlay-mask').forEach((el) => el.remove()));

  it('Enter accepts it', async () => {
    const { accept } = await open();
    const event = press('Enter');
    expect(accept).toHaveBeenCalledTimes(1);
    expect(event.defaultPrevented).toBe(true);
  });

  it('Enter accepts it once even when the accept button has focus', async () => {
    const { accept } = await open();
    acceptButton().focus();
    press('Enter', {}, acceptButton());
    expect(accept).toHaveBeenCalledTimes(1);
  });

  it('Enter on the reject button is left to the button once the user tabbed there', async () => {
    const { accept } = await open();
    press('Tab', {}, rejectButton());
    const event = press('Enter', {}, rejectButton());
    expect(accept).not.toHaveBeenCalled();
    expect(event.defaultPrevented).toBe(false);
  });

  it('Enter accepts even when PrimeNG put the initial focus on the reject button', async () => {
    const { accept } = await open();
    const event = press('Enter', {}, rejectButton());
    expect(accept).toHaveBeenCalledTimes(1);
    expect(event.defaultPrevented).toBe(true);
  });

  it('a Tab in one confirmation does not carry over to the next', async () => {
    const { accept, fixture } = await open();
    press('Tab');
    press('Enter');
    expect(accept).toHaveBeenCalledTimes(1);
    fixture.detectChanges();
    const { accept: second } = await confirm(fixture, 'Again?');
    press('Enter', {}, rejectButton());
    expect(second).toHaveBeenCalledTimes(1);
  });

  it('ignores a held-down Enter', async () => {
    const { accept } = await open();
    press('Enter', { repeat: true });
    expect(accept).not.toHaveBeenCalled();
  });

  it('ignores Enter with Ctrl held', async () => {
    const { accept } = await open();
    press('Enter', { ctrlKey: true });
    expect(accept).not.toHaveBeenCalled();
  });

  it('does not accept on the Enter that opened the dialog', async () => {
    // Registered before the directive's listener, so it runs first and opens the dialog mid-event.
    let opened: (() => void) | undefined;
    const opener = (e: KeyboardEvent) => {
      if (e.key !== 'Enter' || opened) return;
      opened = vi.fn();
      TestBed.inject(ConfirmationService).confirm({ header: 'Confirm', message: 'Sure?', accept: opened });
    };
    document.addEventListener('keydown', opener);
    try {
      const fixture = TestBed.createComponent(Host);
      fixture.detectChanges();
      press('Enter');
      fixture.detectChanges();
      await fixture.whenStable();
      expect(opened).toBeDefined();
      expect(opened).not.toHaveBeenCalled();
      // The next, separate Enter does accept.
      press('Enter');
      expect(opened).toHaveBeenCalledTimes(1);
    } finally {
      document.removeEventListener('keydown', opener);
    }
  });

  it('moves focus from the reject button to the accept button', async () => {
    await open();
    focusin(rejectButton());
    expect(document.activeElement).toBe(acceptButton());
  });

  it('leaves focus on the reject button after the user pressed Tab', async () => {
    await open();
    press('Tab', {}, rejectButton());
    focusin(rejectButton());
    expect(document.activeElement).toBe(rejectButton());
  });

  it('does nothing while no confirmation is open', () => {
    const fixture = TestBed.createComponent(Host);
    fixture.detectChanges();
    expect(press('Enter').defaultPrevented).toBe(false);
  });
});
