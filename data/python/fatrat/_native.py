"""Access to the ``_fatrat`` module built into the FatRat binary.

Importing :mod:`fatrat` outside FatRat has to work -- plugin authors unit-test
their parsing code with plain pytest, and most of this package is pure Python.
So the native module is optional here, and only *using* a native service
outside FatRat fails.
"""

from __future__ import annotations

from typing import Any

try:
    import _fatrat as _impl  # type: ignore[import-not-found]
except ImportError:  # running outside FatRat
    _impl = None


def available() -> bool:
    """True when this package is running inside FatRat."""
    return _impl is not None


def native() -> Any:
    if _impl is None:
        raise RuntimeError(
            "this part of the FatRat plugin API only works inside FatRat")
    return _impl


def __getattr__(name: str) -> Any:
    return getattr(native(), name)
