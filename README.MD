# FatRat

FatRat is an open source download and upload manager, written in C++ and built on Qt 6.
It downloads over HTTP(S) and FTP(S), speaks BitTorrent, organises transfers in queues, and can be controlled from a desktop window, a web browser, a terminal or scripts, all at the same time.

**Supported platforms:** Linux, BSD and macOS.

Licensed under the GNU GPL version 3. Copyright © 2006-2015 Luboš Doležel. Credits for third-party graphics, libraries and web interface components are in the original `README` file.

## What it can do

**Transfers**

- HTTP, HTTPS, FTP and FTPS downloads, with segmented downloading (several connections per file) and mirror lists.
- BitTorrent through libtorrent-rasterbar: .torrent files and magnet links, per-file priorities, creating torrents.
- Metalink files.
- FTP uploads.
- Extensions written in Java, for sites that need special handling (login, link extraction, captchas).
- Per-transfer, per-queue and global speed limits.

**Organising**

- Queues, each with its own default directory, "move when finished" directory, speed limits and limits on the number of active transfers.
- Transfers can be paused, resumed, force-started, reordered, moved between queues and deleted with or without their data.
- A scheduler for timed actions, a clipboard monitor that offers to download copied links, and a drop box.
- RSS feeds with download rules based on regular expressions, including TV episode tracking (season/episode or date numbering, repack and trailer handling) so an episode is fetched only once.

**Ways to use it**

| Front end | Build option | Notes |
| --- | --- | --- |
| Desktop application (Qt Widgets) | `WITH_GUI` (default on) | Tray icon, all settings. |
| Headless daemon (`fatrat-nogui`) | `WITH_GUI=OFF` | For servers and seedboxes. |
| Web interface | `WITH_WEBINTERFACE` | Angular application served by FatRat itself. |
| REST API | `WITH_WEBINTERFACE` | JSON API under `/api/v1`, described in [docs/api/openapi.yaml](docs/api/openapi.yaml). |
| Terminal UI (`fatrat-tui`) | `WITH_TUI` | Keyboard and mouse driven ncurses client, see below. |
| D-Bus | `WITH_DBUS` (default on) | Add transfers from scripts and other programs, single-instance check. |
| Command line | always | `fatrat "http://example.org/file.iso"` opens the new transfer dialog. |

## Building

FatRat uses CMake (3.16 or newer), needs Qt 6.5 or newer and a C++17 compiler.

### Dependencies

| Feature | Option | Needs |
| --- | --- | --- |
| Core | | QtCore, QtNetwork, QtXml, OpenSSL |
| Desktop UI | `WITH_GUI` | QtWidgets, QtSvg |
| D-Bus | `WITH_DBUS` | QtDBus |
| HTTP/FTP | `WITH_CURL` | libcurl |
| BitTorrent | `WITH_BITTORRENT` | libtorrent-rasterbar 2.0 or newer, Boost (date_time) |
| Web interface | `WITH_WEBINTERFACE` | Node.js and npm at build time (the Angular front end in `webui/` is built and installed into `data/remote`) |
| Java extensions | `WITH_JPLUGINS` | A JRE, and a JDK to build |
| Terminal UI | `WITH_TUI` | ncurses with wide character support (`ncursesw`) |
| Translations | `WITH_NLS` | |
| Built-in help | `WITH_DOCUMENTATION` | QtHelp, requires `WITH_GUI` |
| Unit tests | `WITH_TESTS` | QtTest |

`WITH_EVERYTHING` switches on the optional features at once.

### Examples

A desktop build with BitTorrent and the web interface:

```sh
cmake -B build -DWITH_BITTORRENT=ON -DWITH_CURL=ON -DWITH_WEBINTERFACE=ON -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build -j
sudo cmake --install build
```

A headless server (installs `fatrat-nogui`) with the terminal client:

```sh
cmake -B build -DWITH_GUI=OFF -DWITH_DBUS=OFF -DWITH_BITTORRENT=ON -DWITH_CURL=ON \
      -DWITH_WEBINTERFACE=ON -DWITH_TUI=ON
cmake --build build -j
```

Only the terminal client, for use against a FatRat running elsewhere (`WITH_TUI` does not need the rest of the program to be buildable):

```sh
cmake -B build -DWITH_TUI=ON -DWITH_GUI=OFF -DWITH_DBUS=OFF
cmake --build build --target fatrat-tui
```

Running the tests:

```sh
cmake -B build -DWITH_TESTS=ON -DWITH_TUI=ON ...
cmake --build build
ctest --test-dir build
```

`tests/rest-smoke.sh` and `tests/tui-smoke.sh` are end-to-end checks against a real headless FatRat; their headers explain the environment variables they need.

