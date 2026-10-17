"""The stand-in ``_fatrat`` module (see :mod:`fatrat_devkit`).

Mirrors the surface that src/python/PyModule.cpp, PyCore.cpp, PyPlugin.cpp and
PyTransferBindings.cpp expose, and nothing more.
"""

from __future__ import annotations

import sys
import types
import uuid
from typing import Any, Dict, List, Optional

VERSION = "devkit"


def install() -> None:
    """Make ``import _fatrat`` resolve to the stand-in.

    Does nothing inside FatRat itself, where the real module exists.
    """
    existing = sys.modules.get("_fatrat")
    if existing is not None:
        return  # real FatRat, or already installed

    if "fatrat" in sys.modules:
        raise RuntimeError(
            "import fatrat_devkit before fatrat: the plugin base classes are "
            "chosen when fatrat is first imported")

    sys.modules["_fatrat"] = _build()


class _State:
    """What one plugin instance did, collected for :class:`~fatrat_devkit.Result`."""

    def __init__(self) -> None:
        from http.cookiejar import CookieJar

        self.jar = CookieJar()
        self.vars: Dict[str, Any] = {}
        self.messages: List[str] = []
        self.download: Any = None
        self.file_name: Optional[str] = None
        self.failed: Optional[str] = None
        self.link: Optional[str] = None
        self.kill_link: str = ""
        self.waits: List[int] = []
        self.requests: List[Any] = []
        self.source_path: Optional[str] = None
        self.finished = False


def _state(plugin: Any) -> _State:
    # Plugins may define __init__ without calling super().__init__().
    state = plugin.__dict__.get("_dk_state")
    if state is None:
        state = plugin.__dict__["_dk_state"] = _State()
    return state


def _host() -> Any:
    from . import host

    return host.current()


def _build() -> types.ModuleType:
    module = types.ModuleType("_fatrat")
    module.__doc__ = "FatRat native plugin API stand-in (fatrat_devkit)"
    module.__version__ = VERSION
    module.api_version = "1"

    def log(message: str) -> None:
        _host().log(str(message))

    def settings_get(key: str, default: Any = None) -> Any:
        return _host().settings.get(key, default)

    def settings_set(key: str, value: Any) -> None:
        _host().settings[key] = value

    def settings_remove(key: str) -> None:
        _host().settings.pop(key, None)

    def settings_get_array(key: str) -> List[Any]:
        return list(_host().settings.get(key, []) or [])

    def run_in_thread(callable_: Any) -> Any:
        return _host().in_thread(callable_)

    module.log = log
    module.settings_get = settings_get
    module.settings_set = settings_set
    module.settings_remove = settings_remove
    module.settings_get_array = settings_get_array
    module.run_in_thread = run_in_thread
    module.PluginBase = _plugin_base()

    return module


def _plugin_base() -> type:
    """The ``PluginBase`` class (fatrat's own types are imported lazily)."""

    class PluginBase:
        plugin_name = ""

        def __init__(self, *args: Any, **kwargs: Any) -> None:
            _state(self)

        # -- shared by every plugin kind ---------------------------------
        def fetch(self, url: str, data: Any = None, headers: Any = None, method: Any = None) -> Any:
            return _host().fetch(_state(self), self, url, data, headers, method)

        def log(self, message: Any) -> None:
            _host().log(str(message), plugin=self)

        def wait(self, seconds: int) -> Any:
            state = _state(self)
            state.waits.append(int(seconds))
            return _host().wait(self, int(seconds))

        def solve_captcha(self, url: str) -> Any:
            return _host().solve_captcha(self, url)

        # -- transfer plugins --------------------------------------------
        def set_message(self, text: Any) -> None:
            state = _state(self)
            state.messages.append(str(text))
            _host().log("status: %s" % text, plugin=self, quiet=True)

        def set_state(self, state: int) -> None:
            pass

        def fail(self, message: Any) -> None:
            state = _state(self)
            state.failed = str(message)
            state.messages.append(str(message))
            _host().log("FAILED: %s" % message, plugin=self)

        def _var_get(self, key: str) -> Any:
            return _state(self).vars.get(key)

        def _var_set(self, key: str, value: Any) -> None:
            vars_ = _state(self).vars
            if value is None:
                vars_.pop(key, None)
            else:
                vars_[key] = value

        def _var_names(self) -> List[str]:
            return list(_state(self).vars)

        def report_file_name(self, name: Any) -> None:
            _state(self).file_name = str(name)

        def start_download(self, target: Any) -> None:
            from fatrat import DownloadUrl

            if isinstance(target, str):
                target = DownloadUrl(url=target)
            if not target.url:
                raise RuntimeError("start_download() needs a URL")

            state = _state(self)
            state.download = target
            if target.file_name:
                state.file_name = target.file_name

        # -- upload plugins ----------------------------------------------
        def upload(self, url: str, parts: Any, offset: int = 0, length: int = -1) -> Any:
            return _host().upload(self, url, list(parts), offset, length)

        def put_download_link(self, url: str, kill_link: str = "") -> None:
            state = _state(self)
            state.link = url
            state.kill_link = kill_link

    return PluginBase


__all__ = ["install", "VERSION"]
