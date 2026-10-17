"""The decorators that turn a class into a FatRat plugin.

Decorating a class records it here; FatRat reads the record after importing a
plugin's entry module and registers a transfer engine for each one, exactly as
the engines built into FatRat are registered. Nothing is instantiated until a
transfer needs it.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Callable, Dict, List, Optional, Type, Union

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
    #: Kept only for plugins that implement on_config_saved().
    listener: Optional[Any] = None

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
    """True when *cls* (or a base of its own) defines *method* itself.

    Walking the MRO rather than comparing against one base class matters:
    every plugin base declares ``acceptable``, so comparing against the wrong
    one would report an override that is not there.
    """
    for klass in cls.__mro__:
        if method in klass.__dict__:
            return klass.__module__ != "fatrat.plugin"
    return False


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
                    single_transfer: Union[bool, int] = False,
                    truncate_incomplete: bool = False) -> Callable[[Type[Any]], Type[Any]]:
    """Register a download plugin.

    :param name: what the user sees in the engine list.
    :param url_pattern: regular expression matching the links this plugin
        handles, anchored at both ends. Leave it empty and implement
        ``acceptable(self, url) -> int`` instead when matching needs code.
    :param single_transfer: limit how many transfers of this plugin may run
        at a time: ``True`` for one (free-tier file hosts usually insist), or
        a number such as ``2`` for a host that allows that many. A transfer
        over the limit fails with a message saying so. ``False`` is no limit.
    :param truncate_incomplete: a restarted transfer starts from zero rather
        than resuming, because the link is single-use.
    """

    def decorate(cls: Type[Any]) -> Type[Any]:
        _register(DOWNLOAD, cls, name, url_pattern, {
            "single_transfer": max(0, int(single_transfer)),
            "truncate_incomplete": bool(truncate_incomplete),
        })
        return cls

    return decorate


def extractor_plugin(name: str, url_pattern: str = "", *,
                     transfer_class: str = "") -> Callable[[Type[Any]], Type[Any]]:
    """Register an extractor plugin: one link in, many links out.

    :param name: what the user sees in the engine list.
    :param url_pattern: regular expression matching the links this plugin
        handles, anchored at both ends.
    :param transfer_class: the engine the extracted links should be handed
        to. Leave it empty to let FatRat work that out per link, which is
        what you want unless the links only make sense to one plugin.
    """

    def decorate(cls: Type[Any]) -> Type[Any]:
        _register(EXTRACTOR, cls, name, url_pattern, {"transfer_class": transfer_class})
        return cls

    return decorate


def upload_plugin(name: str) -> Callable[[Type[Any]], Type[Any]]:
    """Register an upload plugin.

    Uploads are picked from the engine list by the user rather than matched
    against a URL, so there is no pattern here.
    """

    def decorate(cls: Type[Any]) -> Type[Any]:
        _register(UPLOAD, cls, name, "", {})
        return cls

    return decorate


def search_plugin(name: str) -> Callable[[Type[Any]], Type[Any]]:
    """Register a search plugin: a site the user can search from FatRat."""

    def decorate(cls: Type[Any]) -> Type[Any]:
        _register(SEARCH, cls, name, "", {})
        return cls

    return decorate


def account_status_plugin(name: str) -> Callable[[Type[Any]], Type[Any]]:
    """Register an account-status plugin.

    FatRat asks it, when the user looks, how much the premium account it is
    configured for has left.
    """

    def decorate(cls: Type[Any]) -> Type[Any]:
        _register(ACCOUNT_STATUS, cls, name, "", {})
        return cls

    return decorate


def notify_config_saved() -> None:
    """Tell plugins that the user saved the settings.

    Only plugins that implement ``on_config_saved`` are instantiated for
    this; the rest read their settings when they next run.
    """
    for descriptor in _registered:
        if not _overrides(descriptor.cls, "on_config_saved"):
            continue

        if descriptor.listener is None:
            descriptor.listener = descriptor.cls()

        descriptor.listener.on_config_saved()
