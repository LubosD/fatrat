"""Plugin base classes.

:class:`Plugin` is the root of every plugin type. Its services come from the
native base class that FatRat builds in: :meth:`fetch` runs through FatRat's
own network stack, so the plugin inherits the configured proxy, the cookie jar
of this transfer and the user agent, and the transfer's log shows what the
plugin did.
"""

from __future__ import annotations

from typing import Any, Mapping, Optional, Union

from . import _native
from ._async import Future

if _native.available():
    _Base: Any = _native.native().PluginBase
else:
    class _Base:  # pragma: no cover - import-time stand-in outside FatRat
        """Stand-in so that ``import fatrat`` works outside FatRat."""

        def __init__(self, *args: Any, **kwargs: Any) -> None:
            pass

        def __getattr__(self, name: str) -> Any:
            raise RuntimeError(
                "%r is only available while running inside FatRat" % (name,))


class Plugin(_Base):
    """Common base of every FatRat plugin.

    Inherited from the native base:

    ``fetch(url, *, data=None, headers=None, method=None) -> Future``
        Fetch a page through FatRat. ``data`` makes it a POST: pass a mapping
        (form-encoded for you), or ``str``/``bytes`` to send a body verbatim.
        Await it for a :class:`fatrat.Response`; a failure raises
        :class:`fatrat.errors.NetworkError` into your coroutine.

    ``log(message)``
        Write to this transfer's log (the global log when there is no
        transfer, such as during a search).
    """

    def fetch(self, url: str, *, data: Union[None, Mapping[str, Any], str, bytes] = None,
              headers: Optional[Mapping[str, str]] = None,
              method: Optional[str] = None) -> Future:  # pragma: no cover - native
        return _Base.fetch(self, url, data=data, headers=headers, method=method)
