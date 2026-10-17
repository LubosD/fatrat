"""Loading and validating a plugin directory or repository, as FatRat would."""

from __future__ import annotations

import json
import os
import re
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Tuple

from fatrat import _loader, registry

API_VERSION = "1"
CONFIG_TYPES = ("group", "text", "password", "checkbox", "combo", "order")


@dataclass
class Report:
    """Problems found while checking something. Errors would stop FatRat loading it."""

    errors: List[str] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)

    def error(self, message: str) -> None:
        self.errors.append(message)

    def warn(self, message: str) -> None:
        self.warnings.append(message)

    @property
    def ok(self) -> bool:
        return not self.errors


def _read_json(path: str, report: Report) -> Optional[Dict[str, Any]]:
    try:
        with open(path, encoding="utf-8") as f:
            data = json.load(f)
    except FileNotFoundError:
        report.error("no %s" % os.path.basename(path))
        return None
    except ValueError as error:
        report.error("%s is not valid JSON: %s" % (os.path.basename(path), error))
        return None

    if not isinstance(data, dict):
        report.error("%s is not a JSON object" % os.path.basename(path))
        return None

    return data


def _check_config(items: Any, plugin: str, report: Report, keys: Optional[List[str]] = None) -> None:
    if keys is None:
        keys = []

    if not isinstance(items, list):
        report.error("\"config\" must be a list")
        return

    for item in items:
        if not isinstance(item, dict):
            report.error("a \"config\" entry is not an object: %r" % (item,))
            continue

        kind = item.get("type")
        if kind not in CONFIG_TYPES:
            report.error("config entry has unknown type %r (known: %s)" % (kind, ", ".join(CONFIG_TYPES)))
            continue

        if kind == "group":
            _check_config(item.get("items", []), plugin, report, keys)
            continue

        key = item.get("key")
        if not key or not isinstance(key, str):
            report.error("a %r config entry has no \"key\"" % kind)
            continue

        if key in keys:
            report.error("config key %r is declared twice" % key)
        keys.append(key)

        if not key.startswith(plugin + "/"):
            report.warn("config key %r does not start with \"%s/\"; it may collide with other settings"
                        % (key, plugin))

        if not item.get("title"):
            report.warn("config key %r has no \"title\"" % key)

        if kind in ("combo", "order") and not item.get("options"):
            report.error("config key %r (%s) has no \"options\"" % (key, kind))


@dataclass
class LoadedPlugin:
    """One plugin directory, checked and imported."""

    directory: str
    manifest: Dict[str, Any]
    report: Report
    descriptors: List[Any] = field(default_factory=list)

    @property
    def name(self) -> str:
        return str(self.manifest.get("name", os.path.basename(self.directory)))


def load(directory: str) -> LoadedPlugin:
    """Check ``plugin.json`` in *directory* and import the plugin it names.

    Applies the rules FatRat applies when it loads a plugin, plus a few
    extra warnings; nothing here needs FatRat.
    """
    directory = os.path.abspath(directory)
    report = Report()
    manifest = _read_json(os.path.join(directory, "plugin.json"), report) or {}
    loaded = LoadedPlugin(directory, manifest, report)

    if not manifest:
        return loaded

    name = manifest.get("name")
    if not isinstance(name, str) or not name:
        report.error("plugin.json has no name")
        return loaded

    if re.search(r"[^A-Za-z0-9._-]", name):
        report.warn("the name %r has characters other than letters, digits, '.', '_' and '-'" % name)

    api = manifest.get("api", API_VERSION)
    if api != API_VERSION:
        report.error("%s needs plugin API %s, this devkit has %s" % (name, api, API_VERSION))
        return loaded

    entry = manifest.get("entry") or name + ".py"
    entry_path = os.path.join(directory, entry)

    if not (os.path.isfile(entry_path) or os.path.isfile(os.path.join(entry_path, "__init__.py"))):
        report.error("the entry module %r does not exist" % entry)
        return loaded

    for key in ("version", "description", "license"):
        if not manifest.get(key):
            report.warn("plugin.json has no %r" % key)

    permissions = manifest.get("permissions", [])
    if not isinstance(permissions, list) or not all(isinstance(p, str) for p in permissions):
        report.error("\"permissions\" must be a list of strings")

    dependencies = manifest.get("dependencies", [])
    if not isinstance(dependencies, list) or not all(isinstance(d, str) for d in dependencies):
        report.error("\"dependencies\" must be a list of strings")

    if "config" in manifest:
        _check_config(manifest["config"], name, report)

    try:
        loaded.descriptors = _loader.load(name, directory, entry)
    except BaseException as error:  # noqa: BLE001 - this is what a broken plugin looks like
        report.error("importing %s failed: %s: %s" % (entry, type(error).__name__, error))
        return loaded

    if not loaded.descriptors:
        report.error("%s registers no plugin: decorate a class with @fatrat.download_plugin or similar" % entry)

    for descriptor in loaded.descriptors:
        _check_descriptor(descriptor, report)

    return loaded


