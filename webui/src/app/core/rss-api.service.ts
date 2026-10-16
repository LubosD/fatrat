import { Injectable, inject } from '@angular/core';
import { Observable } from 'rxjs';
import { RestClient } from './api/rest-client';
import { RssFeed, RssRegexp, RssRegexpProperties, RssTestResult, RssTvsMode } from './api/models';

const e = encodeURIComponent;

@Injectable({ providedIn: 'root' })
export class RssApi {
  private readonly rest = inject(RestClient);

  listFeeds(): Observable<RssFeed[]> {
    return this.rest.get<RssFeed[]>('/rss/feeds');
  }

  createFeed(feed: { name: string; url: string }): Observable<RssFeed> {
    return this.rest.post<RssFeed>('/rss/feeds', feed);
  }

  updateFeed(id: string, changes: Partial<{ name: string; url: string }>): Observable<RssFeed> {
    return this.rest.patch<RssFeed>(`/rss/feeds/${e(id)}`, changes);
  }

  /** Also deletes the regexps of the feed. */
  deleteFeed(id: string): Observable<void> {
    return this.rest.delete<void>(`/rss/feeds/${e(id)}`);
  }

  listRegexps(): Observable<RssRegexp[]> {
    return this.rest.get<RssRegexp[]>('/rss/regexps');
  }

  createRegexp(regexp: RssRegexpProperties & { feedId: string; queue: string }): Observable<RssRegexp> {
    return this.rest.post<RssRegexp>('/rss/regexps', regexp);
  }

  updateRegexp(id: string, changes: RssRegexpProperties): Observable<RssRegexp> {
    return this.rest.patch<RssRegexp>(`/rss/regexps/${e(id)}`, changes);
  }

  deleteRegexp(id: string): Observable<void> {
    return this.rest.delete<void>(`/rss/regexps/${e(id)}`);
  }

  getDownloaded(regexpId: string): Observable<string[]> {
    return this.rest.get<string[]>(`/rss/regexps/${e(regexpId)}/downloaded`);
  }

  setDownloaded(regexpId: string, episodes: string[]): Observable<void> {
    return this.rest.put<void>(`/rss/regexps/${e(regexpId)}/downloaded`, episodes);
  }

  /** Evaluated by the server, so the result agrees with what the fetcher will do. */
  test(body: {
    expression: string;
    text: string;
    tvs?: RssTvsMode;
    includeRepacks?: boolean;
    includeTrailers?: boolean;
  }): Observable<RssTestResult> {
    return this.rest.post<RssTestResult>('/rss/test', body);
  }

  refresh(): Observable<void> {
    return this.rest.post<void>('/rss/refresh');
  }
}
