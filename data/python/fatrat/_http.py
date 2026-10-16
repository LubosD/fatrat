"""What a fetch gives back."""

from __future__ import annotations

import json as _json
import re
from dataclasses import dataclass, field
from typing import Any, Mapping, Optional
from urllib.parse import urljoin


@dataclass(frozen=True)
class Response:
    """The result of :meth:`fatrat.Plugin.fetch`.

    Fetches go through FatRat's network stack, so cookies, the proxy and the
    user agent are already handled; what arrives here is just the result.
    """

    url: str
    status: int
    content: bytes
    headers: Mapping[str, str] = field(default_factory=dict)

    @property
    def text(self) -> str:
        """Body decoded with the charset from Content-Type (UTF-8 otherwise)."""
        charset = "utf-8"
        ctype = self.headers.get("content-type", "")
        match = re.search(r"charset=([\w-]+)", ctype, re.I)
        if match:
            charset = match.group(1)
        try:
            return self.content.decode(charset, "replace")
        except LookupError:
            return self.content.decode("utf-8", "replace")

    def json(self) -> Any:
        return _json.loads(self.text)

    def urljoin(self, url: str) -> str:
        """Resolve a possibly relative URL against this response's URL."""
        return urljoin(self.url, url)

    def search(self, pattern: str, group: int | str = 1, flags: int = 0) -> Optional[str]:
        """``re.search`` over :attr:`text`, returning one group or None.

        The single most common thing a file-host plugin does.
        """
        match = re.search(pattern, self.text, flags)
        return match.group(group) if match else None

    @property
    def filename(self) -> Optional[str]:
        """Filename from Content-Disposition, if the server sent one."""
        disposition = self.headers.get("content-disposition", "")
        match = re.search(r'filename\*?=(?:UTF-8\'\')?"?([^";]+)"?', disposition, re.I)
        return match.group(1).strip() if match else None
