import { TestBed } from '@angular/core/testing';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { provideHttpClient } from '@angular/common/http';
import { BrowseService } from './browse.service';

describe('BrowseService', () => {
  let service: BrowseService;
  let httpMock: HttpTestingController;

  beforeEach(() => {
    TestBed.configureTestingModule({
      providers: [provideHttpClient(), provideHttpClientTesting(), BrowseService],
    });
    service = TestBed.inject(BrowseService);
    httpMock = TestBed.inject(HttpTestingController);
  });

  afterEach(() => httpMock.verify());

  it('GETs /browse/<uuid> with the path param and parses items into TreeNodes', () => {
    return new Promise<void>((done) => {
      service.list('t1', '').subscribe((nodes) => {
        expect(nodes).toEqual([
          { key: '/movies', label: 'movies', leaf: false },
          { key: '/readme.txt', label: 'readme.txt', leaf: true },
        ]);
        done();
      });

      const req = httpMock.expectOne((r) => r.url === '/browse/t1');
      expect(req.request.method).toBe('GET');
      expect(req.request.params.get('path')).toBe('');
      req.flush(
        '<root>' +
          '<item id="/movies" type="folder" state="closed"><content><name>movies</name></content></item>' +
          '<item id="/readme.txt" type="default"><content><name>readme.txt</name></content></item>' +
          '</root>',
      );
    });
  });

  it('passes a non-empty path through to the request', () => {
    return new Promise<void>((done) => {
      service.list('t1', '/movies').subscribe(() => done());

      const req = httpMock.expectOne((r) => r.url === '/browse/t1');
      expect(req.request.params.get('path')).toBe('/movies');
      req.flush('<root></root>');
    });
  });
});
