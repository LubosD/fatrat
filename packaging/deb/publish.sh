#!/bin/bash
# Adds the packages built by build.sh to a signed apt repository in packaging/deb/repo/ (or REPO).
#
#   GPG_KEY=<key id> packaging/deb/publish.sh [--upload] [SERIES...]     (default: every series in out/)
#
# The repository is plain static files. --upload hands them to website/deploy.sh, which puts them into
# the gh-pages branch next to the website and pushes it, publishing both on GitHub Pages at
# https://fatrat.dolezel.info/. Keep packaging/deb/repo/ around: its db/ is what reprepro knows about
# the packages already published.
#
# reprepro runs in Docker; the Release files are signed here on the host with GPG_KEY, so the
# private key never enters a container. Users then add it with:
#
#   sudo curl -fsSLo /etc/apt/keyrings/fatrat.gpg https://fatrat.dolezel.info/fatrat.gpg
#   echo "deb [signed-by=/etc/apt/keyrings/fatrat.gpg] https://fatrat.dolezel.info $(. /etc/os-release; echo $VERSION_CODENAME) main" \
#       | sudo tee /etc/apt/sources.list.d/fatrat.list
set -euo pipefail

TOP=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
OUT=$TOP/packaging/deb/out
REPO=${REPO:-$TOP/packaging/deb/repo}
: "${GPG_KEY:?Set GPG_KEY to the id of the key that signs the repository}"
UPLOAD=
if [ "${1:-}" = --upload ]; then UPLOAD=1; shift; fi

if [ $# -eq 0 ]; then
	set -- $(cd "$OUT" && ls -d */ 2>/dev/null | tr -d /)
fi
[ $# -gt 0 ] || { echo "Nothing in $OUT, run build.sh first" >&2; exit 1; }

mkdir -p "$REPO/conf"
for series; do
	ls "$OUT/$series"/*.deb >/dev/null || exit 1
	grep -qx "Codename: $series" "$REPO/conf/distributions" 2>/dev/null && continue
	cat >> "$REPO/conf/distributions" <<-EOF
		Origin: FatRat
		Label: FatRat
		Codename: $series
		Architectures: amd64 arm64
		Components: main
		Description: FatRat download manager for Ubuntu $series

	EOF
done

docker run --rm -v "$REPO:/repo" -v "$OUT:/out:ro" -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" \
	ubuntu:26.10 bash -euo pipefail -c '
		apt-get update -qq >/dev/null && apt-get install -y -qq --no-install-recommends reprepro >/dev/null
		for series in '"$*"'; do
			reprepro -b /repo includedeb "$series" /out/"$series"/*.deb
		done
		reprepro -b /repo export
		chown -R "$HOST_UID:$HOST_GID" /repo
	'

gpg --batch --yes --export "$GPG_KEY" > "$REPO/fatrat.gpg"
# apt (sqv) rejects keys whose self-signatures use SHA-1, as older keys' do
if gpg --list-packets "$REPO/fatrat.gpg" | grep -q "digest algo 2,"; then
	echo "Key $GPG_KEY has SHA-1 self-signatures, which apt rejects. Re-sign them with:" >&2
	echo "  gpg --cert-digest-algo SHA512 --quick-set-expire $GPG_KEY 0" >&2
	echo "  gpg --cert-digest-algo SHA512 --quick-set-expire $GPG_KEY 0 '*'" >&2
	echo "(0 keeps the key without an expiry date; give a date or period instead to set one)" >&2
	exit 1
fi

for release in "$REPO"/dists/*/Release; do
	dir=$(dirname "$release")
	rm -f "$dir/InRelease" "$dir/Release.gpg"
	gpg --batch --yes --local-user "$GPG_KEY" --clearsign -o "$dir/InRelease" "$release"
	gpg --batch --yes --local-user "$GPG_KEY" --detach-sign --armor -o "$dir/Release.gpg" "$release"
done

echo "Repository in $REPO:"
ls "$REPO/dists"

[ -n "$UPLOAD" ] || exit 0

# GitHub Pages: the gh-pages branch holds the website and the published files (no conf/ and db/)
REPO=$REPO exec "$TOP/website/deploy.sh" "Update apt repository ($*)"
