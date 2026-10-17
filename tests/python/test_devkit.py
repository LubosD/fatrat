"""Tests for fatrat_devkit. Run: python3 -m unittest discover -s tests/python"""

import contextlib
import http.server
import io
import json
import os
import sys
import tempfile
import threading
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "data", "python"))

from fatrat_devkit import Host, MockTransport  # noqa: E402  (must precede fatrat)
from fatrat_devkit import cli, loader  # noqa: E402
import fatrat  # noqa: E402

EXAMPLES = os.path.join(ROOT, "examples", "plugin-repo")
FILE_BODY = b"hello from the fake host"


class _Handler(http.server.BaseHTTPRequestHandler):
    posted = []

    def log_message(self, *args):
        pass

    def _send(self, body, ctype="text/html", status=200):
        self.send_response(status)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/f/1":
            self._send(b'<form><input name="token" value="T0K"><img id="captcha" src="/captcha.png"></form>')
        elif self.path == "/captcha.png":
            self._send(b"\x89PNG....", "image/png")
        elif self.path == "/files/data.bin":
            self._send(FILE_BODY, "application/octet-stream")
        elif self.path == "/files/error.bin":
            self._send(b"<!DOCTYPE html><html>nope</html>")
        else:
            self._send(b"missing", status=404)

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        form = dict(p.split("=") for p in self.rfile.read(length).decode().split("&") if p)
        _Handler.posted.append(form)
        name = "error.bin" if form.get("captcha") == "bad" else "data.bin"
        host = self.headers["Host"]
        self._send(json.dumps({"link": "http://%s/files/%s" % (host, name), "name": name}).encode(),
                   "application/json")


class LiveServerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), _Handler)
        cls.port = cls.server.server_address[1]
        threading.Thread(target=cls.server.serve_forever, daemon=True).start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()

    def run_cli(self, *argv):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            code = cli.main(list(argv))
        return code, out.getvalue(), err.getvalue()

    def url(self, path="/f/1"):
        return "dummy://127.0.0.1:%d%s" % (self.port, path)

    def test_download_resolves_link(self):
        code, out, _ = self.run_cli("download", os.path.join(EXAMPLES, "dummy"), self.url(),
                                    "--captcha", "abc", "-q")
        self.assertEqual(code, 0, out)
        self.assertIn("/files/data.bin", out)
        self.assertEqual(_Handler.posted[-1], {"token": "T0K", "captcha": "abc"})

    def test_download_fetches_file_and_runs_final_check(self):
        with tempfile.TemporaryDirectory() as directory:
            code, out, _ = self.run_cli("download", os.path.join(EXAMPLES, "dummy"), self.url(),
                                        "--captcha", "abc", "--download", directory, "-q")
            self.assertEqual(code, 0, out)
            with open(os.path.join(directory, "data.bin"), "rb") as f:
                self.assertEqual(f.read(), FILE_BODY)

    def test_final_check_rejects_an_error_page(self):
        with tempfile.TemporaryDirectory() as directory:
            code, out, _ = self.run_cli("download", os.path.join(EXAMPLES, "dummy"), self.url(),
                                        "--captcha", "bad", "--download", directory, "-q")
            self.assertEqual(code, 1)
            self.assertIn("error page", out)

    def test_missing_captcha_front_end_fails_cleanly(self):
        code, out, _ = self.run_cli("download", os.path.join(EXAMPLES, "dummy"), self.url(), "-q")
        self.assertEqual(code, 1)
        self.assertIn("captcha", out)

    def test_http_error_is_a_failed_fetch(self):
        code, out, _ = self.run_cli("download", os.path.join(EXAMPLES, "dummy"), self.url("/nothing"), "-q")
        self.assertEqual(code, 1)
        self.assertIn("404", out)

    def test_json_output(self):
        code, out, _ = self.run_cli("download", os.path.join(EXAMPLES, "dummy"), self.url(),
                                    "--captcha", "abc", "--json", "-q")
        self.assertEqual(code, 0)
        data = json.loads(out)
        self.assertTrue(data["ok"])
        self.assertTrue(data["url"].endswith("/files/data.bin"))
        self.assertEqual(data["vars"]["token"], "T0K")


