import { ComponentFixture, TestBed } from '@angular/core/testing';
import { of } from 'rxjs';
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
});
