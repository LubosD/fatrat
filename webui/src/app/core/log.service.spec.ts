import { TestBed } from '@angular/core/testing';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { provideHttpClient } from '@angular/common/http';
import { LogService } from './log.service';

describe('LogService', () => {
  let service: LogService;
  let httpMock: HttpTestingController;

  beforeEach(() => {
    TestBed.configureTestingModule({
      providers: [provideHttpClient(), provideHttpClientTesting(), LogService],
    });
    service = TestBed.inject(LogService);
    httpMock = TestBed.inject(HttpTestingController);
  });

  afterEach(() => httpMock.verify());

  it('GETs /log for the global log', () => {
    return new Promise<void>((done) => {
      service.getGlobalLog().subscribe((text) => {
        expect(text).toBe('global log contents');
        done();
      });

      const req = httpMock.expectOne('/log');
      expect(req.request.method).toBe('GET');
      req.flush('global log contents');
    });
  });

  it('GETs /log/<uuid> for a transfer log', () => {
    return new Promise<void>((done) => {
      service.getTransferLog('t1').subscribe((text) => {
        expect(text).toBe('transfer log contents');
        done();
      });

      const req = httpMock.expectOne('/log/t1');
      expect(req.request.method).toBe('GET');
      req.flush('transfer log contents');
    });
  });
});