More details, and the list of installed files, are in [INSTALL](INSTALL).

## Using FatRat

### Desktop and headless

```sh
fatrat                       # desktop application
fatrat --hidden              # start into the tray icon
fatrat --nogui               # no window at all (also what fatrat-nogui does)
fatrat --daemon --pidfile /run/fatrat.pid --user fatrat   # run as a daemon
fatrat -c /path/to/fatrat.conf   # use another settings file
fatrat --force               # skip the "already running" check
```

Only one FatRat can run per session (it checks over D-Bus). `fatrat --help` lists every option; there is also a man page, `doc/fatrat.1`.

Files:

- Settings: `~/.config/Dolezel/fatrat.conf`
- Queues and transfers: `~/.local/share/fatrat/queues.xml`
- Other user data (extensions, .torrent files): `~/.local/share/fatrat/`

### Web interface and REST API

Build with `WITH_WEBINTERFACE`, then enable the web interface in the settings (port, optional password, optional HTTPS with a one-click self-signed certificate). Open `http://host:port/` in a browser. Authentication is HTTP Basic with any user name and the configured password.

The same server exposes the JSON API the web interface uses:

```sh
curl -u x:PASSWORD http://localhost:PORT/api/v1/queues
curl -u x:PASSWORD -X POST http://localhost:PORT/api/v1/queues/QUEUE-UUID/transfers \
     -H 'Content-Type: application/json' -d '{"urls":["http://example.org/file.iso"]}'
```

The API covers queues (create, edit, delete, reorder), transfers (add, pause, resume, delete, reorder, move between queues, torrent file priorities), RSS feeds and rules, and settings; the logs are served at `/log`. See [docs/api/openapi.yaml](docs/api/openapi.yaml).

For local tools there is also a unix socket without a password, protected by file permissions:

```sh
fatrat --nogui --rest-socket "$XDG_RUNTIME_DIR/fatrat/rest.sock"
curl --unix-socket "$XDG_RUNTIME_DIR/fatrat/rest.sock" http://localhost/api/v1/stats
```

### Terminal UI

`fatrat-tui` is a client for the REST API that runs in any terminal, over SSH or in tmux. Everything can be done with the keyboard, the bottom line always shows the keys that apply, and the mouse works as well.

`fatrat-tui` decides how to reach FatRat, tells you what it chose (in the title bar and on the start-up screen), and `fatrat-tui --which` prints the decision and the reasons without starting anything. With `--mode auto`, the default, it tries in this order:

1. **A server already running on the unix socket** (`$XDG_RUNTIME_DIR/fatrat/rest.sock`), for example one that an earlier `fatrat-tui` left running. No password.
2. **A FatRat that is already running on this machine.** It reads the FatRat settings file (`~/.config/Dolezel/fatrat.conf`, or the file given with `--config`), and if the web interface is enabled there and its port answers, it connects to `http://127.0.0.1:PORT` (HTTPS if the settings say so) with the password from the settings. This is the way to use the TUI next to the desktop application or a daemon you started yourself.
3. **A private server of its own**, started in the background on the unix socket, using your normal FatRat settings and transfers. This needs a FatRat built with `WITH_WEBINTERFACE`, and cannot work while another FatRat is running (only one runs per session), which is why step 2 comes first.

You can pin the choice instead of relying on detection:

```sh
fatrat-tui --mode local              # only a running FatRat found through its settings; never starts anything
fatrat-tui --mode socket             # only the unix socket: use the server there, or start one
fatrat-tui --no-start                # auto, but fail instead of starting a server
fatrat-tui --url http://host:5010    # any FatRat; asks for the password
fatrat-tui --url https://host:5010 --password-file ~/.fatrat-pw
fatrat-tui --which                   # explain what would be used, then exit
fatrat-tui --stop                    # stop a server that fatrat-tui started and left running
```

When the TUI started the private server and transfers are active, quitting asks whether to keep the server running or stop it.

Passwords come from `--password-file` (the file must not be readable by others), then `FATRAT_PASSWORD`, then, for a local FatRat, the settings file, and otherwise a prompt (`--password-prompt` always asks). They are never stored by the TUI. For HTTPS, an untrusted certificate shows its fingerprint and can be trusted for the session or remembered by fingerprint (`--insecure` turns the check off).

Other options: `--no-mouse`, `--ascii` (plain ASCII drawing), `--socket PATH`, `--server-bin PATH`, `--config FILE`. `fatrat-tui --help` lists them all.

**Keys** (press `?` in the program for the complete list)

