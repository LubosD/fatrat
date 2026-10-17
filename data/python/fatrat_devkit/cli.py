"""Command line for :mod:`fatrat_devkit`: ``python3 -m fatrat_devkit``."""

from __future__ import annotations

import argparse
import dataclasses
import json
import os
import sys
import tempfile
from http.cookiejar import CookieJar
from typing import Any, Dict, List, Optional

from fatrat import registry

from . import loader
from .host import Host, LiveTransport, Request, Result

DESCRIPTION = """\
Run and check FatRat plugins from the command line, without FatRat.

PATH is a plugin directory (holding plugin.json) or a plugin repository
(holding fatrat-plugins.json). Pages are really fetched over the network;
the link a download plugin resolves is printed, and only fetched with
--download.
"""

EPILOG = """\
examples:
  fatrat_devkit check ./my-repo
  fatrat_devkit match ./my-repo 'https://example.com/f/abc123'
  fatrat_devkit download ./my-repo/example 'https://example.com/f/abc123' --captcha-prompt
  fatrat_devkit download ./my-repo 'https://example.com/f/abc123' -s example/user=me -s example/password=secret
  fatrat_devkit search ./my-repo/example 'some query'
"""


def _parse_value(text: str) -> Any:
    if text.lower() in ("true", "false"):
        return text.lower() == "true"
    return text


def _settings(args: argparse.Namespace) -> Dict[str, Any]:
    settings: Dict[str, Any] = {}

    if args.settings:
        with open(args.settings, encoding="utf-8") as f:
            settings.update(json.load(f))

    for item in args.set or []:
        key, sep, value = item.partition("=")
        if not sep:
            raise SystemExit("--set wants KEY=VALUE, got %r" % item)
        settings[key] = _parse_value(value)

    return settings


def _captcha_handler(args: argparse.Namespace):
    if args.captcha is not None:
        return lambda url: args.captcha

    if not args.captcha_prompt:
        return None

    def ask(url: str) -> str:
        try:
            image = LiveTransport().request(Request("GET", url, {"User-Agent": "FatRat/devkit"}), CookieJar())
            suffix = ".png" if image.content[:4] == b"\x89PNG" else ".jpg"
            with tempfile.NamedTemporaryFile(delete=False, suffix=suffix, prefix="fatrat-captcha-") as f:
                f.write(image.content)
            where = f.name
        except Exception as error:  # noqa: BLE001
            where = "(could not fetch it: %s)" % error

        print("\ncaptcha image: %s\nURL: %s" % (where, url), file=sys.stderr)
        return input("solution (empty to give up): ").strip()

    return ask


def _load(args: argparse.Namespace) -> List[loader.LoadedPlugin]:
    path = args.path

    if loader.is_repository(path):
        check = loader.load_repository(path)
        plugins = check.plugins
        problems = list(check.report.errors)
    elif os.path.isfile(os.path.join(path, "plugin.json")):
        plugins = [loader.load(path)]
        problems = []
    else:
        raise SystemExit("%s holds neither plugin.json nor fatrat-plugins.json" % path)

    for plugin in plugins:
        problems += ["%s: %s" % (plugin.name, e) for e in plugin.report.errors]

    if problems:
        print("The plugin does not load; fix this first (see the 'check' command):", file=sys.stderr)
        for problem in problems:
            print("  error: %s" % problem, file=sys.stderr)
        raise SystemExit(2)

    return plugins


def _host(args: argparse.Namespace) -> Host:
    return Host(settings=_settings(args), wait="real" if args.real_wait else "skip",
                captcha=_captcha_handler(args), echo=None if args.quiet else sys.stderr,
                timeout=args.timeout, transport=LiveTransport(timeout=args.timeout))


# -- check ---------------------------------------------------------------------

def _describe(descriptor: Any) -> str:
    extra = ""
    if descriptor.kind in (registry.DOWNLOAD, registry.EXTRACTOR):
        extra = "  matches " + (descriptor.url_pattern or "(acceptable() decides)")
    return "  %-14s %s%s" % (descriptor.kind, descriptor.cls.__name__, extra)


def _print_report(label: str, report: loader.Report) -> None:
    for message in report.errors:
        print("%s: error: %s" % (label, message))
    for message in report.warnings:
        print("%s: warning: %s" % (label, message))


