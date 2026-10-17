# Ubuntu packages

FatRat is packaged for Ubuntu releases with Qt 6.5 or newer (currently 26.10 "stonking" and 26.04 "resolute")
and published as a self-hosted apt repository. Packages:

| Package         | Contents                                                                  |
|-----------------|---------------------------------------------------------------------------|
| `fatrat`        | the Qt desktop application                                                |
| `fatrat-nogui`  | `fatrat-nogui` (headless, controlled through the web interface) and `fatrat-tui` |
| `fatrat-common` | web interface, Python extension support, translations, `fatrat-conf`      |

All packages are built with libcurl, BitTorrent, the web interface and Python extensions; `fatrat` also
has D-Bus and the help browser. Jabber (gloox) is not enabled.

## Building

    packaging/deb/build.sh [SERIES...]

This needs only Docker. Each series is built in an `ubuntu:<release>` container. The web UI is built there
with npm (it needs network access), and then `dpkg-buildpackage` builds FatRat twice (GUI and nogui)
using `-DWEBUI_PREBUILT=ON`. The `.deb` files end up in `packaging/deb/out/SERIES/`.

To rebuild the same upstream version, bump the Debian revision: `DEB_REVISION=2 packaging/deb/build.sh`.
The upstream version comes from `VERSION` in `src/fatrat.h`.

To add a new Ubuntu release, add it to `RELEASES` in `build.sh`.

## Publishing

The repository is served by GitHub Pages from the `gh-pages` branch, together with the FatRat website
(`website/`): https://fatrat.dolezel.info/

    GPG_KEY=87619D75096A7E41 packaging/deb/publish.sh --upload

This adds the built packages to the reprepro repository in `packaging/deb/repo/`, signs it with your
key (on the host, not in Docker), then runs `website/deploy.sh`, which copies it into the `gh-pages`
worktree in `packaging/deb/gh-pages/` together with the website, then commits and pushes. To update only
the website, run `website/deploy.sh` on its own. Without `--upload`, it only updates `packaging/deb/repo/`.

Keep `packaging/deb/repo/` (it is not in git): its `db/` is reprepro's record of the published packages.
If it is lost, the next publish only contains the packages being published then.

## Installing (users)

    sudo install -d /etc/apt/keyrings
    sudo curl -fsSLo /etc/apt/keyrings/fatrat.gpg https://fatrat.dolezel.info/fatrat.gpg
    echo "deb [signed-by=/etc/apt/keyrings/fatrat.gpg] https://fatrat.dolezel.info $(. /etc/os-release; echo $VERSION_CODENAME) main" \
        | sudo tee /etc/apt/sources.list.d/fatrat.list
    sudo apt update
    sudo apt install fatrat          # or fatrat-nogui
