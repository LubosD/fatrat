import { Injectable, inject } from '@angular/core';
import { HttpClient, HttpErrorResponse, HttpParams } from '@angular/common/http';
import { Observable, throwError } from 'rxjs';
import { catchError } from 'rxjs/operators';

const BASE = '/api/v1';

/** Error thrown by RestClient for any failed call; mirrors the {error:{code,message}} envelope. */
export class ApiError extends Error {
  constructor(
    readonly status: number,
    readonly code: string,
    message: string,
  ) {
    super(message);
    this.name = 'ApiError';
  }
}

export type QueryParams = { [key: string]: string | boolean | readonly string[] };

function toApiError(err: unknown): ApiError {
  if (!(err instanceof HttpErrorResponse)) {
    return new ApiError(0, 'unknown', err instanceof Error ? err.message : String(err));
  }
  const envelope = (err.error as { error?: { code?: unknown; message?: unknown } } | null)?.error;
  const code = typeof envelope?.code === 'string' ? envelope.code : 'unknown';
  const message = typeof envelope?.message === 'string' ? envelope.message : err.statusText;
  return new ApiError(err.status, code, message);
}

/** Thin JSON wrapper over HttpClient rooted at /api/v1. */
@Injectable({ providedIn: 'root' })
export class RestClient {
  private readonly http = inject(HttpClient);

  get<T>(path: string, query?: QueryParams): Observable<T> {
    return this.http
      .get<T>(BASE + path, { params: toParams(query) })
      .pipe(catchError((e) => throwError(() => toApiError(e))));
  }

  post<T>(path: string, body?: unknown): Observable<T> {
    return this.http
      .post<T>(BASE + path, body ?? null)
      .pipe(catchError((e) => throwError(() => toApiError(e))));
  }

  put<T>(path: string, body: unknown): Observable<T> {
    return this.http
      .put<T>(BASE + path, body)
      .pipe(catchError((e) => throwError(() => toApiError(e))));
  }

  patch<T>(path: string, body: unknown): Observable<T> {
    return this.http
      .patch<T>(BASE + path, body)
      .pipe(catchError((e) => throwError(() => toApiError(e))));
  }

  delete<T>(path: string, query?: QueryParams): Observable<T> {
    return this.http
      .delete<T>(BASE + path, { params: toParams(query) })
      .pipe(catchError((e) => throwError(() => toApiError(e))));
  }
}

function toParams(query?: QueryParams): HttpParams {
  let params = new HttpParams();
  for (const [key, value] of Object.entries(query ?? {})) {
    if (typeof value === 'string' || typeof value === 'boolean') {
      params = params.append(key, String(value));
    } else {
      for (const item of value) params = params.append(key, item);
    }
  }
  return params;
}
