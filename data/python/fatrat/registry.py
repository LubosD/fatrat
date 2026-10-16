"""The decorators that turn a class into a FatRat plugin.

Decorating a class records it here; FatRat reads the record after importing a
plugin's entry module and registers a transfer engine for each one, exactly as
the engines built into FatRat are registered. Nothing is instantiated until a
transfer needs it.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Callable, Dict, List, Optional, Type

#: Plugin kinds FatRat knows about.
DOWNLOAD = "download"
EXTRACTOR = "extractor"
UPLOAD = "upload"
SEARCH = "search"
ACCOUNT_STATUS = "account_status"


@dataclass
class PluginDescriptor:
    """One decorated class, as FatRat sees it."""

    kind: str
    name: str
    cls: Type[Any]
    module: str
    url_pattern: str = ""
    options: Dict[str, Any] = field(default_factory=dict)

    @property
    def identifier(self) -> str:
        """Stable id used in queues.xml and in FatRat's engine registry.

        It has to survive restarts and FatRat upgrades, so it is the module
        and class name rather than anything the user can change.
        """
        return "%s.%s" % (self.module, self.cls.__name__)


_registered: List[PluginDescriptor] = []


def registered() -> List[PluginDescriptor]:
    """Every plugin class decorated so far. FatRat calls this; plugins do not."""
    return list(_registered)


def _overrides(cls: Type[Any], method: str) -> bool:
    """True when *cls* implements *method* itself rather than inheriting the default."""
    from . import plugin as _plugin

    for base in (_plugin.DownloadPlugin, _plugin.TransferPlugin, _plugin.Plugin):
        default = getattr(base, method, None)
        if default is not None:
            return getattr(cls, method, None) is not default
    return hasattr(cls, method)


def _register(kind: str, cls: Type[Any], name: str, url_pattern: str, options: Dict[str, Any]) -> None:
    options["has_acceptable"] = _overrides(cls, "acceptable")
    descriptor = PluginDescriptor(
        kind=kind,
        name=name or cls.__name__,
        cls=cls,
        module=cls.__module__,
        url_pattern=url_pattern,
        options=options,
    )
    cls._fatrat_descriptor = descriptor
    _registered.append(descriptor)


def download_plugin(name: str, url_pattern: str = "", *,
                    single_transfer: bool = False,
                    truncate_incomplete: bool = False) -> Callable[[Type[Any]], Type[Any]]:
    """Register a download plugin.

    :param name: what the user sees in the engine list.
    :param url_pattern: regular expression matching the links this plugin
        handles, anchored at both ends. Leave it empty and implement
        ``acceptable(self, url) -> int`` instead when matching needs code.
    :param single_transfer: only one transfer of this plugin may run at a
        time (free-tier file hosts usually insist).
    :param truncate_incomplete: a restarted transfer starts from zero rather
        than resuming, because the link is single-use.
    """

    def decorate(cls: Type[Any]) -> Type[Any]:
        _register(DOWNLOAD, cls, name, url_pattern, {
            "single_transfer": bool(single_transfer),
            "truncate_incomplete": bool(truncate_incomplete),
        })
        return cls

    return decorate
