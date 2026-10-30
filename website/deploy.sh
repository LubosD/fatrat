#!/bin/bash
# Publishes https://fatrat.dolezel.info/ on GitHub Pages: the website (this directory) together with the
# apt repository built by packaging/deb/publish.sh (packaging/deb/repo/, or REPO).
#
#   website/deploy.sh [COMMIT MESSAGE]
#
# Both go into the gh-pages branch, checked out as a worktree in packaging/deb/gh-pages/, which is then
# committed as a single parentless commit and force-pushed, so old .deb files never pile up in history. Without a local repository (no REPO/dists), the apt files already in gh-pages are
# left as they are and only the website is updated. The docs section (website/docs/) is generated from
# doc/ by build-docs.py first.
#
# The custom domain needs a DNS CNAME record fatrat.dolezel.info -> lubosd.github.io, and "Enforce HTTPS"
# ticked in the GitHub repository's Pages settings once the certificate has been issued.
set -euo pipefail

TOP=$(git -C "$(dirname "$0")" rev-parse --show-toplevel)
REPO=${REPO:-$TOP/packaging/deb/repo}
PAGES=$TOP/packaging/deb/gh-pages
DOMAIN=fatrat.dolezel.info
MESSAGE=${1:-Update website}

cd "$TOP"
python3 "$TOP/website/build-docs.py"   # fail fast, before $PAGES is touched
git worktree prune
if [ ! -d "$PAGES" ]; then
	if git show-ref -q --verify refs/heads/gh-pages; then
		git worktree add "$PAGES" gh-pages
	elif git fetch origin gh-pages 2>/dev/null; then
		git worktree add -B gh-pages "$PAGES" origin/gh-pages
	else
		git worktree add --orphan -b gh-pages "$PAGES"
	fi
fi

# The repository first, with --delete so packages dropped from it disappear; this also removes the
# website files, which are copied back right after. conf/ and db/ are reprepro's own state.
if [ -d "$REPO/dists" ]; then
	rsync -a --delete --exclude=.git --exclude=/conf --exclude=/db "$REPO/" "$PAGES/"
fi
rsync -a --exclude=deploy.sh --exclude=build-docs.py --exclude=test_build_docs.py --exclude=__pycache__ "$TOP/website/" "$PAGES/"
# Drop docs pages removed from doc/; no top-level --delete, as $PAGES also holds the apt repository.
rsync -a --delete "$TOP/website/docs/" "$PAGES/docs/"
echo "$DOMAIN" > "$PAGES/CNAME"
touch "$PAGES/.nojekyll"   # serve the files as they are, without Jekyll processing

git -C "$PAGES" add -A
if git -C "$PAGES" diff --cached --quiet; then
	echo "gh-pages is up to date"
else
	# One commit with no parent, force-pushed: the .deb files are binaries, and keeping every release in
	# history would bloat the repository. (Git LFS is no option, GitHub Pages does not serve LFS files.)
	COMMIT=$(git -C "$PAGES" commit-tree "$(git -C "$PAGES" write-tree)" -m "$MESSAGE")
	git -C "$PAGES" reset -q --soft "$COMMIT"
	git -C "$PAGES" push --force origin gh-pages
	echo "Pushed; GitHub Pages will serve it at https://$DOMAIN/ in a minute or two"
fi