| Where | Keys |
| --- | --- |
| Everywhere | `1` `2` `3` tabs (Transfers, RSS, Log), `Tab` switch pane, arrows or `j` `k`, `/` filter, `O` or `F2` settings, `F3` global speed limit, `?` help, `q` quit |
| Transfers | `R` resume, `F` force, `P` pause, `N` new, `D` delete, `E` edit, `M` move then `T` `U` `D` `B`, `[` `]` move up and down, `S` send to another queue, `L` speed limit, `Space` mark, `A` mark all, `Enter` details |
| Queues | `N` new, `E` edit, `D` delete (moving or deleting its transfers), `[` `]` reorder, `Enter` open |
| Torrent files | `+` `-` priority, `S` skip, `A` all, `Enter` apply |
| RSS | `N` `E` `D` feeds and rules, `T` test a regular expression, `W` downloaded episodes, `R` refresh now |
| Log | `F` follow, `/` search, `n` next match, `T` log of the selected transfer |
| Settings | `↑` `↓` pick a page, `Enter` edit it, `Tab` next field (past the last one: back to the pages), `PgUp` `PgDn` other page, `Enter` save, `S` save from the page list, `Esc` back or cancel, `Ctrl-G` generate a certificate (web interface page) |

**Settings** (`O`, `F2`, or click "Settings" at the top right) has the same pages as the web interface: BitTorrent, RSS (switch and interval; feeds and rules are on the RSS tab) and the web interface. Edits on all pages are kept until you save, then sent in one request. The global speed limit is shown at the right end of the status line; click it or press `F3` to change it.

**Mouse**: click to select, double-click to open details, right-click for a context menu, Ctrl-click to mark, Shift-click to mark a range, wheel to scroll, and drag a transfer to another row to reorder it or onto a queue to send it there. Every key hint in the bottom bar is clickable. Hold Shift to select text with the terminal's own selection.

### Docker

The [Dockerfile](Dockerfile) builds a headless FatRat on Ubuntu 26.04 with HTTP(S)/FTP(S) (libcurl), BitTorrent, the web interface and the terminal UI. Ubuntu 24.04 does not work as the base, as it only has Qt 6.4 and FatRat needs 6.5.

```sh
docker build -t fatrat .

# headless server (no terminal attached)
docker run -d --name fatrat -p 2233:2233 -p 6881:6881 -p 6881:6881/udp \
       -e FATRAT_PASSWORD=secret -v fatrat-config:/config -v ~/Downloads:/downloads fatrat

# terminal UI (a terminal is attached): the TUI runs against a server inside the container
docker run -it --rm -p 2233:2233 -v fatrat-config:/config -v ~/Downloads:/downloads fatrat

# terminal UI on the already running headless container
docker exec -it fatrat fatrat-tui
```

[docker/entrypoint.sh](docker/entrypoint.sh) picks the mode: with a terminal on stdin and stdout it starts the server in the background and shows `fatrat-tui` (leaving the TUI stops the container), otherwise it runs `fatrat-nogui` as the main process, so `docker stop` lets it save its state. The web interface is served in both modes, and a unix socket for the TUI is opened in both as well.

| Variable | Meaning |
| --- | --- |
| `FATRAT_MODE` | `auto` (default), `tui` or `headless` |
| `FATRAT_PORT` | Port of the web interface, 2233 by default |
| `FATRAT_PASSWORD` | Web interface password (any user name). Without it, the password from an earlier run is kept, or a random one is generated and printed in the container log; FatRat's built-in default password is never used |

Extra arguments after the image name are passed to `fatrat-nogui` (for example `--no-java`). `/config` holds the settings (`fatrat.conf`), queues and `.torrent` files, and `/downloads` is the working directory. The container runs as an unprivileged user (uid 10001), so those mounts must be writable by it. Port 6881 (TCP and UDP) is for incoming BitTorrent connections and has to be published to be reachable.

### D-Bus

The interface `info.dolezel.fatrat` on path `/` accepts links from scripts:

```sh
qdbus info.dolezel.fatrat / addTransfers "http://example.org/file.iso"
qdbus info.dolezel.fatrat / getQueues
```

See `doc/dbus.html` for `addTransfersNonInteractive` and the other methods.

## Source layout

| Path | Contents |
| --- | --- |
| `src/` | The program: queues, transfers, engines (`src/engines`), RSS (`src/rss`), the HTTP server and REST API (`src/remote`), Java bridge (`src/java`) |
| `src/tui/` | The terminal client |
| `webui/` | The Angular web interface (sources; the build output goes to `data/remote`) |
| `docs/api/` | OpenAPI description of the REST API |
| `doc/` | End-user documentation and the man page |
| `autotests/`, `tests/` | Unit tests and end-to-end scripts |
| `data/` | Default configuration, mirror and tracker lists |
| `locale/` | Translations |
