import { Injectable, inject } from '@angular/core';
import { HttpClient } from '@angular/common/http';
import { Observable, map } from 'rxjs';

@Injectable({ providedIn: 'root' })
export class CaptchaHttpService {
  private readonly http = inject(HttpClient);

  /** Mirrors the backend's req.get_param_value("id")/("solution") form-urlencoded parsing (HttpService.cpp). */
  submitSolution(id: number, solution: string): Observable<void> {
    const body = `id=${id}&solution=${encodeURIComponent(solution)}`;
    return this.http
      .post('/captcha', body, {
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        responseType: 'text',
      })
      .pipe(map(() => undefined));
  }
}
