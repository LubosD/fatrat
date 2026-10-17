"""Run and test FatRat plugins without FatRat.

Importing this package installs a stand-in for the native ``_fatrat`` module
that FatRat embeds, so a plugin's ``process_link()`` and friends really run:
page fetches go out over the network (or to canned responses, in tests), a
countdown can be skipped, a captcha is asked for on the terminal, and the
link the plugin hands back is captured instead of being downloaded.

It has to be imported *before* ``fatrat`` is, because the plugin base classes
are chosen when ``fatrat`` is first imported::

    from fatrat_devkit import Host      # first
    import fatrat                       # then the plugin API

From the command line, ``python3 -m fatrat_devkit --help``. See "Testing a
plugin" in doc/plugins.html.

This is a development aid, not a second implementation of FatRat: it follows
the native bindings closely (src/python/PyPlugin.cpp), but FatRat's own
network stack, cookie jar and captcha front-ends are not reproduced exactly.
"""

from . import _fake

_fake.install()

from .host import Host, Result, Request, MockTransport, LiveTransport  # noqa: E402

__all__ = ["Host", "LiveTransport", "MockTransport", "Request", "Result"]
