import { Injectable } from '@angular/core';
import { Observable, Subject } from 'rxjs';
import { PluginTask } from './api/models';

export interface CaptchaEvent {
  id: number;
  url: string;
}

export type TaskPushEvent = { type: 'task'; task: PluginTask } | { type: 'task-removed'; id: string };

const INITIAL_BACKOFF_MS = 1000;
const MAX_BACKOFF_MS = 30000;

/**
 * Opens /websocket and surfaces what the server pushes: captchas and plugin task changes.
 * Never sends any frame itself — the backend's reader thread discards all inbound text messages
 * (HttpService.cpp), so this is a read-only listener, matching legacy's
 * ReconnectingWebSocket-based client.js. Root singleton, app-lifetime, same as PollingService:
 * reconnects indefinitely with exponential backoff rather than giving up after N attempts.
 */
@Injectable({ providedIn: 'root' })
export class PushSocketService {
  private readonly events = new Subject<CaptchaEvent>();
  readonly captchaEvents$: Observable<CaptchaEvent> = this.events.asObservable();

  private readonly tasks = new Subject<TaskPushEvent>();
  readonly taskEvents$: Observable<TaskPushEvent> = this.tasks.asObservable();

  private readonly connections = new Subject<void>();
  /** Emits every time the socket opens: anything pushed while it was down is lost. */
  readonly connected$: Observable<void> = this.connections.asObservable();

  private backoffMs = INITIAL_BACKOFF_MS;

  constructor() {
    this.connect();
  }

  private connect(): void {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const socket = new WebSocket(`${protocol}//${window.location.host}/websocket`);

    socket.onopen = () => {
      this.backoffMs = INITIAL_BACKOFF_MS;
      this.connections.next();
    };
    socket.onmessage = (event: MessageEvent<string>) => {
      this.handleMessage(event.data);
    };
    socket.onclose = () => {
      this.scheduleReconnect();
    };
    socket.onerror = () => {
      socket.close();
    };
  }

  private handleMessage(data: string): void {
    let parsed: unknown;
    try {
      parsed = JSON.parse(data);
    } catch (err) {
      console.error('Failed to parse WebSocket message', err);
      return;
    }
    if (typeof parsed !== 'object' || parsed === null) {
      return;
    }
    const record = parsed as Record<string, unknown>;
    if (record['type'] === 'captcha' && typeof record['id'] === 'number' && typeof record['url'] === 'string') {
      this.events.next({ id: record['id'], url: record['url'] });
    }
    if (record['type'] === 'task' && typeof record['task'] === 'object' && record['task'] !== null) {
      this.tasks.next({ type: 'task', task: record['task'] as PluginTask });
    }
    if (record['type'] === 'task-removed' && typeof record['id'] === 'string') {
      this.tasks.next({ type: 'task-removed', id: record['id'] });
    }
    // 'ping' and any other message type are intentionally ignored.
  }

  private scheduleReconnect(): void {
    const delay = this.backoffMs;
    this.backoffMs = Math.min(this.backoffMs * 2, MAX_BACKOFF_MS);
    setTimeout(() => this.connect(), delay);
  }
}
