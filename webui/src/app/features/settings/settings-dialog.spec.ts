import { ComponentFixture, TestBed } from '@angular/core/testing';
import { signal } from '@angular/core';
import { ViewportService } from '../../core/viewport.service';
import { of, Subject } from 'rxjs';
import { SettingsDialog } from './settings-dialog';
import { TransferApi } from '../../core/api/transfer-api';
import { SettingsDraftService } from '../../core/settings-draft.service';
import { SettingsPage } from '../../core/api/models';

function makePage(overrides: Partial<SettingsPage> = {}): SettingsPage {
  return { title: 'BitTorrent', script: '/scripts/settings/bittorrent.js', icon: '/img/settings/bittorrent.png', ...overrides };
}

function configure(
  getSettingsPages = vi.fn().mockReturnValue(of([makePage()])),
  setSettingsValues = vi.fn().mockReturnValue(of(undefined)),
  applySettings = vi.fn().mockReturnValue(of(undefined)),
) {
  TestBed.configureTestingModule({
    imports: [SettingsDialog],
    providers: [
      { provide: TransferApi, useValue: { getSettingsPages, setSettingsValues, applySettings, getSettingsValues: () => of([]) } },
    ],
  });
  return { getSettingsPages, setSettingsValues, applySettings };
}

/**
 * SettingsDraftService is provided on SettingsDialog itself (providers: [SettingsDraftService]),
 * so each dialog instance's own child injector holds a real instance a test can reach into
 * directly, instead of needing to mock it.
 */
function getDraft(fixture: ComponentFixture<SettingsDialog>): SettingsDraftService {
  return fixture.debugElement.injector.get(SettingsDraftService);
}

describe('SettingsDialog', () => {
  it('does not fetch pages while closed', () => {
    const { getSettingsPages } = configure();
    const fixture = TestBed.createComponent(SettingsDialog);
    fixture.componentRef.setInput('visible', false);
    fixture.detectChanges();

    expect(getSettingsPages).not.toHaveBeenCalled();
  });

  it('fetches pages and selects the first one when opened', () => {
    const page = makePage({ title: 'BitTorrent' });
    const { getSettingsPages } = configure(vi.fn().mockReturnValue(of([page, makePage({ title: 'Web Interface', script: '/scripts/settings/webinterface.js' })])));
    const fixture = TestBed.createComponent(SettingsDialog);
    fixture.componentRef.setInput('visible', true);
    fixture.detectChanges();

    expect(getSettingsPages).toHaveBeenCalled();
    expect(fixture.componentInstance['selectedPage']()).toEqual(page);
    expect(fixture.nativeElement.querySelector('app-bittorrent-settings')).toBeTruthy();
  });

  it('switches the rendered page component when a different page is selected', () => {
    const btPage = makePage();
    const wiPage = makePage({ title: 'Web Interface', script: '/scripts/settings/webinterface.js' });
    configure(vi.fn().mockReturnValue(of([btPage, wiPage])));
    const fixture = TestBed.createComponent(SettingsDialog);
    fixture.componentRef.setInput('visible', true);
    fixture.detectChanges();

    fixture.componentInstance['selectPage'](wiPage);
    fixture.detectChanges();

    expect(fixture.nativeElement.querySelector('app-webinterface-settings')).toBeTruthy();
    expect(fixture.nativeElement.querySelector('app-bittorrent-settings')).toBeNull();
  });

  it('OK batches the staged draft into one setSettingsValues call followed by applySettings, then closes', () => {
    const { setSettingsValues, applySettings } = configure();
    const fixture = TestBed.createComponent(SettingsDialog);
    fixture.componentRef.setInput('visible', true);
    fixture.detectChanges();

    // Simulate a page having staged an edit via the dialog's own SettingsDraftService instance.
    getDraft(fixture).stage('torrent/maxratio', 3);

    fixture.componentInstance['onOk']();

    expect(setSettingsValues).toHaveBeenCalledWith(['torrent/maxratio'], [3]);
    expect(applySettings).toHaveBeenCalled();
  });

  it('Cancel closes without calling setSettingsValues or applySettings', () => {
    const { setSettingsValues, applySettings } = configure();
    const fixture = TestBed.createComponent(SettingsDialog);
    fixture.componentRef.setInput('visible', true);
    fixture.detectChanges();

    fixture.componentInstance['onCancel']();

    expect(setSettingsValues).not.toHaveBeenCalled();
    expect(applySettings).not.toHaveBeenCalled();
  });

  it('clears the draft and refetches pages every time the dialog is reopened', () => {
    const { getSettingsPages } = configure();
    const fixture = TestBed.createComponent(SettingsDialog);
    fixture.componentRef.setInput('visible', true);
    fixture.detectChanges();
    expect(getSettingsPages).toHaveBeenCalledTimes(1);

    // Stage an edit, then close WITHOUT clicking OK (e.g. the user hit Cancel or the dialog's own
    // close affordance) — reopening must not resurrect it.
    getDraft(fixture).stage('torrent/maxratio', 3);

    fixture.componentRef.setInput('visible', false);
    fixture.detectChanges();
    fixture.componentRef.setInput('visible', true);
    fixture.detectChanges();

    expect(getSettingsPages).toHaveBeenCalledTimes(2);
    expect(getDraft(fixture).get('torrent/maxratio')).toBeUndefined();
  });

  it('ignores a stale getSettingsPages response from a prior, already-closed dialog session', () => {
    const firstPage = makePage({ title: 'Stale BitTorrent' });
    const secondPage = makePage({ title: 'Fresh Web Interface', script: '/scripts/settings/webinterface.js' });

    const firstResponse = new Subject<SettingsPage[]>();
    const secondResponse = new Subject<SettingsPage[]>();
    const getSettingsPages = vi.fn().mockReturnValueOnce(firstResponse).mockReturnValueOnce(secondResponse);
    configure(getSettingsPages);

    const fixture = TestBed.createComponent(SettingsDialog);

    // First open: request is issued but left pending.
    fixture.componentRef.setInput('visible', true);
    fixture.detectChanges();

    // Closed again before the first request resolves.
    fixture.componentRef.setInput('visible', false);
    fixture.detectChanges();

    // Reopened: a second, fresh request is issued.
    fixture.componentRef.setInput('visible', true);
    fixture.detectChanges();

    expect(getSettingsPages).toHaveBeenCalledTimes(2);

    // The second (fresher) session's response arrives first.
    secondResponse.next([secondPage]);
    secondResponse.complete();

    // The first (now-stale) session's response arrives afterwards and must be ignored.
    firstResponse.next([firstPage]);
    firstResponse.complete();

    expect(fixture.componentInstance['pages']()).toEqual([secondPage]);
    expect(fixture.componentInstance['selectedPage']()).toEqual(secondPage);
  });
});

