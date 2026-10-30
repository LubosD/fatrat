import { TestBed } from '@angular/core/testing';
import { of, throwError } from 'rxjs';
import { signal } from '@angular/core';
import { ConfirmationService } from '@openng/optimus-ui/api';
import { RssSettings } from './rss-settings';
import { TransferApi } from '../../core/api/transfer-api';
import { RssApi } from '../../core/rss-api.service';
import { ApiError } from '../../core/api/rest-client';
import { SettingsDraftService } from '../../core/settings-draft.service';
import { QueuesStore } from '../../state/queues.store';
import { ViewportService } from '../../core/viewport.service';
import { Queue, RssFeed, RssRegexp } from '../../core/api/models';

const feed: RssFeed = { id: 'f1', name: 'Feed', url: 'http://f' };
const regexp = {
  id: 'r1', feedId: 'f1', expression: 'show', queue: 'q1', tvs: 'season',
} as RssRegexp;
const queue = { uuid: 'q1', name: 'Main', defaultDirectory: '/d' } as Queue;

function configure(opts: { mobile?: boolean; feeds?: RssFeed[]; regexps?: RssRegexp[]; queues?: Queue[]; api?: Partial<RssApi>; draft?: Record<string, unknown> } = {}) {
  const stage = vi.fn();
  const confirm = vi.fn();
  const api = {
    listFeeds: vi.fn().mockReturnValue(of(opts.feeds ?? [feed])),
    listRegexps: vi.fn().mockReturnValue(of(opts.regexps ?? [regexp])),
    ...opts.api,
  };
  TestBed.configureTestingModule({
    imports: [RssSettings],
    providers: [
      { provide: TransferApi, useValue: { getSettingsValues: vi.fn().mockReturnValue(of([true, 30])) } },
      { provide: RssApi, useValue: api },
      { provide: SettingsDraftService, useValue: { get: (k: string) => opts.draft?.[k], stage } },
      { provide: QueuesStore, useValue: { queues: signal(opts.queues ?? [queue]) } },
      { provide: ConfirmationService, useValue: { confirm } },
      { provide: ViewportService, useValue: { isMobile: signal(opts.mobile ?? false) } },
    ],
  });
  const fixture = TestBed.createComponent(RssSettings);
  fixture.detectChanges();
  return { fixture, c: fixture.componentInstance as unknown as Record<string, any>, api, stage, confirm };
}

