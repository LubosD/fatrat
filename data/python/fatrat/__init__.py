"""The FatRat plugin API.

Plugins import this package, never the native ``_fatrat`` module underneath it.
Everything here is pure Python; ``_fatrat`` is built into the FatRat binary and
only exists while a plugin is running inside FatRat.

Phase 0 skeleton: the decorators, plugin base classes and the fetch/settings
services arrive in later phases.
"""

import _fatrat

#: Version of the FatRat that is hosting us, e.g. "1.2.0_beta2".
fatrat_version: str = _fatrat.__version__

#: Major version of the plugin API. A plugin declares the one it was written
#: against in its plugin.json ("api"); FatRat refuses to load a plugin whose
#: declared version it does not implement.
api_version: str = _fatrat.api_version

#: Version of this package. Tracks api_version until the API gains minor
#: revisions worth distinguishing.
__version__: str = api_version

__all__ = ["api_version", "fatrat_version", "log"]


def log(message: object) -> None:
    """Write a line to FatRat's log, where the user can see it."""
    _fatrat.log(str(message))
