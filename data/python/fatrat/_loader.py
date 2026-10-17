"""Importing an installed plugin.

Plugins are imported from their own directory under a private module name, so
two plugins may both have a ``helpers.py`` without colliding, and a plugin can
import its own modules normally.
"""

from __future__ import annotations

import importlib.util
import os
import sys
from typing import List

from . import registry

#: Namespace every plugin module is imported under.
PACKAGE = "fatrat_plugins"


def load(name: str, directory: str, entry: str) -> List[registry.PluginDescriptor]:
    """Import one plugin and return the descriptors it registered.

    :param name: the plugin's name from its manifest.
    :param directory: the directory the plugin was installed into.
    :param entry: the entry module, relative to *directory* -- a ``.py`` file
        or a package directory.
    """
    directory = os.path.abspath(directory)
    path = os.path.join(directory, entry)

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

    descriptors = registry._registered[before:]
    for descriptor in descriptors:
        descriptor.options.setdefault("plugin", name)

    return descriptors