def cmd_check(args: argparse.Namespace) -> int:
    ok = True

    if loader.is_repository(args.path):
        check = loader.load_repository(args.path)
        _print_report("fatrat-plugins.json", check.report)
        ok = check.report.ok
        plugins = check.plugins
    else:
        plugins = [loader.load(args.path)]

    for plugin in plugins:
        _print_report(plugin.name, plugin.report)
        ok = ok and plugin.report.ok

        manifest = plugin.manifest
        print("%s %s: %s" % (plugin.name, manifest.get("version", "?"), "ok" if plugin.report.ok else "BROKEN"))
        for descriptor in plugin.descriptors:
            print(_describe(descriptor))

    return 0 if ok and plugins else 2


# -- match ---------------------------------------------------------------------

def cmd_match(args: argparse.Namespace) -> int:
    plugins = _load(args)
    found = False

    for plugin in plugins:
        for descriptor in plugin.descriptors:
            if descriptor.kind not in (registry.DOWNLOAD, registry.EXTRACTOR):
                continue

            score = loader.accepts(descriptor, args.url)
            if score:
                found = True
                print("%s (%s): accepts it, score %d" % (descriptor.cls.__name__, descriptor.kind, score))

    if not found:
        print("no plugin accepts %s" % args.url)
        return 1

    return 0


# -- running plugins -----------------------------------------------------------

def _plugin(args: argparse.Namespace, plugins: List[loader.LoadedPlugin], kind: str) -> Any:
    try:
        return loader.pick(plugins, (kind,), args.plugin)
    except LookupError as error:
        raise SystemExit(str(error))


def _jsonable(value: Any) -> Any:
    if dataclasses.is_dataclass(value) and not isinstance(value, type):
        return {k: _jsonable(v) for k, v in dataclasses.asdict(value).items()}
    if isinstance(value, (list, tuple)):
        return [_jsonable(v) for v in value]
    if isinstance(value, dict):
        return {str(k): _jsonable(v) for k, v in value.items()}
    if isinstance(value, bytes):
        return value.decode("utf-8", "replace")
    if hasattr(value, "name") and hasattr(value, "value") and not isinstance(value, (str, int, float, bool)):
        return value.name  # an enum
    return value


def _finish(args: argparse.Namespace, result: Result, shown: Dict[str, Any]) -> int:
    if args.json:
        out = {"ok": result.ok, "error": result.error, "vars": result.vars, "messages": result.messages,
               "requests": [{"method": r.method, "url": r.url} for r in result.requests]}
        out.update({k: _jsonable(v) for k, v in shown.items()})
        print(json.dumps(out, indent=2))
        return 0 if result.ok else 1

    if not result.ok:
        print("FAILED: %s" % result.error)
        if result.traceback:
            print(result.traceback, file=sys.stderr)
        return 1

    print("OK")
    for key, value in shown.items():
        if value in (None, "", [], {}):
            continue
        if isinstance(value, list):
            print("  %s:" % key)
            for item in value:
                print("    %s" % (item,))
        else:
            print("  %-10s %s" % (key + ":", value))

    if result.vars:
        print("  vars:      %r" % result.vars)
    print("  fetches:   %d" % len(result.requests))
    return 0


def cmd_download(args: argparse.Namespace) -> int:
    plugins = _load(args)
    descriptor = _plugin(args, plugins, registry.DOWNLOAD)

    if not args.plugin and not loader.accepts(descriptor, args.url):
        print("note: %s's pattern does not match this URL; FatRat would not hand it over"
              % descriptor.cls.__name__, file=sys.stderr)

    host = _host(args)
    result = host.run_download(descriptor.cls, args.url)
    shown: Dict[str, Any] = {}

    if result.ok:
        target = result.download
        shown = {"url": target.url, "file name": result.file_name, "referrer": target.referrer,
                 "user agent": target.user_agent, "post data": target.post_data}

        if args.download:
            def progress(done: int, total: int) -> None:
                if not args.quiet:
                    print("\r  downloading %d / %s bytes" % (done, total or "?"), end="", file=sys.stderr, flush=True)

            try:
                path = host.download_file(result, args.download, progress)
            except Exception as error:  # noqa: BLE001
                result.ok, result.error = False, "downloading the resolved link failed: %s" % error
            else:
                if not args.quiet:
                    print(file=sys.stderr)
                shown["saved to"] = path
                shown["size"] = os.path.getsize(path)
                host.run_final_check(result, path)

    return _finish(args, result, shown)


