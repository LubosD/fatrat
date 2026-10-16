"""FatRat's settings, as a dict-like object.

Plugins own the keys they declare in ``plugin.json`` (conventionally
``<plugin>/<name>``); the rest of FatRat's settings are readable too, so a
plugin can honour things like the download directory.
"""

from __future__ import annotations

from typing import Any, Iterator, List, MutableMapping

from . import _native


class _Settings(MutableMapping):
    def __getitem__(self, key: str) -> Any:
        value = _native.native().settings_get(str(key), None)
        if value is None:
            raise KeyError(key)
        return value

    def get(self, key: str, default: Any = None) -> Any:
        return _native.native().settings_get(str(key), default)

    def __setitem__(self, key: str, value: Any) -> None:
        _native.native().settings_set(str(key), value)

    def __delitem__(self, key: str) -> None:
        _native.native().settings_remove(str(key))

    def __contains__(self, key: object) -> bool:
        sentinel = object()
        return _native.native().settings_get(str(key), sentinel) is not sentinel

    def __iter__(self) -> Iterator[str]:
        raise TypeError("FatRat's settings cannot be enumerated; read keys by name")

    def __len__(self) -> int:
        raise TypeError("FatRat's settings cannot be enumerated; read keys by name")

    def array(self, key: str) -> List[Any]:
        """Read a settings array (what an ``order`` config item stores)."""
        return list(_native.native().settings_get_array(str(key)))

    def __repr__(self) -> str:
        return "<FatRat settings>"


#: The settings singleton: ``fatrat.settings["myplugin/user"]``.
settings = _Settings()
