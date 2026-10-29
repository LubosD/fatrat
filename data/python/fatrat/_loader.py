"""Importing an installed plugin.

Plugins are imported from their own directory under a private module name, so
two plugins may both have a ``helpers.py`` without colliding, and a plugin can
import its own modules normally.
"""

from __future__ import annotations

import importlib.machinery
import importlib.util
import os
import sys
from typing import Dict, List

from . import registry

#: Namespace every plugin module is imported under.
PACKAGE = "fatrat_plugins"

#: Plugin name -> the directory it was imported from, for unload().
_directories: Dict[str, str] = {}


def load(name: str, directory: str, entry: str) -> List[registry.PluginDescriptor]:
    """Import one plugin and return the descriptors it registered.

    :param name: the plugin's name from its manifest.
    :param directory: the directory the plugin was installed into.
    :param entry: the entry module, relative to *directory* -- a ``.py`` file
        or a package directory.
    """
    directory = os.path.abspath(directory)
    path = os.path.join(directory, entry)

    # Loading again (a reinstall, an upgrade, an edit during development)
    # must run the code on disk now, not whatever Python cached last time.
    unload(name)

    if os.path.isdir(path):
        path = os.path.join(path, "__init__.py")
    if not os.path.isfile(path):
        raise FileNotFoundError("plugin %r has no entry module at %s" % (name, path))

    if PACKAGE not in sys.modules:
        package = importlib.util.module_from_spec(
            importlib.machinery.ModuleSpec(PACKAGE, None, is_package=True))
        sys.modules[PACKAGE] = package

    module_name = "%s.%s" % (PACKAGE, name.replace("-", "_"))

    # A plugin may ship helper modules next to its entry point, and its
    # dependencies (if it declared any) live in _vendor beside them.
    vendor = os.path.join(directory, "_vendor")
    if os.path.isdir(vendor) and vendor not in sys.path:
        sys.path.insert(0, vendor)
    if directory not in sys.path:
        sys.path.insert(0, directory)

    before = len(registry._registered)

    spec = importlib.util.spec_from_file_location(module_name, path,
                                                  submodule_search_locations=[directory])
    if spec is None or spec.loader is None:
        raise ImportError("cannot import plugin %r from %s" % (name, path))

    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    try:
        spec.loader.exec_module(module)
    except BaseException:
        sys.modules.pop(module_name, None)
        del registry._registered[before:]
        raise

    _directories[name] = directory

    descriptors = registry._registered[before:]
    for descriptor in descriptors:
        descriptor.options.setdefault("plugin", name)

    return descriptors


def unload(name: str) -> None:
    """Forget a plugin, so that loading it again imports it afresh.

    Drops the plugin's module and every other module that was imported from
    its directory (its helpers and vendored packages), and takes the
    directory off ``sys.path``. Objects the plugin already handed out keep
    working -- they hold their own references -- which is what lets running
    transfers finish after the plugin was unloaded.

    Extension modules (compiled code in ``_vendor``) cannot be unloaded by
    Python; a reload of one keeps the old copy.
    """
    directory = _directories.pop(name, None)
    module_name = "%s.%s" % (PACKAGE, name.replace("-", "_"))

    for loaded in [m for m in sys.modules if m == module_name or m.startswith(module_name + ".")]:
        del sys.modules[loaded]

    if directory is None:
        return

    prefix = directory + os.sep

    for loaded, module in list(sys.modules.items()):
        origin = getattr(module, "__file__", None)
        if origin and os.path.abspath(origin).startswith(prefix):
            del sys.modules[loaded]

    for entry in (os.path.join(directory, "_vendor"), directory):
        while entry in sys.path:
            sys.path.remove(entry)

    registry._registered[:] = [d for d in registry._registered
                               if d.options.get("plugin") != name]
    importlib.invalidate_caches()
