"""Plugin base classes.

:class:`Plugin` is the root of every plugin type. Its services come from the
native base class that FatRat builds in: :meth:`fetch` runs through FatRat's
own network stack, so the plugin inherits the configured proxy, the cookie jar
of this transfer and the user agent, and the transfer's log shows what the
plugin did.
"""

from __future__ import annotations

from collections.abc import MutableMapping
from typing import Any, Iterator, Mapping, Optional, Union

from . import _native
from ._async import Future
from ._transfer import DownloadUrl, State

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


class _Vars(MutableMapping):
    """``plugin.vars``: values saved with the transfer in queues.xml."""

    __slots__ = ("_plugin",)

    def __init__(self, plugin: "TransferPlugin") -> None:
        self._plugin = plugin

    def __getitem__(self, key: str) -> Any:
        value = self._plugin._var_get(str(key))
        if value is None:
            raise KeyError(key)
        return value

    def get(self, key: str, default: Any = None) -> Any:
        value = self._plugin._var_get(str(key))
        return default if value is None else value

    def __setitem__(self, key: str, value: Any) -> None:
        self._plugin._var_set(str(key), value)

    def __delitem__(self, key: str) -> None:
        self._plugin._var_set(str(key), None)

    def __iter__(self) -> Iterator[str]:
        return iter(self._plugin._var_names())

    def __len__(self) -> int:
        return len(self._plugin._var_names())

    def __repr__(self) -> str:
        return repr(dict(self))


class TransferPlugin(Plugin):
    """Base of the plugins that drive a transfer (download, upload).

    Beyond :class:`Plugin`, the native base gives a transfer plugin:

    ``log(message)``
        goes into *this transfer's* log, which is where a user looks when a
        download fails.

    ``set_message(text)``
        the one-line status shown in the transfer list.

    ``fail(message)``
        fail the transfer with a message. Raising
        :class:`fatrat.errors.PluginError` does the same thing.

    ``vars``
        a dict of values that survive a restart, saved with the transfer in
        queues.xml. Use it for anything you would otherwise re-scrape (a file
        id, a resolved name); values are stored as text.
    """

    @property
    def vars(self) -> _Vars:
        """Per-transfer values that survive a restart."""
        return _Vars(self)


class DownloadPlugin(TransferPlugin):
    """Resolves a link, then hands the real URL back to FatRat.

    A plugin does the scraping; the download itself is FatRat's job, with its
    segmenting, speed limits and resume. Override :meth:`process_link` and
    finish it by calling :meth:`start_download`.

    Native services, beyond those of :class:`TransferPlugin`:

    ``start_download(url_or_DownloadUrl)``
        hand the resolved link to FatRat and finish. Cookies collected by
        this plugin's fetches are carried over.

    ``wait(seconds)``
        await a countdown, shown to the user. The transfer stays visibly
        alive instead of looking stuck.

    ``solve_captcha(url)``
        await a captcha solution: the desktop dialog, the web UI, or whatever
        captcha front-end is configured. Raises
        :class:`fatrat.errors.CaptchaError` if the user gives up.

    ``report_file_name(name)``
        name the transfer before the download starts, when the link itself
        does not carry a usable name.
    """

    async def process_link(self, url: str) -> None:
        """Resolve *url* and call :meth:`start_download`. Must be overridden."""
        raise NotImplementedError(
            "%s must implement process_link()" % type(self).__name__)

    async def final_check(self, path: str) -> None:
        """Inspect the finished file; call ``fail()`` to reject it.

        File hosts like to serve an HTML error page with a 200, which would
        otherwise look like a successful download.
        """

    def on_failed(self) -> None:
        """Called when the transfer failed, to clean up plugin-side state."""

    def acceptable(self, url: str) -> int:
        """How well this plugin handles *url*: 0 (not at all) to 3 (exactly).

        The default uses the decorator's ``url_pattern``; override when the
        decision needs code.
        """
        raise NotImplementedError


__all__ = ["DownloadPlugin", "DownloadUrl", "Plugin", "State", "TransferPlugin"]
