"""A stand-in for FatRat itself: drives plugins and collects what they did."""

from __future__ import annotations

import inspect
import os
import queue
import re
import sys
import threading
import time
import traceback
import urllib.error
import urllib.parse
import urllib.request
import uuid
from dataclasses import dataclass, field
from typing import Any, Callable, Dict, List, Mapping, Optional, Union

from fatrat import _async
from fatrat._http import Response
from fatrat.errors import CaptchaError, NetworkError, PluginError

DEFAULT_USER_AGENT = "FatRat/devkit"

_current: Optional["Host"] = None


def current() -> "Host":
    """The host plugins talk to. One is created on first use."""
    global _current
    if _current is None:
        Host()
    return _current  # type: ignore[return-value]


# ----------------------------------------------------------------------------
# Network
# ----------------------------------------------------------------------------

@dataclass
class Request:
    """One fetch a plugin made."""

    method: str
    url: str
    headers: Dict[str, str] = field(default_factory=dict)
    body: Optional[bytes] = None

    @property
    def text(self) -> str:
        return (self.body or b"").decode("utf-8", "replace")

    @property
    def form(self) -> Dict[str, str]:
        """The body parsed as a form (``a=1&b=2``), blank values kept."""
        return dict(urllib.parse.parse_qsl(self.text, keep_blank_values=True))


class LiveTransport:
    """Real HTTP through urllib, with the plugin's own cookie jar."""

    def __init__(self, timeout: float = 60.0) -> None:
        self.timeout = timeout

    def _open(self, request: Request, jar: Any) -> Any:
        opener = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(jar))
        prepared = urllib.request.Request(request.url, data=request.body,
                                          headers=request.headers, method=request.method)
        try:
            return opener.open(prepared, timeout=self.timeout)
        except urllib.error.HTTPError as error:
            return error
        except (urllib.error.URLError, OSError, ValueError) as error:
            raise NetworkError("Error transferring %s - %s" % (request.url, getattr(error, "reason", error)))

    def request(self, request: Request, jar: Any) -> Response:
        reply = self._open(request, jar)
        content = reply.read()
        headers = {name.lower(): value.strip() for name, value in reply.headers.items()}
        status = getattr(reply, "status", None) or reply.getcode()
        return Response(url=reply.geturl(), status=status, content=content, headers=headers)

    def download(self, request: Request, jar: Any, destination: str,
                 progress: Optional[Callable[[int, int], None]] = None) -> None:
        reply = self._open(request, jar)
        status = getattr(reply, "status", None) or reply.getcode()
        if status >= 400:
            raise NetworkError("Error transferring %s - server replied: %s" % (request.url, status))

        total = int(reply.headers.get("content-length") or 0)
        done = 0
        with open(destination, "wb") as out:
            while True:
                chunk = reply.read(65536)
                if not chunk:
                    break
                out.write(chunk)
                done += len(chunk)
                if progress:
                    progress(done, total)


Spec = Union[str, bytes, Mapping[str, Any], Response, BaseException, Callable[[Request], Any], List[Any]]


class MockTransport:
    """Canned responses, for unit tests.

    ``routes`` maps a URL to what the "server" answers. A key is an exact URL,
    ``("POST", url)`` to match a method too, or a compiled regular expression.
    A value is a body (``str`` or ``bytes``), a dict with any of ``status``,
    ``body`` and ``headers``, a ``fatrat.Response``, an exception to raise, a
    callable taking the :class:`Request`, or a list of those answered one
    after another. Anything unmatched fails with a ``NetworkError`` that names
    the URL, so a plugin fetching something unexpected is loud about it.
    """

    def __init__(self, routes: Union[Mapping[Any, Spec], Callable[[Request], Any], None] = None) -> None:
        self.routes = routes if routes is not None else {}

    def _find(self, request: Request) -> Any:
        if callable(self.routes):
            return self.routes
        for key, spec in self.routes.items():
            if isinstance(key, tuple):
                method, url = key
                if method.upper() == request.method and url == request.url:
                    return spec
            elif isinstance(key, re.Pattern):
                if key.search(request.url):
                    return spec
            elif key == request.url:
                return spec
        raise NetworkError("no mocked response for %s %s" % (request.method, request.url))

    def request(self, request: Request, jar: Any) -> Response:
        spec = self._find(request)

        while True:
            if isinstance(spec, list):
                if not spec:
                    raise NetworkError("the mocked responses for %s ran out" % request.url)
                spec = spec.pop(0) if len(spec) > 1 else spec[0]
            elif callable(spec) and not isinstance(spec, Response):
                spec = spec(request)
            else:
                break

        if isinstance(spec, BaseException):
            raise spec
        if isinstance(spec, Response):
            return spec
        if isinstance(spec, (str, bytes)):
            spec = {"body": spec}

        body = spec.get("body", b"")
        if isinstance(body, str):
            body = body.encode("utf-8")

        return Response(url=spec.get("url", request.url), status=int(spec.get("status", 200)),
                        content=body, headers={k.lower(): v for k, v in spec.get("headers", {}).items()})

    def download(self, *args: Any, **kwargs: Any) -> None:
        raise NotImplementedError("MockTransport does not download files")


