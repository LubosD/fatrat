import { TestBed } from '@angular/core/testing';
import { of, throwError } from 'rxjs';
import { RssDownloadedDialog } from './rss-downloaded-dialog';
import { RssApi } from '../../core/rss-api.service';
import { ApiError } from '../../core/api/rest-client';
import { RssRegexp } from '../../core/api/models';

const regexp = { id: 'r1', tvs: 'season' } as RssRegexp;

function create(api: Partial<RssApi>, re: RssRegexp = regexp) {
  TestBed.configureTestingModule({
    imports: [RssDownloadedDialog],
    providers: [{ provide: RssApi, useValue: api }],
  });
  const fixture = TestBed.createComponent(RssDownloadedDialog);
  fixture.componentRef.setInput('regexp', re);
  fixture.componentRef.setInput('visible', true);
  fixture.detectChanges();
  return fixture;
}

describe('RssDownloadedDialog', () => {
  it('loads the episodes of the rule when it opens', () => {
    const getDownloaded = vi.fn().mockReturnValue(of(['S01E01']));
    const fixture = create({ getDownloaded });
    expect(getDownloaded).toHaveBeenCalledWith('r1');
    expect(fixture.componentInstance['episodes']()).toEqual(['S01E01']);
  });

  it('only adds a well-formed, new entry for the mode of the rule', () => {
    const fixture = create({ getDownloaded: vi.fn().mockReturnValue(of(['S01E01'])) });
    const c = fixture.componentInstance as unknown as { newEpisode: string; add(): void; canAdd: boolean; episodes(): string[] };

    c.newEpisode = '2026-10-16';
    expect(c.canAdd).toBe(false);
    c.newEpisode = 'S01E01';
    expect(c.canAdd).toBe(false);
    c.newEpisode = 'S01E02|repack';
    expect(c.canAdd).toBe(true);
    c.add();
    expect(c.episodes()).toEqual(['S01E01', 'S01E02|repack']);
    expect(c.newEpisode).toBe('');
  });

  it('removes an entry', () => {
    const fixture = create({ getDownloaded: vi.fn().mockReturnValue(of(['S01E01', 'S01E02'])) });
    (fixture.componentInstance as unknown as { remove(e: string): void }).remove('S01E01');
    expect(fixture.componentInstance['episodes']()).toEqual(['S01E02']);
  });

  it('saves the edited list and closes', () => {
    const setDownloaded = vi.fn().mockReturnValue(of(undefined));
    const fixture = create({ getDownloaded: vi.fn().mockReturnValue(of(['S01E01'])), setDownloaded });
    const visibleChange = vi.fn();
    fixture.componentInstance.visibleChange.subscribe(visibleChange);

    (fixture.componentInstance as unknown as { onSave(): void }).onSave();

    expect(setDownloaded).toHaveBeenCalledWith('r1', ['S01E01']);
    expect(visibleChange).toHaveBeenCalledWith(false);
  });

  it('keeps the dialog open and shows the server error when saving fails', () => {
    const setDownloaded = vi.fn().mockReturnValue(throwError(() => new ApiError(422, 'invalid_property', 'bad episode')));
    const fixture = create({ getDownloaded: vi.fn().mockReturnValue(of([])), setDownloaded });
    const visibleChange = vi.fn();
    fixture.componentInstance.visibleChange.subscribe(visibleChange);

    (fixture.componentInstance as unknown as { onSave(): void }).onSave();

    expect(visibleChange).not.toHaveBeenCalled();
    expect(fixture.componentInstance['error']()).toBe('bad episode');
  });
});