class MockedTest(unittest.TestCase):
    def descriptor(self, name):
        plugin = loader.load(os.path.join(EXAMPLES, "extras"))
        return next(d for d in plugin.descriptors if d.cls.__name__ == name)

    def test_search(self):
        host = Host(transport=MockTransport({
            "http://127.0.0.1:8099/search?q=cats": '{"hits": [{"name": "cat.avi", "url": "http://x/cat", "size": 5}]}'}))
        result = host.run_search(self.descriptor("DummySearch").cls, "cats")
        self.assertTrue(result.ok, result.error)
        self.assertEqual([h.name for h in result.results], ["cat.avi"])

    def test_account_reads_settings(self):
        host = Host(settings={"extras/api_key": "k"},
                    transport=MockTransport({"http://127.0.0.1:8099/account?key=k": '{"days_left": 3}'}))
        result = host.run_account_status(self.descriptor("DummyAccount").cls)
        self.assertEqual(result.account.balance, "3 days left")
        self.assertEqual(result.account.state, fatrat.AccountState.WARNING)

    def test_extractor(self):
        host = Host(transport=MockTransport({"http://h/dir": 'href="http://h/a" href="http://h/b"'}))
        result = host.run_extract(self.descriptor("DummyFolder").cls, "dummy-folder://h/dir")
        self.assertEqual(result.links, ["http://h/a", "http://h/b"])

    def test_upload_sends_multipart_with_the_file(self):
        seen = []

        def answer(request):
            seen.append(request)
            return '{"link": "http://x/u"}'

        host = Host(settings={"extras/api_key": "k"}, transport=MockTransport({"http://127.0.0.1:8099/upload": answer}))
        with tempfile.NamedTemporaryFile(suffix=".txt") as f:
            f.write(b"payload")
            f.flush()
            result = host.run_upload(self.descriptor("DummyUpload").cls, f.name)

        self.assertTrue(result.ok, result.error)
        self.assertEqual(result.link, "http://x/u")
        self.assertIn(b"payload", seen[0].body)
        self.assertIn(b'name="api_key"', seen[0].body)

    def test_plugin_that_never_starts_a_download_is_flagged(self):
        @fatrat.download_plugin(name="Lazy", url_pattern=r"lazy://.*")
        class Lazy(fatrat.DownloadPlugin):
            async def process_link(self, url):
                pass

        result = Host(transport=MockTransport()).run_download(Lazy, "lazy://x")
        self.assertFalse(result.ok)
        self.assertIn("start_download", result.error)

    def test_unexpected_exception_carries_a_traceback(self):
        @fatrat.download_plugin(name="Buggy", url_pattern=r"buggy://.*")
        class Buggy(fatrat.DownloadPlugin):
            async def process_link(self, url):
                {}["missing"]

        result = Host(transport=MockTransport()).run_download(Buggy, "buggy://x")
        self.assertFalse(result.ok)
        self.assertIn("KeyError", result.error)
        self.assertIn("Traceback", result.traceback)

    def test_wait_is_skipped_but_recorded(self):
        @fatrat.download_plugin(name="Waiter", url_pattern=r"waiter://.*")
        class Waiter(fatrat.DownloadPlugin):
            async def process_link(self, url):
                await self.wait(30)
                self.start_download("http://x/f")

        result = Host(transport=MockTransport()).run_download(Waiter, "waiter://x")
        self.assertTrue(result.ok)
        self.assertEqual(result.waits, [30])


class CheckTest(unittest.TestCase):
    def test_example_repository_is_clean(self):
        check = loader.load_repository(EXAMPLES)
        self.assertEqual(check.report.errors, [])
        for plugin in check.plugins:
            self.assertEqual(plugin.report.errors, [], plugin.name)

    def write_plugin(self, manifest, source):
        directory = tempfile.mkdtemp()
        self.addCleanup(__import__("shutil").rmtree, directory)
        with open(os.path.join(directory, "plugin.json"), "w") as f:
            json.dump(manifest, f)
        with open(os.path.join(directory, "broken.py"), "w") as f:
            f.write(source)
        return directory

    def test_reports_a_sync_process_link_and_a_bad_pattern(self):
        directory = self.write_plugin({"name": "broken", "api": "1", "entry": "broken.py"}, (
            "import fatrat\n"
            "@fatrat.download_plugin(name='B', url_pattern='(unclosed')\n"
            "class B(fatrat.DownloadPlugin):\n"
            "    def process_link(self, url):\n"
            "        pass\n"))
        errors = loader.load(directory).report.errors
        self.assertTrue(any("async def" in e for e in errors), errors)
        self.assertTrue(any("regular expression" in e for e in errors), errors)

    def test_reports_wrong_api_and_missing_entry(self):
        directory = self.write_plugin({"name": "broken", "api": "2", "entry": "broken.py"}, "")
        self.assertTrue(any("API" in e for e in loader.load(directory).report.errors))

        directory = self.write_plugin({"name": "broken", "entry": "gone.py"}, "")
        self.assertTrue(any("does not exist" in e for e in loader.load(directory).report.errors))


if __name__ == "__main__":
    unittest.main()
