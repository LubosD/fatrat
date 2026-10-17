"""A worked example of a FatRat download plugin.

It handles links of the form ``dummy://<host>/<path>``, which no real server
serves -- the point is to show the shape of a plugin, and to give FatRat's
own tests something to drive. Read it top to bottom; a plugin for a real file
host differs only in what it scrapes.

To try it: add this directory's parent as a plugin repository
(Settings -> Extensions -> Repositories -> Add -> pick a directory), install
"dummy", restart FatRat, and add a transfer with a dummy:// link.
"""

import fatrat


@fatrat.download_plugin(
    name="Dummy example",
    url_pattern=r"dummy://[^/]+/.+",
    # A free-tier file host usually allows one transfer at a time, and its
    # resolved links are single-use, so a restart has to start over. Neither
    # is true of this example, but this is where you would say so.
    single_transfer=False,
    truncate_incomplete=False,
)
class Dummy(fatrat.DownloadPlugin):
    """Resolves a dummy:// link in the two steps a real host would take."""

    async def process_link(self, url: str) -> None:
        # Step 1: the landing page. Fetching through FatRat means this shares
        # the transfer's cookie jar, proxy and user agent with the download
        # that follows.
        page_url = url.replace("dummy://", "http://", 1)
        page = await self.fetch(page_url)

        token = page.search(r'name="token" value="([^"]+)"')
        if not token:
            # Anything raised out of here fails the transfer with this text,
            # so write it for the user, not for a log file.
            raise fatrat.PluginError("the page has no download token; has the site changed?")

        self.vars["token"] = token  # survives a restart, saved with the transfer

        # Step 2: the things that make a plugin worth having -- a countdown
        # the user can see, and a captcha handed to whichever front-end is
        # configured (desktop dialog, web UI, ...).
        if not fatrat.settings.get("dummy/skip_wait", False):
            await self.wait(3)

        captcha_url = page.search(r'<img id="captcha" src="([^"]+)"')
        if captcha_url:
            solution = await self.solve_captcha(page.urljoin(captcha_url))
        else:
            solution = ""

        # Step 3: hand the resolved link back. FatRat downloads it from here,
        # with its own segmenting, speed limits and resume.
        final = await self.fetch(page_url, data={"token": token, "captcha": solution})
        link = final.json()["link"]

        self.report_file_name(final.json().get("name", "dummy.bin"))
        self.start_download(fatrat.DownloadUrl(url=link, referrer=page_url))

    async def final_check(self, path: str) -> None:
        """File hosts answer 200 with an error page often enough to check."""
        with open(path, "rb") as f:
            if f.read(15).lower().startswith(b"<!doctype html"):
                self.fail("the host served an error page instead of the file")

    def on_failed(self) -> None:
        self.vars.pop("token", None)
