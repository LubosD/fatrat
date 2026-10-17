#!/bin/bash
# Builds the FatRat .deb packages (fatrat, fatrat-nogui, fatrat-common) for Ubuntu in Docker.
#
#   packaging/deb/build.sh [SERIES...]      (default: all series below)
#
# The packages end up in packaging/deb/out/SERIES/. The source is the git working tree: tracked files
# (including uncommitted changes) plus new files in debian/ and packaging/.
# Publish them with packaging/deb/publish.sh.
#
# Environment:
#   DEB_REVISION  Debian revision (default 1); bump it to rebuild the same upstream version
#   JOBS          parallel build jobs (default: nproc)
set -euo pipefail

# Codename -> release. Ubuntu 24.04 and older have Qt < 6.5.
declare -A RELEASES=([stonking]=26.10 [resolute]=26.04)

TOP=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
cd "$TOP"
OUT=$TOP/packaging/deb/out
DEB_REVISION=${DEB_REVISION:-1}
JOBS=${JOBS:-$(nproc)}
VERSION=$(sed -n 's/^#define VERSION "\(.*\)"/\1/p' src/fatrat.h)
[ -n "$VERSION" ] || { echo "Cannot find VERSION in src/fatrat.h" >&2; exit 1; }

if [ $# -eq 0 ]; then set -- "${!RELEASES[@]}"; fi
for series; do
	[ -n "${RELEASES[$series]:-}" ] || { echo "Unknown series $series (known: ${!RELEASES[*]})" >&2; exit 1; }
done

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# Source snapshot; files deleted in the working tree are skipped
{ git ls-files -z --cached; git ls-files -z --others --exclude-standard -- debian packaging; } \
	| while IFS= read -r -d '' f; do [ -e "$f" ] && printf '%s\0' "$f"; done \
	| tar --null -T - -czf "$WORK/source.tar.gz"
mkdir "$WORK/ctx" "$WORK/ctx/debian"
cp debian/control "$WORK/ctx/debian/"

for series in "$@"; do
	release=${RELEASES[$series]}
	debver="$VERSION-$DEB_REVISION~ubuntu$release.1"
	image="fatrat-deb-build:$series"
	echo "=== $series ($release): fatrat $debver"

	docker build -t "$image" --build-arg UBUNTU_VERSION="$release" -f packaging/deb/Dockerfile "$WORK/ctx"

	rm -rf "$OUT/$series"
	mkdir -p "$OUT/$series"
	docker run --rm \
		-v "$WORK/source.tar.gz:/source.tar.gz:ro" \
		-v "$OUT/$series:/out" \
		-v fatrat-deb-npm-cache:/root/.npm \
		-e SERIES="$series" -e DEBVER="$debver" -e JOBS="$JOBS" -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" \
		"$image" bash -euo pipefail -c '
			mkdir -p /build/fatrat && cd /build/fatrat
			tar -xzf /source.tar.gz

			# Version and distribution for this series
			printf "fatrat (%s) %s; urgency=medium\n\n  * Build for Ubuntu %s.\n\n -- %s  %s\n" \
				"$DEBVER" "$SERIES" "$SERIES" "$(dpkg-parsechangelog -S Maintainer)" "$(date -R)" > debian/changelog

			# The web UI needs npm and network access, so it is built here and not in debian/rules
			(cd webui && npm ci --no-audit --no-fund && npm run build -- --configuration production)
			rm -rf data/remote && cp -r webui/dist/webui/browser data/remote
			rm -rf webui/node_modules webui/.angular

			DEB_BUILD_OPTIONS="parallel=$JOBS" dpkg-buildpackage -b -us -uc
			lintian ../*.changes || true
			cp ../*.deb ../*.changes ../*.buildinfo /out/
			chown -R "$HOST_UID:$HOST_GID" /out
		'
	ls -l "$OUT/$series"
done
