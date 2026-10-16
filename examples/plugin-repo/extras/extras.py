"""Worked examples of the plugin kinds other than download.

One module may register as many plugins as it likes, which is usually what a
real site needs: a download plugin for its links, an account-status plugin
for the premium account behind them, and a search plugin for its index.

Like `dummy`, none of this talks to a real service -- it shows the shape.
"""

from typing import List

import fatrat


@fatrat.extractor_plugin(
    name="Dummy folder",
    url_pattern=r"dummy-folder://[^/]+/.+",
    # Leave transfer_class empty and FatRat picks an engine per link, which
    # is what you want unless the links only make sense to your own plugin.
    transfer_class="",
)
class DummyFolder(fatrat.ExtractorPlugin):
    """One link in, many links out: a folder page listing files."""

    async def extract(self, url: str) -> List[str]:
        page = await self.fetch(url.replace("dummy-folder://", "http://", 1))

        # Everything FatRat needs is the list; it creates one transfer per
        # link and replaces this one with them.
        links = [self.url_of(href) for href in page.text.split()
                 if href.startswith("href=")]

        if not links:
            raise fatrat.PluginError("the folder page listed no files")

        return links

    @staticmethod
    def url_of(attribute: str) -> str:
        return attribute[len('href="'):].rstrip('">')


@fatrat.upload_plugin(name="Dummy uploader")
class DummyUpload(fatrat.UploadPlugin):
    """Posts the file as one multipart form and reports the link."""

    async def process_file(self, path: str) -> None:
        self.set_message("Uploading")

        response = await self.upload(
            "http://127.0.0.1:8099/upload",
            [fatrat.MimeValue("api_key", fatrat.settings.get("extras/api_key", "")),
             fatrat.MimeFile("file")],
        )

        link = response.json()["link"]

        # A kill link (the host's "delete this upload" URL) is optional and
        # ends up in the transfer's log.
        self.put_download_link(link, kill_link=response.json().get("kill", ""))


@fatrat.search_plugin(name="Dummy search")
class DummySearch(fatrat.SearchPlugin):
    """Searches a site; the user turns results into transfers."""

    async def search(self, query: str) -> List[fatrat.SearchResult]:
        response = await self.fetch("http://127.0.0.1:8099/search?q=" + query)

        return [
            fatrat.SearchResult(
                name=hit["name"],
                url=hit["url"],
                size=hit.get("size", 0),
                extra=hit.get("uploaded", ""),
            )
            for hit in response.json().get("hits", [])
        ]


@fatrat.account_status_plugin(name="Dummy premium")
class DummyAccount(fatrat.AccountStatusPlugin):
    """Reports what is left of the premium account, behind the status bar."""

    async def account_status(self) -> fatrat.AccountStatus:
        key = fatrat.settings.get("extras/api_key", "")

        if not key:
            return fatrat.AccountStatus("no API key configured", fatrat.AccountState.ERROR)

        response = await self.fetch("http://127.0.0.1:8099/account?key=" + key)
        days = response.json().get("days_left", 0)

        state = fatrat.AccountState.GOOD if days > 7 else fatrat.AccountState.WARNING

        return fatrat.AccountStatus("%d days left" % days, state)

    def on_config_saved(self) -> None:
        # Only implement this if you keep state that settings invalidate --
        # most plugins just read fatrat.settings when they run.
        fatrat.log("extras: settings were saved")