_REQUIRED = {
    registry.DOWNLOAD: "process_link",
    registry.EXTRACTOR: "extract",
    registry.UPLOAD: "process_file",
    registry.SEARCH: "search",
    registry.ACCOUNT_STATUS: "account_status",
}


def _check_descriptor(descriptor: Any, report: Report) -> None:
    label = "%s (%s)" % (descriptor.cls.__name__, descriptor.kind)
    method = _REQUIRED[descriptor.kind]

    if not registry._overrides(descriptor.cls, method):
        report.error("%s does not implement %s()" % (label, method))
    elif not _is_async(descriptor.cls, method):
        report.error("%s.%s() must be declared 'async def'" % (descriptor.cls.__name__, method))

    if descriptor.kind in (registry.DOWNLOAD, registry.EXTRACTOR):
        pattern = descriptor.url_pattern
        has_acceptable = descriptor.options.get("has_acceptable")

        if not pattern and not has_acceptable:
            report.error("%s has neither a url_pattern nor an acceptable() method, so it can never match" % label)
        elif pattern:
            try:
                re.compile(pattern)
            except re.error as error:
                report.error("%s: url_pattern is not a valid regular expression: %s" % (label, error))
            else:
                if has_acceptable:
                    report.warn("%s has both a url_pattern and acceptable(); acceptable() wins" % label)


def _is_async(cls: Any, method: str) -> bool:
    import inspect

    return inspect.iscoroutinefunction(getattr(cls, method))


def accepts(descriptor: Any, url: str) -> int:
    """How well *descriptor* handles *url*, 0 to 3, like FatRat's engine selection.

    FatRat anchors ``url_pattern`` at both ends (and uses Qt's regular
    expression engine, which differs from Python's ``re`` in corners).
    """
    if descriptor.options.get("has_acceptable"):
        return int(descriptor.cls().acceptable(url))

    if descriptor.url_pattern and re.fullmatch(descriptor.url_pattern, url):
        return 3

    return 0


@dataclass
class RepositoryCheck:
    directory: str
    report: Report
    manifest: Dict[str, Any]
    plugins: List[LoadedPlugin] = field(default_factory=list)


def load_repository(directory: str) -> RepositoryCheck:
    """Check a ``fatrat-plugins.json`` repository and every plugin it lists."""
    directory = os.path.abspath(directory)
    report = Report()
    manifest = _read_json(os.path.join(directory, "fatrat-plugins.json"), report) or {}
    check = RepositoryCheck(directory, report, manifest)

    if not manifest:
        return check

    if not manifest.get("name"):
        report.error("the repository manifest has no name")

    entries = manifest.get("plugins", [])
    if not isinstance(entries, list) or not entries:
        report.warn("the repository lists no plugins")
        return check

    seen: List[str] = []

    for entry in entries:
        name = entry.get("name") if isinstance(entry, dict) else None
        if not name:
            report.error("a plugin entry has no name, so FatRat skips it: %r" % (entry,))
            continue

        if name in seen:
            report.error("plugin %r is listed twice" % name)
        seen.append(name)

        source = entry.get("source") or "./" + name
        path = os.path.normpath(os.path.join(directory, source))

        if not os.path.isdir(path):
            report.error("plugin %r: source %r is not a directory" % (name, source))
            continue

        loaded = load(path)

        if loaded.manifest.get("name") and loaded.manifest["name"] != name:
            loaded.report.error("listed as %r in fatrat-plugins.json but named %r in its plugin.json"
                                % (name, loaded.manifest["name"]))

        check.plugins.append(loaded)

    return check


def is_repository(directory: str) -> bool:
    return os.path.isfile(os.path.join(directory, "fatrat-plugins.json"))


def pick(loaded: List[LoadedPlugin], kinds: Tuple[str, ...], selector: Optional[str] = None) -> Any:
    """Choose the one descriptor of the given *kinds* across *loaded*.

    *selector* is a class name or plugin name; it is needed only when more
    than one candidate exists.
    """
    candidates = [d for plugin in loaded for d in plugin.descriptors if d.kind in kinds]

    if selector:
        candidates = [d for d in candidates if selector in (d.cls.__name__, d.name, d.identifier)]

    if not candidates:
        raise LookupError("no %s plugin%s found" % ("/".join(kinds), " matching %r" % selector if selector else ""))

    if len(candidates) > 1:
        raise LookupError("several plugins qualify (%s); pick one with --plugin"
                          % ", ".join(d.cls.__name__ for d in candidates))

    return candidates[0]
