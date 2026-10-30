#!/usr/bin/env python3
"""Builds the website's /docs/ section from the Qt Help pages in doc/.

doc/fatrat.qhp is the table of contents. Each listed page's <body> is wrapped in the website's page shell
with a sidebar; images the pages use are copied along. Any TOC entry, page link or image that does not
exist stops the build, so broken docs are never published.

	website/build-docs.py [DOC_DIR [OUT_DIR]]
"""
import html
import os
import re
import shutil
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

TOP = Path(__file__).resolve().parent.parent


class DocsError(Exception):
	pass


PAGE = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{title} · FatRat documentation</title>
<meta name="theme-color" content="#1c160e">
<link rel="icon" href="{root}img/favicon.png" type="image/png">
<link rel="stylesheet" href="{root}style.css">
</head>
<body>

<header class="top">
	<div class="wrap">
		<a class="brand" href="{root}"><img src="{root}img/logo.svg" alt="" width="28" height="34">FatRat</a>
		<nav>
			<a href="{root}docs/">Docs</a>
			<a href="https://github.com/LubosD/fatrat">GitHub</a>
		</nav>
	</div>
</header>

<div class="wrap docs">
	<nav class="docs-nav" aria-label="Documentation">
		<details open>
			<summary>Contents</summary>
{toc}
		</details>
	</nav>
	<article class="docs-page">
{body}
	</article>
</div>

<script>
	// The contents are folded on narrow screens, where they sit above the page, and open beside it otherwise.
	const mq = matchMedia('(max-width: 820px)'), details = document.querySelector('.docs-nav details');
	details.open = !mq.matches;
	mq.addEventListener('change', () => details.open = !mq.matches);
</script>
</body>
</html>
"""

LINK_RE = re.compile(r'\b(href|src)="([^"]*)"', re.IGNORECASE)


def read_toc(doc):
	"""The TOC as nested (title, ref, children) tuples."""
	root = ET.parse(doc / "fatrat.qhp").getroot()

	def walk(el):
		return (el.get("title"), el.get("ref"), [walk(c) for c in el.findall("section")])

	return [walk(s) for s in root.findall("filterSection/toc/section")]


def flatten(toc):
	for title, ref, children in toc:
		yield ref
		yield from flatten(children)


def toc_html(toc, current, depth=3):
	here = os.path.dirname(current)
	pad = "\t" * depth
	out = [pad + "<ul>"]
	for title, ref, children in toc:
		href = os.path.relpath(ref, here or ".")
		mark = ' aria-current="page"' if ref == current else ""
		out.append(f'{pad}\t<li><a href="{html.escape(href)}"{mark}>{html.escape(title)}</a>')
		if children:
			out.append(toc_html(children, current, depth + 2))
		out.append(pad + "\t</li>")
	out.append(pad + "</ul>")
	return "\n".join(out)


def parse_page(path):
	text = path.read_text(encoding="utf-8")
	title = re.search(r"<title>(.*?)</title>", text, re.IGNORECASE | re.DOTALL)
	body = re.search(r"<body[^>]*>(.*)</body>", text, re.IGNORECASE | re.DOTALL)
	if not title or not body:
		raise DocsError(f"{path}: needs a <title> and a <body>")
	return title.group(1).strip(), body.group(1).strip()


def check_links(doc, ref, body, pages):
	"""Returns the images the page uses, relative to doc/."""
	images = set()
	here = Path(ref).parent
	for attr, target in LINK_RE.findall(body):
		target = target.split("#", 1)[0]
		if not target or ":" in target:  # an anchor in the same page, or another scheme
			continue
		resolved = os.path.normpath(here / target)
		if resolved.startswith("..") or os.path.isabs(resolved):
			raise DocsError(f"{ref}: {target} points outside doc/")
		if attr.lower() == "href" and resolved.lower().endswith((".html", ".htm")):
			if resolved not in pages:
				raise DocsError(f"{ref}: links to {target}, which is not in the table of contents")
		elif not (doc / resolved).is_file():
			raise DocsError(f"{ref}: {target} does not exist")
		else:
			images.add(resolved)
	return images


def build(doc, out):
	doc, out = Path(doc), Path(out)
	toc = read_toc(doc)
	pages = list(flatten(toc))

	for ref in pages:
		if not (doc / ref).is_file():
			raise DocsError(f"The table of contents lists {ref}, which does not exist")

	rendered, images = {}, set()
	for ref in pages:
		title, body = parse_page(doc / ref)
		images |= check_links(doc, ref, body, set(pages))
		root = "../" * (ref.count("/") + 1)
		rendered[ref] = PAGE.format(title=title, root=root, toc=toc_html(toc, ref), body=body)

	if out.exists():
		shutil.rmtree(out)
	for ref, text in rendered.items():
		(out / ref).parent.mkdir(parents=True, exist_ok=True)
		(out / ref).write_text(text, encoding="utf-8")
	for img in images:
		(out / img).parent.mkdir(parents=True, exist_ok=True)
		shutil.copyfile(doc / img, out / img)


def main(argv):
	doc = Path(argv[1]) if len(argv) > 1 else TOP / "doc"
	out = Path(argv[2]) if len(argv) > 2 else TOP / "website" / "docs"
	try:
		build(doc, out)
	except DocsError as e:
		print(f"build-docs: {e}", file=sys.stderr)
		return 1
	print(f"build-docs: wrote {out}")
	return 0


if __name__ == "__main__":
	sys.exit(main(sys.argv))
