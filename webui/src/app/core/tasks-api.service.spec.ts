import { TestBed } from '@angular/core/testing';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { provideHttpClient } from '@angular/common/http';
import { TasksApi } from './tasks-api.service';

describe('TasksApi', () => {
  let api: TasksApi;
  let http: HttpTestingController;

  beforeEach(() => {
    TestBed.configureTestingModule({ providers: [provideHttpClient(), provideHttpClientTesting()] });
    api = TestBed.inject(TasksApi);
    http = TestBed.inject(HttpTestingController);
  });

  afterEach(() => http.verify());

  it("lists a transfer's actions", () => {
    let result: unknown;
    api.actions('t 1').subscribe((v) => (result = v));
    http.expectOne('/api/v1/transfers/t%201/actions').flush([{ id: 'a', label: 'A', icon: '' }]);
    expect(result).toEqual([{ id: 'a', label: 'A', icon: '' }]);
  });

  it('runs an action and reports the task it started', () => {
    let result: unknown;
    api.run('t1', 'unpack.unpack').subscribe((v) => (result = v));
    const req = http.expectOne('/api/v1/transfers/t1/actions/unpack.unpack');
    expect(req.request.method).toBe('POST');
    req.flush({ task: 'k1' }, { status: 202, statusText: 'Accepted' });
    expect(result).toBe('k1');
  });

  it('reports no task for an action that finished at once', () => {
    let result: unknown = 'unset';
    api.run('t1', 'a').subscribe((v) => (result = v));
    http.expectOne('/api/v1/transfers/t1/actions/a').flush(null, { status: 204, statusText: 'No Content' });
    expect(result).toBeNull();
  });

  it('answers and dismisses prompts', () => {
    api.answer('k1', 'p1', { name: 'Ann' }).subscribe();
    const answer = http.expectOne('/api/v1/tasks/k1/prompts/p1');
    expect(answer.request.method).toBe('POST');
    expect(answer.request.body).toEqual({ values: { name: 'Ann' } });
    answer.flush({});

    api.dismissPrompt('k1', 'p1').subscribe();
    expect(http.expectOne('/api/v1/tasks/k1/prompts/p1').request.method).toBe('DELETE');
  });

  it('lists tasks and cancels or removes one', () => {
    api.list().subscribe();
    http.expectOne('/api/v1/tasks').flush([]);

    api.cancelOrDismiss('k1').subscribe();
    expect(http.expectOne('/api/v1/tasks/k1').request.method).toBe('DELETE');
  });
});
