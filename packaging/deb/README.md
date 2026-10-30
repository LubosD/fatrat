# Ubuntu packages

FatRat is packaged for Ubuntu releases with Qt 6.5 or newer (currently 26.10 "stonking" and 26.04 "resolute")
and published as a self-hosted apt repository. Packages:

| Package         | Contents                                                                  |
|-----------------|---------------------------------------------------------------------------|
| `fatrat`        | the Qt desktop application                                                |
| `fatrat-nogui`  | `fatrat-nogui` (headless, controlled through the web interface) and `fatrat-tui` |
| `fatrat-common` | web interface, Python extension support, translations, `fatrat-conf`      |
| `fatrat-dev`    | headers for native plugins: `/usr/include/fatrat` for the desktop version, `/usr/include/fatrat-nogui` for the headless one |
| `fatrat-unpack`, `fatrat-nogui-unpack` | the [fatrat-unpack](https://github.com/LubosD/fatrat-unpack) plugin (RAR, 7z, ZIP, split files), built for `fatrat` and `fatrat-nogui` respectively |

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

### The unpack plugin

A native plugin has to be built against the headers of the exact FatRat it is loaded into, and the desktop
and headless flavours differ. `fatrat-unpack` is built in `~/Projects/fatrat-unpack` by its own
`packaging/deb/build.sh`, from the `fatrat-dev` package that the FatRat build leaves in `out/SERIES/`, and
the plugin packages are written to the same directory. So build FatRat first, then the plugin:

    packaging/deb/build.sh
    ~/Projects/fatrat-unpack/packaging/deb/build.sh      # FATRAT_OUT=... if FatRat is elsewhere

The plugin packages depend on exactly the FatRat version they were built with, so publish them together
with it. The two plugin packages install the same file and conflict with each other: with both `fatrat` and
`fatrat-nogui` installed, use `fatrat-unpack` (the desktop version is what the systemd units start).

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

## Running as a service

`fatrat-common` ships two systemd units named `fatrat.service`. Neither is enabled on install. They work
with either package: they run `fatrat --nogui` if `fatrat` is installed, otherwise `fatrat-nogui`
(`/usr/libexec/fatrat/fatrat-server` picks). The desktop build comes first because it registers on D-Bus,
so the desktop app started in the same session finds the running service instead of starting a second
instance on the same data. For the same reason the user unit exits without error if the desktop app is
already running.

- **User unit**: `systemctl --user enable --now fatrat`. It uses the user's normal settings and serves the
  REST API on `$XDG_RUNTIME_DIR/fatrat/rest.sock`, where `fatrat-tui` finds it. Run
  `sudo loginctl enable-linger $USER` to keep it running after logout.
- **System unit**: `sudo systemctl enable --now fatrat`. It runs as the `fatrat` user (created by
  sysusers) with its home in `/var/lib/fatrat`. Turn on the web interface with a password of your
  own, then restart it:
  `sudo -u fatrat fatrat-conf -w remote/enable=true remote/password=SECRET`. Or use the terminal UI:
  `sudo -u fatrat fatrat-tui --socket /run/fatrat/rest.sock` (`fatrat-tui` is in `fatrat-nogui`).
