"""Values a transfer plugin passes to FatRat."""

from __future__ import annotations

from dataclasses import dataclass
from enum import IntEnum
from typing import Optional


class State(IntEnum):
    """A transfer's state, mirroring FatRat's own enumeration."""

    WAITING = 0
    ACTIVE = 1
    PAUSED = 2
    FAILED = 3
    COMPLETED = 4


@dataclass
class DownloadUrl:
    """The resolved link a download plugin hands back to FatRat.

    Only ``url`` is required. The cookies this plugin collected while
    scraping are carried over automatically -- that is the whole point of
    fetching through FatRat.
    """

    url: str
    referrer: str = ""
    user_agent: str = ""
    post_data: str = ""
    #: Overrides the name FatRat would derive from the URL.
    file_name: Optional[str] = None
