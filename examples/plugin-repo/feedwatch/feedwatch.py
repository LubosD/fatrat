"""Example of a plugin that needs a package from PyPI.

Most plugins need nothing beyond the standard library and `fatrat` — a file
host scraper is regular expressions and JSON. Declare `dependencies` in
plugin.json only when you genuinely need a library, because they are fetched
from PyPI at install time and the user has to agree to them by name.

They are installed per plugin, in `_vendor` beside this file, so two plugins
can want different versions of the same package and removing a plugin takes
its dependencies with it.
"""

from typing import List

import feedparser  # installed from PyPI by FatRat, into this plugin's _vendor

import fatrat


@fatrat.extractor_plugin(name="Feed watcher", url_pattern=r"feed-links://.+")
class FeedWatch(fatrat.ExtractorPlugin):
    """Turns a feed into the links it advertises."""

    async def extract(self, url: str) -> List[str]:
        response = await self.fetch(url.replace("feed-links://", "https://", 1))
        feed = feedparser.parse(response.content)

        links = [entry.link for entry in feed.entries if getattr(entry, "link", "")]
        if not links:
            raise fatrat.PluginError("the feed has no entries with links")

        return links
