import { TestBed } from '@angular/core/testing';
import { of } from 'rxjs';
import { RssRegexpDialog, RssRegexpFormValue } from './rss-regexp-dialog';
import { RssApi } from '../../core/rss-api.service';
import { Queue, RssFeed, RssRegexp } from '../../core/api/models';

const feeds: RssFeed[] = [{ id: 'f1', name: 'Feed 1', url: 'http://1' }, { id: 'f2', name: 'Feed 2', url: 'http://2' }];
const queues = [
  { uuid: 'q1', name: 'One', defaultDirectory: '/one' },
  { uuid: 'q2', name: 'Two', defaultDirectory: '/two' },
] as Queue[];

function makeRegexp(overrides: Partial<RssRegexp> = {}): RssRegexp {
  return {
    id: 'r1', feedId: 'f2', expression: 'show', queue: 'q2', target: '/custom', addPaused: true, tvs: 'season',
    from: 'S01E01', to: 'S02E05', includeRepacks: false, includeTrailers: true, excludeManuals: false, linkRegexp: '',
    ...overrides,
  };
}

type Dialog = {
  feedId: string; queue: string; target: string; tvs: string; from: string; to: string; extractLinks: boolean; linkRegexp: string;
  testText: string; expression: string; valid: boolean; rangeValid: boolean; canManageDownloaded: boolean;
  onQueueChange(): void; onTvsChange(): void; onSave(): void; scheduleTest(): void; testState(): { kind: string };
};

function create(regexp: RssRegexp | null, api: Partial<RssApi> = {}) {
  TestBed.configureTestingModule({
    imports: [RssRegexpDialog],
    providers: [{ provide: RssApi, useValue: api }],
  });
  const fixture = TestBed.createComponent(RssRegexpDialog);
  fixture.componentRef.setInput('regexp', regexp);
  fixture.componentRef.setInput('feeds', feeds);
  fixture.componentRef.setInput('queues', queues);
  fixture.componentRef.setInput('visible', true);
  fixture.detectChanges();
  return { fixture, dialog: fixture.componentInstance as unknown as Dialog };
}

describe('RssRegexpDialog', () => {
  it('starts a new rule on the first feed and queue, in that queue\'s directory', () => {
    const { dialog } = create(null);
    expect(dialog.feedId).toBe('f1');
    expect(dialog.queue).toBe('q1');
    expect(dialog.target).toBe('/one');
    expect(dialog.tvs).toBe('none');
  });

  it('prefills an existing rule', () => {
    const { dialog } = create(makeRegexp());
    expect(dialog.feedId).toBe('f2');
    expect(dialog.queue).toBe('q2');
    expect(dialog.target).toBe('/custom');
    expect(dialog.from).toBe('S01E01');
  });

  it('moves the directory with the queue only while it is still the old queue\'s default', () => {
    const { dialog } = create(null);
    dialog.queue = 'q2';
    dialog.onQueueChange();
    expect(dialog.target).toBe('/two');

    dialog.target = '/elsewhere';
    dialog.queue = 'q1';
    dialog.onQueueChange();
    expect(dialog.target).toBe('/elsewhere');
  });

  it('resets the range to the widest one when the numbering changes', () => {
    const { dialog } = create(null);
    dialog.tvs = 'date';
    dialog.onTvsChange();
    expect([dialog.from, dialog.to]).toEqual(['0000-00-00', '9999-99-99']);
    dialog.tvs = 'none';
    dialog.onTvsChange();
    expect([dialog.from, dialog.to]).toEqual(['', '']);
  });

  it('refuses to save a range that does not fit the numbering', () => {
    const { dialog, fixture } = create(makeRegexp());
    const save = vi.fn();
    fixture.componentInstance.save.subscribe(save);

    dialog.from = '0001';
    expect(dialog.rangeValid).toBe(false);
    expect(dialog.valid).toBe(false);
    dialog.onSave();
    expect(save).not.toHaveBeenCalled();

    dialog.from = 'S01E02';
    expect(dialog.valid).toBe(true);
  });

  it('needs a link expression when links are extracted from the description', () => {
    const { dialog } = create(null);
    dialog.extractLinks = true;
    expect(dialog.valid).toBe(false);
    dialog.linkRegexp = 'http://\\S+';
    expect(dialog.valid).toBe(true);
  });

  it('emits the full form value, clearing the link expression when links are not extracted', () => {
    const { dialog, fixture } = create(makeRegexp({ linkRegexp: 'old' }));
    const save = vi.fn();
    fixture.componentInstance.save.subscribe(save);

    dialog.extractLinks = false;
    dialog.onSave();

    const value = save.mock.calls[0][0] as RssRegexpFormValue;
    expect(value).toEqual({
      feedId: 'f2', queue: 'q2', expression: 'show', target: '/custom', addPaused: true, tvs: 'season',
      from: 'S01E01', to: 'S02E05', includeRepacks: false, includeTrailers: true, excludeManuals: false, linkRegexp: '',
    });
  });

  it('can manage downloaded episodes only for a saved rule whose numbering is unchanged', () => {
    expect(create(null).dialog.canManageDownloaded).toBe(false);
    TestBed.resetTestingModule();

    const { dialog } = create(makeRegexp());
    expect(dialog.canManageDownloaded).toBe(true);
    dialog.tvs = 'date';
    expect(dialog.canManageDownloaded).toBe(false);
    TestBed.resetTestingModule();

    expect(create(makeRegexp({ tvs: 'none' })).dialog.canManageDownloaded).toBe(false);
  });

  it('asks the server to try the expression, after a pause', async () => {
    vi.useFakeTimers();
    try {
      const test = vi.fn().mockReturnValue(of({ matches: true, episode: 'S01E02' }));
      const { dialog } = create(makeRegexp(), { test });

      dialog.expression = 'SHOW';
      dialog.testText = 'Show S01E02';
      dialog.scheduleTest();
      dialog.scheduleTest();
      expect(test).not.toHaveBeenCalled();

      await vi.advanceTimersByTimeAsync(300);

      expect(test).toHaveBeenCalledTimes(1);
      expect(test).toHaveBeenCalledWith({
        expression: 'SHOW', text: 'Show S01E02', tvs: 'season', includeRepacks: false, includeTrailers: true,
      });
      expect(dialog.testState().kind).toBe('result');
    } finally {
      vi.useRealTimers();
    }
  });

  it('shows no verdict while the title is empty', async () => {
    vi.useFakeTimers();
    try {
      const test = vi.fn();
      const { dialog } = create(makeRegexp(), { test });
      dialog.testText = '';
      dialog.scheduleTest();
      await vi.advanceTimersByTimeAsync(300);
      expect(test).not.toHaveBeenCalled();
      expect(dialog.testState().kind).toBe('idle');
    } finally {
      vi.useRealTimers();
    }
  });
});