describe('RssSettings', () => {
  it('loads the enable switch, the interval, the feeds and the rules', () => {
    const { c, api } = configure();
    expect(c['enable']).toBe(true);
    expect(c['interval']).toBe(30);
    expect(api.listFeeds).toHaveBeenCalled();
    expect(c['feeds']()).toEqual([feed]);
    expect(c['regexps']()).toEqual([regexp]);
  });

  it('prefers a staged value over the stored one', () => {
    const { c } = configure({ draft: { 'rss/interval': 5 } });
    expect(c['interval']).toBe(5);
  });

  it('stages the two options under their keys', () => {
    const { c, stage } = configure();
    c['stage']('rss/enable', false);
    expect(stage).toHaveBeenCalledWith('rss/enable', false);
  });

  it('labels a rule like the desktop UI: expression @ feed', () => {
    const { c } = configure();
    expect(c['regexpLabel'](regexp)).toBe('show @ Feed');
    expect(c['regexpLabel']({ ...regexp, expression: '' })).toBe('Feed');
  });

  it('cannot add a rule without a feed or without a queue', () => {
    expect(configure({ feeds: [] }).c['canAddRegexp']()).toBe(false);
    TestBed.resetTestingModule();
    const { c } = configure({ queues: [] });
    expect(c['canAddRegexp']()).toBe(false);
    c['addRegexp']();
    expect(c['regexpDialogOpen']()).toBe(false);
    TestBed.resetTestingModule();
    const ok = configure();
    ok.c['addRegexp']();
    expect(ok.c['regexpDialogOpen']()).toBe(true);
  });

  it('creates a feed, closes the dialog and reloads', () => {
    const createFeed = vi.fn().mockReturnValue(of(feed));
    const { c, api } = configure({ api: { createFeed } });
    c['addFeed']();
    c['saveFeed']({ name: 'N', url: 'http://n' });
    expect(createFeed).toHaveBeenCalledWith({ name: 'N', url: 'http://n' });
    expect(c['feedDialogOpen']()).toBe(false);
    expect(api.listFeeds).toHaveBeenCalledTimes(2);
  });

  it('updates the edited feed and keeps the dialog open with the error if the server refuses', () => {
    const updateFeed = vi.fn().mockReturnValue(throwError(() => new ApiError(409, 'duplicate_feed', 'A feed with this URL already exists')));
    const { c } = configure({ api: { updateFeed } });
    c['editFeed'](feed);
    c['saveFeed']({ name: 'N', url: 'http://dup' });
    expect(updateFeed).toHaveBeenCalledWith('f1', { name: 'N', url: 'http://dup' });
    expect(c['feedDialogOpen']()).toBe(true);
    expect(c['feedError']()).toBe('A feed with this URL already exists');
  });

  it('warns that deleting a feed deletes its rules, and deletes only after confirmation', () => {
    const deleteFeed = vi.fn().mockReturnValue(of(undefined));
    const { c, confirm, api } = configure({ api: { deleteFeed } });
    c['deleteFeed'](feed);
    expect(confirm.mock.calls[0][0].message).toContain('1 rule');
    expect(deleteFeed).not.toHaveBeenCalled();

    confirm.mock.calls[0][0].accept();
    expect(deleteFeed).toHaveBeenCalledWith('f1');
    expect(api.listRegexps).toHaveBeenCalledTimes(2);
  });

  it('creates a rule with every form field and updates the edited one', () => {
    const createRegexp = vi.fn().mockReturnValue(of(regexp));
    const updateRegexp = vi.fn().mockReturnValue(of(regexp));
    const { c } = configure({ api: { createRegexp, updateRegexp } });
    const value = { feedId: 'f1', queue: 'q1', expression: 'x' };

    c['addRegexp']();
    c['saveRegexp'](value);
    expect(createRegexp).toHaveBeenCalledWith(value);
    expect(c['regexpDialogOpen']()).toBe(false);

    c['editRegexp'](regexp);
    c['saveRegexp'](value);
    expect(updateRegexp).toHaveBeenCalledWith('r1', value);
  });

  it('keeps the rule dialog open with the error when the server rejects the expression', () => {
    const createRegexp = vi.fn().mockReturnValue(throwError(() => new ApiError(422, 'invalid_property', 'not a valid regular expression')));
    const { c } = configure({ api: { createRegexp } });
    c['addRegexp']();
    c['saveRegexp']({ feedId: 'f1', queue: 'q1', expression: '(' });
    expect(c['regexpDialogOpen']()).toBe(true);
    expect(c['regexpError']()).toBe('not a valid regular expression');
  });

  it('asks the server to fetch now and says so', () => {
    const refresh = vi.fn().mockReturnValue(of(undefined));
    const { c } = configure({ api: { refresh } });
    c['updateNow']();
    expect(refresh).toHaveBeenCalled();
    expect(c['message']()).toContain('Updating');
  });

  it('opens the downloaded-episodes dialog for the chosen rule', () => {
    const { c } = configure();
    c['manageDownloaded'](regexp);
    expect(c['downloadedRegexp']()).toBe(regexp);
    expect(c['downloadedDialogOpen']()).toBe(true);
  });

  it('lists feeds and rules in a table on desktop', () => {
    const { fixture } = configure();
    const el = fixture.nativeElement as HTMLElement;
    expect(el.querySelector('[data-testid="rss-feeds-table"]')).not.toBeNull();
    expect(el.querySelector('[data-testid="rss-regexps-table"]')).not.toBeNull();
    expect(el.querySelector('[data-testid="rss-feeds-list"]')).toBeNull();
  });

  it('lists feeds and rules as stacked rows on a phone, with the same actions', () => {
    const { fixture, c, confirm } = configure({ mobile: true });
    const el = fixture.nativeElement as HTMLElement;

    expect(el.querySelector('[data-testid="rss-feeds-table"]')).toBeNull();
    expect(el.querySelector('[data-testid="rss-feeds-list"]')?.textContent).toContain('http://f');
    expect(el.querySelector('[data-testid="rss-regexps-list"]')?.textContent).toContain('show @ Feed');
    expect(el.querySelector('[data-testid="rss-regexps-list"]')?.textContent).toContain('Main');

    (el.querySelector('[data-testid="rss-feed-edit"]') as HTMLButtonElement).click();
    expect(c['feedDialogOpen']()).toBe(true);
    (el.querySelector('[data-testid="rss-regexp-delete"]') as HTMLButtonElement).click();
    expect(confirm).toHaveBeenCalled();
  });

  it('says so when there is nothing to list on a phone', () => {
    const { fixture } = configure({ mobile: true, feeds: [], regexps: [] });
    const text = (fixture.nativeElement as HTMLElement).textContent;
    expect(text).toContain('No feeds yet.');
    expect(text).toContain('No rules yet.');
  });
});
