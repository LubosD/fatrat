"""Exceptions a plugin can see, or raise."""

from __future__ import annotations


class PluginError(Exception):
    """Base class for plugin-visible failures.

    Letting one reach FatRat fails the transfer with this message, which the
    user sees -- so write messages for a user, not for a log.
    """


class NetworkError(PluginError):
    """A fetch failed: connection refused, timed out, TLS error, ..."""


class CaptchaError(PluginError):
    """The user dismissed the captcha, or no captcha front-end is available."""
