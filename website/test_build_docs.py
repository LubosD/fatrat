import importlib.util
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("build_docs", HERE / "build-docs.py")
build_docs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build_docs)

QHP = """<?xml version="1.0" encoding="UTF-8"?>
<QtHelpProject version="1.0">
	<filterSection>
		<toc>
			<section title="FatRat" ref="index.html">
				<section title="RSS" ref="rss/index.html" />
			</section>
		</toc>
	</filterSection>
</QtHelpProject>
"""

def page(title, body):
	return f"<html><head><title>{title}</title></head><body>\n{body}\n</body></html>"


class BuildDocsTest(unittest.TestCase):
	def setUp(self):
		self.tmp = tempfile.TemporaryDirectory()
		root = Path(self.tmp.name)
		self.doc, self.out = root / "doc", root / "out"
		(self.doc / "rss").mkdir(parents=True)
		(self.doc / "fatrat.qhp").write_text(QHP)
		(self.doc / "index.html").write_text(page("Welcome", '<h1>Welcome</h1><p><a href="rss/index.html">RSS</a></p>'))
		(self.doc / "rss" / "index.html").write_text(page("RSS", '<h1>RSS</h1><img src="rss.png" alt="x"><a href="../index.html#top">back</a>'))
		(self.doc / "rss" / "rss.png").write_bytes(b"png")

	def tearDown(self):
		self.tmp.cleanup()

	def test_wraps_pages_and_copies_images(self):
		build_docs.build(self.doc, self.out)
		index = (self.out / "index.html").read_text()
		rss = (self.out / "rss" / "index.html").read_text()

		self.assertIn("<title>Welcome · FatRat documentation</title>", index)
		self.assertIn('href="../style.css"', index)
		self.assertIn('href="../../style.css"', rss)
		self.assertIn("<h1>RSS</h1>", rss)
		self.assertTrue((self.out / "rss" / "rss.png").exists())

	def test_sidebar_marks_current_page(self):
		build_docs.build(self.doc, self.out)
		rss = (self.out / "rss" / "index.html").read_text()
		self.assertIn('<a href="index.html" aria-current="page">RSS</a>', rss)
		self.assertIn('<a href="../index.html">FatRat</a>', rss)

	def test_rebuild_removes_stale_output(self):
		self.out.mkdir()
		(self.out / "old.html").write_text("x")
		build_docs.build(self.doc, self.out)
		self.assertFalse((self.out / "old.html").exists())

	def test_missing_page_fails(self):
		(self.doc / "rss" / "index.html").unlink()
		with self.assertRaisesRegex(build_docs.DocsError, "rss/index.html"):
			build_docs.build(self.doc, self.out)

	def test_broken_link_fails(self):
		(self.doc / "index.html").write_text(page("Welcome", '<a href="nope.html">x</a>'))
		with self.assertRaisesRegex(build_docs.DocsError, "nope.html"):
			build_docs.build(self.doc, self.out)

	def test_missing_image_fails(self):
		(self.doc / "rss" / "rss.png").unlink()
		with self.assertRaisesRegex(build_docs.DocsError, "rss.png"):
			build_docs.build(self.doc, self.out)

	def test_external_links_are_not_checked(self):
		(self.doc / "index.html").write_text(page("Welcome", '<a href="https://example.com/x.html">x</a><a href="mailto:a@b">m</a>'))
		build_docs.build(self.doc, self.out)

	def test_title_is_not_escaped_twice(self):
		(self.doc / "index.html").write_text(page("Q&amp;A", "<p>x</p>"))
		build_docs.build(self.doc, self.out)
		self.assertIn("<title>Q&amp;A · FatRat documentation</title>", (self.out / "index.html").read_text())

	def test_uppercase_page_link_is_not_an_image(self):
		(self.doc / "index.html").write_text(page("Welcome", '<a href="rss/INDEX.HTML">x</a>'))
		(self.doc / "rss" / "INDEX.HTML").write_text("x")
		with self.assertRaisesRegex(build_docs.DocsError, "INDEX.HTML"):
			build_docs.build(self.doc, self.out)
		self.assertFalse((self.out / "rss" / "INDEX.HTML").exists())

	def test_image_outside_doc_fails(self):
		(self.doc.parent / "x.png").write_bytes(b"png")
		(self.doc / "index.html").write_text(page("Welcome", '<img src="../x.png">'))
		with self.assertRaisesRegex(build_docs.DocsError, r"index\.html.*\.\./x\.png"):
			build_docs.build(self.doc, self.out)

	def test_link_outside_doc_fails(self):
		(self.doc / "index.html").write_text(page("Welcome", '<a href="/etc/x.html">x</a>'))
		with self.assertRaisesRegex(build_docs.DocsError, "/etc/x.html"):
			build_docs.build(self.doc, self.out)

	def test_link_to_page_outside_toc_fails(self):
		(self.doc / "extra.html").write_text(page("Extra", "x"))
		(self.doc / "index.html").write_text(page("Welcome", '<a href="extra.html">x</a>'))
		with self.assertRaisesRegex(build_docs.DocsError, "extra.html.*table of contents"):
			build_docs.build(self.doc, self.out)

	def test_page_without_body_fails(self):
		(self.doc / "index.html").write_text("<html><head><title>T</title></head></html>")
		with self.assertRaisesRegex(build_docs.DocsError, "index.html.*<body>"):
			build_docs.build(self.doc, self.out)

	def test_failed_build_leaves_out_untouched(self):
		self.out.mkdir()
		(self.out / "old.html").write_text("keep")
		(self.doc / "index.html").write_text(page("Welcome", '<a href="nope.html">x</a>'))
		with self.assertRaises(build_docs.DocsError):
			build_docs.build(self.doc, self.out)
		self.assertEqual((self.out / "old.html").read_text(), "keep")

	def test_nested_page_links_up_in_sidebar(self):
		(self.doc / "a").mkdir()
		(self.doc / "fatrat.qhp").write_text(QHP.replace(
			'<section title="RSS" ref="rss/index.html" />',
			'<section title="A" ref="a/index.html"><section title="B" ref="a/b.html" /></section>'))
		for ref in ("index.html", "a/index.html", "a/b.html"):
			(self.doc / ref).write_text(page("T", "<p>x</p>"))
		build_docs.build(self.doc, self.out)
		b = (self.out / "a" / "b.html").read_text()
		self.assertIn('<a href="../index.html">FatRat</a>', b)
		self.assertIn('<a href="index.html">A</a>', b)
		self.assertIn('<a href="b.html" aria-current="page">B</a>', b)
		self.assertIn('href="../../style.css"', b)


if __name__ == "__main__":
	unittest.main()
