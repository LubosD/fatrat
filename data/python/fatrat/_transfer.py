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


@dataclass
class MimeValue:
    """A plain form field in an upload."""

    name: str
    value: str


@dataclass
class MimeFile:
    """The file being uploaded, as one form field.

    FatRat streams the file itself, so a plugin never reads it: this only
    says which field it goes in.
    """

    name: str


@dataclass
class SearchResult:
    """One hit from a search plugin."""

    name: str
    url: str
    #: Size in bytes, 0 when the site does not say.
    size: int = 0
    #: Anything else worth showing in the results list.
    extra: str = ""


class AccountState(IntEnum):
    """How happy a premium account is."""

    GOOD = 0
    WARNING = 1
    BAD = 2
    ERROR = 3


@dataclass
class AccountStatus:
    """What an account-status plugin reports for one account."""

    #: Free text shown next to the account: days left, traffic left, ...
    balance: str
    state: AccountState = AccountState.GOOD