describe('SettingsDialog (mobile)', () => {
  const btPage = makePage();
  const wiPage = makePage({ title: 'Web Interface', script: '/scripts/settings/webinterface.js' });

  function open() {
    configure(vi.fn().mockReturnValue(of([btPage, wiPage])));
    TestBed.overrideProvider(ViewportService, { useValue: { isMobile: signal(true) } });
    const fixture = TestBed.createComponent(SettingsDialog);
    fixture.componentRef.setInput('visible', true);
    fixture.detectChanges();
    return fixture;
  }

  const query = (fixture: ComponentFixture<SettingsDialog>, testId: string) =>
    fixture.nativeElement.querySelector(`[data-testid="${testId}"]`) as HTMLElement | null;

  it('starts on the page list without rendering a page', () => {
    const fixture = open();

    expect(fixture.nativeElement.querySelectorAll('[data-testid="settings-page-item"]')).toHaveLength(2);
    expect(fixture.nativeElement.querySelector('app-bittorrent-settings')).toBeNull();
    expect(query(fixture, 'settings-back')).toBeNull();
  });

  it('drills into a page and back to the list', () => {
    const fixture = open();

    (fixture.nativeElement.querySelectorAll('[data-testid="settings-page-item"]')[1] as HTMLElement).click();
    fixture.detectChanges();
    expect(fixture.nativeElement.querySelector('app-webinterface-settings')).toBeTruthy();
    expect(fixture.nativeElement.querySelector('[data-testid="settings-page-item"]')).toBeNull();
    expect(query(fixture, 'settings-back')!.textContent).toContain('Web Interface');

    query(fixture, 'settings-back')!.click();
    fixture.detectChanges();
    expect(fixture.nativeElement.querySelectorAll('[data-testid="settings-page-item"]')).toHaveLength(2);
  });

  it('returns to the page list each time the dialog is reopened', () => {
    const fixture = open();
    (fixture.nativeElement.querySelectorAll('[data-testid="settings-page-item"]')[0] as HTMLElement).click();
    fixture.componentRef.setInput('visible', false);
    fixture.detectChanges();

    fixture.componentRef.setInput('visible', true);
    fixture.detectChanges();

    expect(fixture.componentInstance['pageOpen']()).toBe(false);
  });
});
