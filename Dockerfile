# FatRat for headless use: HTTP(S)/FTP(S) (libcurl), BitTorrent, the web interface, the terminal UI and Python extensions.
#
#   docker build -t fatrat .
#   docker run -d --name fatrat -p 2233:2233 -e FATRAT_PASSWORD=secret -v fatrat-config:/config -v ~/Downloads:/downloads fatrat
#   docker run -it --rm -p 2233:2233 -v fatrat-config:/config -v ~/Downloads:/downloads fatrat    # TUI (a TTY is attached)
#   docker exec -it fatrat fatrat-tui                                                              # TUI on a running container
#
# See docker/entrypoint.sh for the environment variables.

# Qt 6.5 or newer is required, which rules out Ubuntu 24.04 (Qt 6.4).
ARG UBUNTU_VERSION=26.04

# ---------------------------------------------------------------------------------------------------
FROM ubuntu:${UBUNTU_VERSION} AS build
ARG DEBIAN_FRONTEND=noninteractive
# Node.js for the Angular web UI (the distribution's version is too old for Angular 22)
ARG NODE_VERSION=24.15.0

RUN apt-get update && apt-get install -y --no-install-recommends \
		build-essential cmake ninja-build pkg-config ca-certificates curl xz-utils \
		qt6-base-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools \
		libcurl4-openssl-dev libssl-dev libtorrent-rasterbar-dev libboost-dev libboost-date-time-dev \
		libncurses-dev python3-dev pybind11-dev \
	&& rm -rf /var/lib/apt/lists/*

RUN set -eux; \
	case "$(dpkg --print-architecture)" in amd64) a=x64;; arm64) a=arm64;; *) echo "unsupported architecture" >&2; exit 1;; esac; \
	curl -fsSL "https://nodejs.org/dist/v${NODE_VERSION}/node-v${NODE_VERSION}-linux-${a}.tar.xz" | tar -xJ -C /usr/local --strip-components=1; \
	node --version; npm --version

WORKDIR /src
COPY . .

# WITH_GUI=OFF installs bin/fatrat-nogui; the D-Bus single-instance check is not needed in a container
RUN cmake -S . -B /build -G Ninja \
		-DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
		-DWITH_GUI=OFF -DWITH_DBUS=OFF -DWITH_CURL=ON -DWITH_BITTORRENT=ON -DWITH_WEBINTERFACE=ON -DWITH_TUI=ON -DWITH_PYPLUGINS=ON \
	&& cmake --build /build \
	&& DESTDIR=/stage cmake --install /build \
	&& ls -l /stage/usr/bin

# ---------------------------------------------------------------------------------------------------
FROM ubuntu:${UBUNTU_VERSION}
ARG DEBIAN_FRONTEND=noninteractive

# Python extensions need the interpreter (its standard library, and pip for plugin dependencies) and git
# (plugin repositories are cloned with it).
# Runtime libraries are found from what the binaries link against, so package names (which change with
# Ubuntu releases) do not have to be spelled out. 'xz' etc. are not needed, ncurses brings its terminfo.
COPY --from=build /stage/ /
RUN apt-get update \
	&& apt-get install -y --no-install-recommends ca-certificates ncurses-term libqt6network6 libqt6xml6 libqt6sql6 \
		python3 python3-pip git \
	&& missing=$(ldd /usr/bin/fatrat-nogui /usr/bin/fatrat-tui | awk '/not found/ {print $1}' | sort -u); \
	if [ -n "$missing" ]; then \
		echo "Resolving missing libraries: $missing" >&2; \
		apt-get install -y --no-install-recommends apt-file >/dev/null && apt-file update >/dev/null; \
		for lib in $missing; do apt-file search --regexp "/${lib}\$" | cut -d: -f1 | head -n1; done | sort -u | xargs -r apt-get install -y --no-install-recommends; \
		apt-get purge -y apt-file; apt-get autoremove -y; \
	fi \
	&& ! ldd /usr/bin/fatrat-nogui /usr/bin/fatrat-tui | grep "not found" \
	&& rm -rf /var/lib/apt/lists/* /var/cache/apt-file

RUN useradd --uid 10001 --user-group --home-dir /config --no-create-home --shell /usr/sbin/nologin fatrat \
	&& mkdir -p /config /downloads && chown fatrat:fatrat /config /downloads
COPY docker/entrypoint.sh /usr/local/bin/fatrat-entrypoint
RUN chmod 0755 /usr/local/bin/fatrat-entrypoint

ENV HOME=/config \
	LANG=C.UTF-8 \
	XDG_RUNTIME_DIR=/tmp/runtime \
	FATRAT_PORT=2233
USER fatrat
WORKDIR /downloads
VOLUME ["/config", "/downloads"]
EXPOSE 2233 6881 6881/udp

ENTRYPOINT ["/usr/local/bin/fatrat-entrypoint"]