# ----------------------------------------------------------------------------
# Event loop
# ----------------------------------------------------------------------------

class _Loop:
    """What FatRat's Qt event loop does for plugins: run completions in order."""

    def __init__(self) -> None:
        self._queue: "queue.Queue[Callable[[], None]]" = queue.Queue()

    def post(self, fn: Callable[[], None]) -> None:
        self._queue.put(fn)

    def call_later(self, delay: float, fn: Callable[[], None]) -> None:
        timer = threading.Timer(delay, self.post, (fn,))
        timer.daemon = True
        timer.start()

    def run_until(self, done: Callable[[], bool], timeout: float) -> None:
        deadline = time.monotonic() + timeout

        while not done():
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError("the plugin did not finish within %g s" % timeout)

            try:
                fn = self._queue.get(timeout=min(remaining, 0.2))
            except queue.Empty:
                continue

            fn()


# ----------------------------------------------------------------------------
# Results
# ----------------------------------------------------------------------------

@dataclass
class Result:
    """What running a plugin produced."""

    #: The plugin finished without failing.
    ok: bool = True
    #: Why it failed, worded as the user would see it.
    error: Optional[str] = None
    #: Python traceback, when the failure was an unexpected exception.
    traceback: Optional[str] = None
    #: download plugins: what was handed to ``start_download()``
    download: Any = None
    #: download plugins: the name from ``report_file_name()`` (or the DownloadUrl)
    file_name: Optional[str] = None
    #: extractors: the links found
    links: List[str] = field(default_factory=list)
    #: search plugins: the hits
    results: List[Any] = field(default_factory=list)
    #: account-status plugins
    account: Any = None
    #: upload plugins: ``put_download_link()``
    link: Optional[str] = None
    kill_link: str = ""
    #: ``self.vars`` at the end
    vars: Dict[str, Any] = field(default_factory=dict)
    #: ``set_message()`` texts, in order
    messages: List[str] = field(default_factory=list)
    #: seconds passed to each ``wait()``
    waits: List[int] = field(default_factory=list)
    #: every fetch the plugin made
    requests: List[Request] = field(default_factory=list)
    #: log lines written during the run
    log: List[str] = field(default_factory=list)
    #: the plugin instance, for follow-up calls such as ``final_check``
    plugin: Any = None


# ----------------------------------------------------------------------------
# The host
# ----------------------------------------------------------------------------

