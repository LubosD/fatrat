"""The cooperative scheduler plugins run on.

There is no asyncio here and no second thread: a plugin coroutine runs on
FatRat's main thread, and every ``await`` suspends it on a :class:`Future`
that FatRat's C++ side completes from a Qt signal -- a finished network
reply, an elapsed countdown, a solved captcha. Completing a future resumes
the coroutine right there, inside that slot, which is exactly where the rest
of FatRat expects plugin code to run.

Plugins never import this module directly; they just ``await``.
"""

from __future__ import annotations

from typing import Any, Callable, Generator, Optional


class CancelledError(BaseException):
    """Thrown into a plugin coroutine when its transfer is stopped.

    Derives from BaseException so that a plugin's ``except Exception`` does
    not accidentally swallow a stop request.
    """


class Future:
    """A value that FatRat will supply later. Await it."""

    __slots__ = ("_done", "_value", "_exc", "_callbacks", "_on_cancel", "_cancelled")

    def __init__(self, on_cancel: Optional[Callable[[], None]] = None) -> None:
        self._done = False
        self._value: Any = None
        self._exc: Optional[BaseException] = None
        self._callbacks: list[Callable[[Future], None]] = []
        self._on_cancel = on_cancel
        self._cancelled = False

    # -- completion, called by FatRat ------------------------------------
    def set_result(self, value: Any = None) -> None:
        if self._done:
            return
        self._done = True
        self._value = value
        self._fire()

    def set_exception(self, exc: BaseException) -> None:
        if self._done:
            return
        self._done = True
        self._exc = exc
        self._fire()

    def _fire(self) -> None:
        callbacks, self._callbacks = self._callbacks, []
        for cb in callbacks:
            cb(self)

    # -- consumption -----------------------------------------------------
    def done(self) -> bool:
        return self._done

    def cancelled(self) -> bool:
        return self._cancelled

    def cancel(self) -> None:
        """Abandon the underlying operation, if it has not completed."""
        if self._done or self._cancelled:
            return
        self._cancelled = True
        if self._on_cancel is not None:
            on_cancel, self._on_cancel = self._on_cancel, None
            on_cancel()

    def add_done_callback(self, cb: Callable[["Future"], None]) -> None:
        if self._done:
            cb(self)
        else:
            self._callbacks.append(cb)

    def result(self) -> Any:
        if not self._done:
            raise RuntimeError("future is not done")
        if self._exc is not None:
            raise self._exc
        return self._value

    def __await__(self) -> Generator["Future", Any, Any]:
        if not self._done:
            yield self
        return self.result()


class Task:
    """Drives one plugin coroutine to completion.

    ``on_done(result, exception)`` is called exactly once, from whatever slot
    completed the last future (or inline, if the coroutine never suspended).
    """

    __slots__ = ("_coro", "_on_done", "_awaiting", "_finished", "_stepping")

    def __init__(self, coro, on_done: Optional[Callable[[Any, Optional[BaseException]], None]] = None) -> None:
        self._coro = coro
        self._on_done = on_done
        self._awaiting: Optional[Future] = None
        self._finished = False
        self._stepping = False

    def start(self) -> "Task":
        self._step(None, None)
        return self

    def done(self) -> bool:
        return self._finished

    def cancel(self) -> None:
        """Stop the coroutine: cancel what it awaits, raise CancelledError in it."""
        if self._finished:
            return
        awaiting, self._awaiting = self._awaiting, None
        if awaiting is not None:
            awaiting.cancel()
        self._step(None, CancelledError())

    def _wake(self, future: "Future") -> None:
        if self._awaiting is not future:
            return  # a cancelled or superseded future completing late
        self._awaiting = None
        self._step(future._value, future._exc)

    def _step(self, value: Any, exc: Optional[BaseException]) -> None:
        if self._finished or self._stepping:
            return
        self._stepping = True
        try:
            while True:
                try:
                    if exc is not None:
                        awaited = self._coro.throw(exc)
                    else:
                        awaited = self._coro.send(value)
                except StopIteration as stop:
                    self._finish(stop.value, None)
                    return
                except BaseException as error:  # noqa: BLE001 - reported to FatRat
                    self._finish(None, error)
                    return

                if not isinstance(awaited, Future):
                    value, exc = None, TypeError(
                        "plugins may only await objects provided by FatRat, got %r" % (awaited,))
                    continue

                if awaited.done():
                    # Already resolved: continue in this loop rather than
                    # recursing, so a plugin that awaits many ready futures
                    # cannot blow the stack.
                    value, exc = awaited._value, awaited._exc
                    continue

                self._awaiting = awaited
                awaited.add_done_callback(self._wake)
                return
        finally:
            self._stepping = False

    def _finish(self, result: Any, exc: Optional[BaseException]) -> None:
        self._finished = True
        self._awaiting = None
        on_done, self._on_done = self._on_done, None
        if on_done is not None:
            on_done(result, exc)


def run(coro, on_done: Optional[Callable[[Any, Optional[BaseException]], None]] = None) -> Task:
    """Start a coroutine as a Task. Used by FatRat, not by plugins."""
    return Task(coro, on_done).start()