def cmd_extract(args: argparse.Namespace) -> int:
    plugins = _load(args)
    descriptor = _plugin(args, plugins, registry.EXTRACTOR)
    result = _host(args).run_extract(descriptor.cls, args.url)
    return _finish(args, result, {"links": result.links})


def cmd_search(args: argparse.Namespace) -> int:
    plugins = _load(args)
    descriptor = _plugin(args, plugins, registry.SEARCH)
    result = _host(args).run_search(descriptor.cls, args.query)
    shown = {"results": ["%s  %s%s" % (h.name, h.url, "  (%d bytes)" % h.size if h.size else "")
                         for h in result.results]}
    if args.json:
        shown = {"results": result.results}
    return _finish(args, result, shown)


def cmd_account(args: argparse.Namespace) -> int:
    plugins = _load(args)
    descriptor = _plugin(args, plugins, registry.ACCOUNT_STATUS)
    result = _host(args).run_account_status(descriptor.cls)
    account = result.account
    shown = {"state": getattr(account, "state", None), "balance": getattr(account, "balance", None)}
    if args.json:
        shown = {"account": account}
    return _finish(args, result, shown)


def cmd_upload(args: argparse.Namespace) -> int:
    plugins = _load(args)
    descriptor = _plugin(args, plugins, registry.UPLOAD)

    if not os.path.isfile(args.file):
        raise SystemExit("%s is not a file" % args.file)

    result = _host(args).run_upload(descriptor.cls, args.file)
    return _finish(args, result, {"link": result.link, "kill link": result.kill_link})


# -- entry ---------------------------------------------------------------------

def _common(sub: argparse.ArgumentParser, runs: bool = True) -> None:
    sub.add_argument("path", help="plugin directory or plugin repository")
    if not runs:
        return

    sub.add_argument("-p", "--plugin", help="plugin or class name, when PATH holds several of that kind")
    sub.add_argument("-s", "--set", action="append", metavar="KEY=VALUE",
                     help="a FatRat setting the plugin can read (repeatable); true/false become booleans")
    sub.add_argument("--settings", metavar="FILE.json", help="settings as a JSON object")
    sub.add_argument("--real-wait", action="store_true", help="really wait out countdowns instead of skipping them")
    sub.add_argument("--captcha", metavar="TEXT", help="answer every captcha with TEXT")
    sub.add_argument("--captcha-prompt", action="store_true",
                     help="save each captcha image to a temp file and ask for the answer on the terminal")
    sub.add_argument("--timeout", type=float, default=300, metavar="SECONDS", help="give up after this long (300)")
    sub.add_argument("-q", "--quiet", action="store_true", help="do not echo the plugin's log")
    sub.add_argument("--json", action="store_true", help="print the outcome as JSON")


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(prog="fatrat_devkit", description=DESCRIPTION, epilog=EPILOG,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True, metavar="COMMAND")

    sub = commands.add_parser("check", help="validate plugin.json / fatrat-plugins.json and import the plugin")
    _common(sub, runs=False)
    sub.set_defaults(func=cmd_check)

    sub = commands.add_parser("match", help="which plugins accept a URL")
    _common(sub, runs=False)
    sub.add_argument("url")
    sub.set_defaults(func=cmd_match)

    sub = commands.add_parser("download", help="run a download plugin on a URL and show the link it resolves")
    _common(sub)
    sub.add_argument("url")
    sub.add_argument("--download", metavar="DIR", help="also fetch the resolved link into DIR, then run final_check()")
    sub.set_defaults(func=cmd_download)

    sub = commands.add_parser("extract", help="run an extractor plugin")
    _common(sub)
    sub.add_argument("url")
    sub.set_defaults(func=cmd_extract)

    sub = commands.add_parser("search", help="run a search plugin")
    _common(sub)
    sub.add_argument("query")
    sub.set_defaults(func=cmd_search)

    sub = commands.add_parser("account", help="run an account-status plugin")
    _common(sub)
    sub.set_defaults(func=cmd_account)

    sub = commands.add_parser("upload", help="run an upload plugin on a file")
    _common(sub)
    sub.add_argument("file")
    sub.set_defaults(func=cmd_upload)

    args = parser.parse_args(argv)
    return args.func(args)
