"""The FatRat plugin API.

Plugins import this package; the native ``_fatrat`` module underneath it is an
implementation detail. Most of the package is pure Python and imports fine
outside FatRat, so plugin authors can unit-test their parsing code normally --
only calls that reach into FatRat need FatRat.

A plugin is a module that defines a decorated class::

    import fatrat

    @fatrat.download_plugin(name="Example", url_pattern=r"https?://example\\.com/f/\\w+")
    class Example(fatrat.DownloadPlugin):
        async def process_link(self, url):
            page = await self.fetch(url)
            ...

Plugin code runs on FatRat's main thread and must not block it: every slow
operation is awaited, and genuinely CPU-bound work goes through
:func:`run_in_thread`.
"""

from __future__ import annotations

from typing import Any, Callable, Optional

from . import _native
from ._async import CancelledError, Future, Task
from ._http import Response
from .plugin import Plugin
from .settings import settings

#: Major version of the plugin API this package implements.
api_version: str = "1"

#: Version of the FatRat hosting us, or None outside FatRat.
fatrat_version: Optional[str] = getattr(_native, "__version__", None) if _native.available() else None

__version__: str = api_version

__all__ = [
    "CancelledError",
    "Future",
    "Plugin",
    "Response",
    "Task",
    "api_version",
    "fatrat_version",
    "log",
    "run_in_thread",
    "settings",
]


def log(message: object) -> None:
    """Write a line to FatRat's log, where the user can see it."""
    _native.native().log(str(message))


def run_in_thread(fn: Callable[..., Any], *args: Any, **kwargs: Any) -> Future:
    """Run a blocking or CPU-bound callable off the main thread; await the result.

    The call happens on a worker thread with the GIL released around the wait,
    so FatRat's UI keeps running. ``fn`` must not touch FatRat's API -- it only
    computes and returns.
    """
    if kwargs:
        bound = lambda: fn(*args, **kwargs)  # noqa: E731 - cheap closure
    elif args:
        bound = lambda: fn(*args)  # noqa: E731
    else:
        bound = fn
    return _native.native().run_in_thread(bound)