class Host:
    """Stands in for FatRat while plugins run.

    :param transport: :class:`LiveTransport` (default) or :class:`MockTransport`.
    :param settings: FatRat settings the plugin can read, as a dict.
    :param wait: ``"skip"`` (default) resolves countdowns at once; ``"real"``
        waits the full time.
    :param captcha: ``callable(url) -> str`` answering captchas; without one a
        captcha fails with :class:`fatrat.CaptchaError`.
    :param echo: a text stream that gets the log as it is written.
    :param timeout: seconds a single run may take.
    """

    def __init__(self, transport: Any = None, settings: Optional[Dict[str, Any]] = None, *,
                 wait: str = "skip", captcha: Optional[Callable[[str], str]] = None,
                 echo: Any = None, timeout: float = 300.0) -> None:
        global _current

        self.transport = transport if transport is not None else LiveTransport()
        self.settings: Dict[str, Any] = settings if settings is not None else {}
        self.wait_mode = wait
        self.captcha = captcha
        self.echo = echo
        self.timeout = timeout
        self.loop = _Loop()
        self.log_lines: List[str] = []
        self.requests: List[Request] = []

        _current = self

    # -- services the stand-in module calls ------------------------------
    def log(self, message: str, plugin: Any = None, quiet: bool = False) -> None:
        name = type(plugin).__name__ if plugin is not None else "fatrat"
        line = "[%s] %s" % (name, message)
        self.log_lines.append(line)

        if self.echo is not None and not quiet:
            print(line, file=self.echo, flush=True)

    def in_thread(self, fn: Callable[[], Any]) -> _async.Future:
        future = _async.Future()

        def work() -> None:
            try:
                value = fn()
            except BaseException as error:  # noqa: BLE001 - delivered to the plugin
                self.loop.post(lambda e=error: future.set_exception(e))
            else:
                self.loop.post(lambda: future.set_result(value))

        threading.Thread(target=work, daemon=True).start()
        return future

    def _send(self, state: Any, request: Request) -> _async.Future:
        self.requests.append(request)
        state.requests.append(request)

        future = _async.Future()
        self.log("fetch: %s %s" % (request.method, request.url), quiet=self.echo is None)

        def work() -> None:
            try:
                response = self.transport.request(request, state.jar)
                if response.status >= 400:
                    # FatRat's network stack treats an HTTP error status as a
                    # failed fetch, so a plugin sees the same here.
                    raise NetworkError("Error transferring %s - server replied: %s"
                                       % (request.url, response.status))
            except BaseException as error:  # noqa: BLE001 - delivered to the plugin
                self.log("fetch failed: %s" % error)
                self.loop.post(lambda e=error: future.set_exception(e))
            else:
                self.loop.post(lambda: future.set_result(response))

        threading.Thread(target=work, daemon=True).start()
        return future

    def fetch(self, state: Any, plugin: Any, url: str, data: Any, headers: Any, method: Any) -> _async.Future:
        body: Optional[bytes] = None
        merged = {"User-Agent": DEFAULT_USER_AGENT}

        if data is not None:
            if isinstance(data, bytes):
                body = data
            elif isinstance(data, str):
                body = data.encode("utf-8")
                merged["Content-Type"] = "application/x-www-form-urlencoded"
            else:
                body = urllib.parse.urlencode([(str(k), str(v)) for k, v in dict(data).items()]).encode("ascii")
                merged["Content-Type"] = "application/x-www-form-urlencoded"

        for key, value in dict(headers or {}).items():
            merged[str(key)] = str(value)

        verb = str(method).upper() if method else ("POST" if body is not None else "GET")
        return self._send(state, Request(verb, str(url), merged, body))

    def wait(self, plugin: Any, seconds: int) -> _async.Future:
        future = _async.Future()

        if seconds <= 0 or self.wait_mode != "real":
            if seconds > 0:
                self.log("waiting %d s (skipped)" % seconds, plugin=plugin)
            future.set_result(None)
            return future

        self.log("waiting %d s" % seconds, plugin=plugin)
        self.loop.call_later(seconds, lambda: future.set_result(None))
        return future

    def solve_captcha(self, plugin: Any, url: str) -> _async.Future:
        self.log("captcha: %s" % url, plugin=plugin)
        future = _async.Future()

        def work() -> None:
            try:
                if self.captcha is None:
                    raise CaptchaError("no captcha front-end is available here (see --captcha)")
                solution = self.captcha(url)
                if not solution:
                    raise CaptchaError("The captcha was not solved")
            except BaseException as error:  # noqa: BLE001 - delivered to the plugin
                self.loop.post(lambda e=error: future.set_exception(e))
            else:
                self.loop.post(lambda: future.set_result(solution))

        threading.Thread(target=work, daemon=True).start()
        return future

    def upload(self, plugin: Any, url: str, parts: List[Any], offset: int, length: int) -> _async.Future:
        state = plugin.__dict__["_dk_state"]
        path = state.source_path

        if not path:
            raise RuntimeError("upload() is only available to an upload plugin")

        with open(path, "rb") as source:
            source.seek(max(offset, 0))
            payload = source.read() if length < 0 else source.read(length)

        boundary = "----fatratdevkit" + uuid.uuid4().hex
        body = b""

        for part in parts:
            if hasattr(part, "value"):
                body += ('--%s\r\nContent-Disposition: form-data; name="%s"\r\n\r\n%s\r\n'
                         % (boundary, part.name, part.value)).encode("utf-8")
            else:
                body += ('--%s\r\nContent-Disposition: form-data; name="%s"; filename="%s"\r\n'
                         'Content-Type: application/octet-stream\r\n\r\n'
                         % (boundary, part.name, os.path.basename(path))).encode("utf-8")
                body += payload + b"\r\n"

        body += ("--%s--\r\n" % boundary).encode("ascii")

        return self._send(state, Request("POST", url, {
            "User-Agent": DEFAULT_USER_AGENT,
            "Content-Type": "multipart/form-data; boundary=" + boundary,
        }, body))

    # -- running plugins -------------------------------------------------
    def _instance(self, plugin: Any) -> Any:
        return plugin() if inspect.isclass(plugin) else plugin

    def _run(self, plugin: Any, method: str, *args: Any, source_path: Optional[str] = None) -> Result:
        from ._fake import _state

        instance = self._instance(plugin)
        state = _state(instance)
        state.source_path = source_path

        result = Result(plugin=instance)
        log_start = len(self.log_lines)
        value: Any = None
        error: Optional[BaseException] = None

        try:
            coroutine = getattr(instance, method)(*args)

            if not inspect.iscoroutine(coroutine):
                raise TypeError("%s() must be declared 'async def'" % method)

            box: Dict[str, Any] = {}
            task = _async.Task(coroutine, lambda r, e: box.update(done=True, value=r, error=e)).start()

            try:
                self.loop.run_until(lambda: "done" in box, self.timeout)
            except TimeoutError:
                task.cancel()
                raise

            value, error = box["value"], box["error"]
        except BaseException as caught:  # noqa: BLE001 - reported in the Result
            error = caught

        result.vars = dict(state.vars)
        result.messages = list(state.messages)
        result.waits = list(state.waits)
        result.requests = list(state.requests)
        result.download = state.download
        result.file_name = state.file_name
        result.link = state.link
        result.kill_link = state.kill_link

        if error is not None:
            result.ok = False
            if isinstance(error, PluginError):
                result.error = str(error)
            elif isinstance(error, _async.CancelledError):
                result.error = "cancelled"
            elif isinstance(error, TimeoutError):
                result.error = str(error)
            else:
                result.error = "unexpected %s: %s" % (type(error).__name__, error)
                result.traceback = "".join(traceback.format_exception(type(error), error, error.__traceback__))
        elif state.failed is not None:
            result.ok = False
            result.error = state.failed

        result.log = self.log_lines[log_start:]
        result._value = value  # type: ignore[attr-defined]

        return result

    def _on_failed(self, result: Result) -> None:
        hook = getattr(result.plugin, "on_failed", None)
        if not result.ok and hook is not None:
            try:
                hook()
            except Exception as error:  # noqa: BLE001
                self.log("on_failed() raised %s: %s" % (type(error).__name__, error))

    def run_download(self, plugin: Any, url: str) -> Result:
        """Run a download plugin's ``process_link(url)``."""
        result = self._run(plugin, "process_link", url)

        if result.ok and result.download is None:
            result.ok = False
            result.error = "process_link() returned without calling start_download() or fail()"

        self._on_failed(result)
        return result

    def run_final_check(self, result: Result, path: str) -> Result:
        """Run the plugin's ``final_check(path)`` on a downloaded file, updating *result*."""
        check = self._run(result.plugin, "final_check", path)

        if not check.ok:
            result.ok = False
            result.error = check.error
            result.traceback = check.traceback

        result.log.extend(check.log)
        self._on_failed(result)
        return result

    def run_extract(self, plugin: Any, url: str) -> Result:
        """Run an extractor plugin's ``extract(url)``."""
        result = self._run(plugin, "extract", url)

        if result.ok:
            links = result._value  # type: ignore[attr-defined]
            if not isinstance(links, (list, tuple)) or not all(isinstance(x, str) for x in links):
                result.ok = False
                result.error = "extract() must return a list of strings, not %r" % (links,)
            else:
                result.links = list(links)

        return result

    def run_search(self, plugin: Any, query: str) -> Result:
        """Run a search plugin's ``search(query)``."""
        result = self._run(plugin, "search", query)

        if result.ok:
            hits = result._value  # type: ignore[attr-defined]
            if not isinstance(hits, (list, tuple)):
                result.ok = False
                result.error = "search() must return a list of SearchResult, not %r" % (hits,)
            else:
                result.results = list(hits)

        return result

    def run_account_status(self, plugin: Any) -> Result:
        """Run an account-status plugin's ``account_status()``."""
        result = self._run(plugin, "account_status")

        if result.ok:
            result.account = result._value  # type: ignore[attr-defined]

        return result

    def run_upload(self, plugin: Any, path: str) -> Result:
        """Run an upload plugin's ``process_file(path)``."""
        result = self._run(plugin, "process_file", path, source_path=path)

        if result.ok and result.link is None:
            result.ok = False
            result.error = "process_file() returned without calling put_download_link() or fail()"

        self._on_failed(result)
        return result

    def download_file(self, result: Result, directory: str,
                      progress: Optional[Callable[[int, int], None]] = None) -> str:
        """Fetch the link a download plugin resolved, the way FatRat would.

        Returns the path written. Only for :class:`LiveTransport`.
        """
        from ._fake import _state

        target = result.download
        name = result.file_name or os.path.basename(urllib.parse.urlparse(target.url).path) or "download.bin"
        name = os.path.basename(urllib.parse.unquote(name)) or "download.bin"

        headers = {"User-Agent": target.user_agent or DEFAULT_USER_AGENT}
        if target.referrer:
            headers["Referer"] = target.referrer

        body = target.post_data.encode("utf-8") if target.post_data else None
        if body is not None:
            headers["Content-Type"] = "application/x-www-form-urlencoded"

        os.makedirs(directory, exist_ok=True)
        destination = os.path.join(directory, name)

        self.transport.download(Request("POST" if body is not None else "GET", target.url, headers, body),
                                _state(result.plugin).jar, destination, progress)
        return destination
