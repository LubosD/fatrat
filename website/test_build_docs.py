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


if __name__ == "__main__":
	unittest.main()
