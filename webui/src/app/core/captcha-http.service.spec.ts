import { TestBed } from '@angular/core/testing';
import { provideHttpClient } from '@angular/common/http';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { CaptchaHttpService } from './captcha-http.service';

describe('CaptchaHttpService', () => {
  let service: CaptchaHttpService;
  let httpMock: HttpTestingController;

  beforeEach(() => {
    TestBed.configureTestingModule({
      providers: [provideHttpClient(), provideHttpClientTesting(), CaptchaHttpService],
    });
    service = TestBed.inject(CaptchaHttpService);
    httpMock = TestBed.inject(HttpTestingController);
  });

  afterEach(() => httpMock.verify());

  it('POSTs id and solution as form-urlencoded to /captcha', () => {
    return new Promise<void>((done) => {
      service.submitSolution(7, 'abc123').subscribe(() => done());

      const req = httpMock.expectOne('/captcha');
      expect(req.request.method).toBe('POST');
      expect(req.request.headers.get('Content-Type')).toBe('application/x-www-form-urlencoded');
      expect(req.request.body).toBe('id=7&solution=abc123');
      req.flush('', { status: 204, statusText: 'No Content' });
    });
  });

  it('URL-encodes a solution containing special characters', () => {
    return new Promise<void>((done) => {
      service.submitSolution(3, 'a&b=c').subscribe(() => done());

      const req = httpMock.expectOne('/captcha');
      expect(req.request.body).toBe('id=3&solution=a%26b%3Dc');
      req.flush('', { status: 204, statusText: 'No Content' });
    });
  });

  it('POSTs an empty solution', () => {
    return new Promise<void>((done) => {
      service.submitSolution(9, '').subscribe(() => done());

      const req = httpMock.expectOne('/captcha');
      expect(req.request.body).toBe('id=9&solution=');
      req.flush('', { status: 204, statusText: 'No Content' });
    });
  });
});
