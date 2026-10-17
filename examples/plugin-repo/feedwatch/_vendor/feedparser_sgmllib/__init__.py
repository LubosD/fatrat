"""A parser for SGML, using the derived class as a static DTD."""

# XXX This only supports those SGML features used by HTML.

# XXX There should be a way to distinguish between PCDATA (parsed
# character data -- the normal case), RCDATA (replaceable character
# data -- only char and entity references and end tags are special)
# and CDATA (character data -- only end tags are special).  RCDATA is
# not supported at all.


import _markupbase
import re
import typing as t

from ._compat import StringIO

__all__ = ["SGMLParser", "SGMLParseError"]

# Regular expressions used for parsing

interesting = re.compile("[&<]")
incomplete = re.compile(
    "&([a-zA-Z][a-zA-Z0-9]*|#[0-9]*)?|"
    "<([a-zA-Z][^<>]*|"
    "/([a-zA-Z][^<>]*)?|"
    "![^<>]*)?"
)

entityref = re.compile("&([a-zA-Z][-.a-zA-Z0-9]*)[^a-zA-Z0-9]")
charref = re.compile("&#([0-9]+|[xX][0-9a-fA-F]+);")

starttagopen = re.compile("<[>a-zA-Z]")
shorttagopen = re.compile("<[a-zA-Z][-.a-zA-Z0-9]*/")
shorttag = re.compile("<([a-zA-Z][-.a-zA-Z0-9]*)/([^/]*)/")
piclose = re.compile(">")
endbracket = re.compile("[<>]")
tagfind = re.compile("[a-zA-Z][-_.:a-zA-Z0-9]*")
attrfind = re.compile(
    r"\s*([a-zA-Z_][-:.a-zA-Z_0-9]*)[$]?(\s*=\s*"
    r'(\'[^\']*\'|"[^"]*"|[][\-a-zA-Z0-9./,:;+*%?!&$\(\)_#=~\'"@]*))?'
)


class SGMLParseError(RuntimeError):
    """Exception raised for all parse errors."""

    pass


# SGML parser base class -- find tags and call handler functions.
# Usage: p = SGMLParser(); p.feed(data); ...; p.close().
# The dtd is defined by deriving a class which defines methods
# with special names to handle tags: start_foo and end_foo to handle
# <foo> and </foo>, respectively, or do_foo to handle <foo> by itself.
# (Tags are converted to lower case for this purpose.)  The data
# between tags is passed to the parser by calling self.handle_data()
# with some data as argument (the data may be split up in arbitrary
# chunks).  Entity references are passed by calling
# self.handle_entityref() with the entity reference as argument.


class SGMLParser(_markupbase.ParserBase):
    # Definition of entities -- derived classes may override
    entity_or_charref = re.compile("&(?:([a-zA-Z][-.a-zA-Z0-9]*)|#([0-9]+))(;?)")

    def __init__(self, verbose: bool = False) -> None:
        """Initialize and reset this instance."""
        self.verbose = verbose

        # These variables help guard against quadratic CPU usage behavior
        # without significantly increasing memory usage.
        #
        # Previously, each call to `.feed()` resulted in string-concatenation
        # and a regular expression matching from the start of the raw data
        # even though it was known that the existing raw data wouldn't match.
        # For large unclosed tokens (like `<a href="...`) fed in small chunks,
        # this became quadratically expensive.
        #
        # This quadratic CPU behavior is addressed by:
        #
        # * Gating failing calls to `.goahead()` behind an exponential backoff
        # * Avoiding string concatenation until it's needed
        #
        # Memory usage is guarded by using an object with a resizeable buffer;
        # a simple list of strings can increase memory usage significantly.
        #
        self._rawdata = ""
        self._pending = StringIO()
        self._retry_at = 0

        self.reset()

    def reset(self) -> None:
        """Reset this instance. Loses all unprocessed data."""
        self.__starttag_text: str | None = None
        self._rawdata = ""
        self._pending = StringIO()
        self._retry_at = 0
        self.stack: list[str] = []
        self.lasttag = "???"
        self.nomoretags = 0
        self.literal = 0
        super().reset()

    @property
    def rawdata(self) -> str:
        self._flush_pending()
        return self._rawdata

    @rawdata.setter
    def rawdata(self, value: str) -> None:
        self._rawdata = value
        self._pending = StringIO()

    def _flush_pending(self) -> None:
        """Merge any chunks queued by `feed()` into `self._rawdata`."""

        if self._pending.tell():
            self._rawdata += self._pending.getvalue()
            self._pending = StringIO()

    def setnomoretags(self) -> None:
        """Enter literal mode (CDATA) till EOF.

        Intended for derived classes only.
        """
        self.nomoretags = self.literal = 1

    def setliteral(self, *args: t.Any) -> None:
        """Enter literal mode (CDATA).

        Intended for derived classes only.
        """
        self.literal = 1

    def feed(self, data: str) -> None:
        """Feed some data to the parser.

        Call this as often as you want, with as little or as much text
        as you want (may include '\n').  (This just saves the text,
        all the processing is done by goahead().)
        """

        self._pending.write(data)
        if len(self._rawdata) + self._pending.tell() < self._retry_at:
            # The last attempt to resolve the pending token failed.
            # Skip re-scanning until enough new data has arrived
            # to make another attempt worthwhile.
            return
        self._flush_pending()
        self.goahead(False)

    def close(self) -> None:
        """Handle the remaining data."""

        self._flush_pending()
        self.goahead(True)

    def error(self, message: str) -> t.NoReturn:
        raise SGMLParseError(message)

    # Internal -- handle data as far as reasonable.  May leave state
    # and data to be processed by a subsequent call.  If 'end' is
    # true, force handling all data as if followed by EOF marker.
    def goahead(self, end: bool) -> None:
        # Accessing `self.rawdata` flushes pending data.
        rawdata = self.rawdata
        i = 0
        n = len(rawdata)
        while i < n:
            if self.nomoretags:
                self.handle_data(rawdata[i:n])
                i = n
                break
            match = interesting.search(rawdata, i)
            if match:
                j = match.start()
            else:
                j = n
            if i < j:
                self.handle_data(rawdata[i:j])
            i = j
            if i == n:
                break
            if rawdata[i] == "<":
                if starttagopen.match(rawdata, i):
                    if self.literal:
                        self.handle_data(rawdata[i])
                        i = i + 1
                        continue
                    k = self.parse_starttag(i)
                    if k < 0:
                        break
                    i = k
                    continue
                if rawdata.startswith("</", i):
                    k = self.parse_endtag(i)
                    if k < 0:
                        break
                    i = k
                    self.literal = 0
                    continue
                if self.literal:
                    if n > (i + 1):
                        self.handle_data("<")
                        i = i + 1
                    else:
                        # incomplete
                        break
                    continue
                if rawdata.startswith("<!--", i):
                    # Strictly speaking, a comment is --.*--
                    # within a declaration tag <!...>.
                    # This should be removed,
                    # and comments handled only in parse_declaration.
                    k = self.parse_comment(i)
                    if k < 0:
                        break
                    i = k
                    continue
                if rawdata.startswith("<?", i):
                    k = self.parse_pi(i)
                    if k < 0:
                        break
                    i = i + k
                    continue
                if rawdata.startswith("<!", i):
                    # This is some sort of declaration; in "HTML as
                    # deployed," this should only be the document type
                    # declaration ("<!DOCTYPE html...>").
                    try:
                        k = self.parse_declaration(i)
                    except AssertionError as error:
                        raise SGMLParseError(error.args)
                    if k < 0:
                        break
                    i = k
                    continue
            elif rawdata[i] == "&":
                if self.literal:
                    self.handle_data(rawdata[i])
                    i = i + 1
                    continue
                match = charref.match(rawdata, i)
                if match:
                    name = match.group(1)
                    self.handle_charref(name)
                    i = match.end(0)
                    if rawdata[i - 1] != ";":
                        i = i - 1
                    continue
                match = entityref.match(rawdata, i)
                if match:
                    name = match.group(1)
                    self.handle_entityref(name)
                    i = match.end(0)
                    if rawdata[i - 1] != ";":
                        i = i - 1
                    continue
            else:
                self.error("neither < nor & ??")
            # We get here only if incomplete matches but
            # nothing else
            match = incomplete.match(rawdata, i)
            if not match:
                self.handle_data(rawdata[i])
                i = i + 1
                continue
            j = match.end(0)
            if j == n:
                break  # Really incomplete
            self.handle_data(rawdata[i:j])
            i = j
        # end while
        if end and i < n:
            self.handle_data(rawdata[i:n])
            i = n
        self.rawdata = rawdata[i:]
        if i == 0 and n > 0:
            # No progress was made this call.
            # Use exponential back off to set how much new data must arrive
            # before the next scan retry.
            # This ensures a persistently-incomplete token costs O(n) overall
            # instead of O(n**2).
            self._retry_at = n * 2
        else:
            self._retry_at = 0
        # XXX if end: check for empty stack

    # Extensions for the DOCTYPE scanner:
    _decl_otherchars = "="

    # Internal -- parse processing instr, return length or -1 if not terminated
    def parse_pi(self, i: int) -> int:
        rawdata = self.rawdata
        if rawdata[i : i + 2] != "<?":
            self.error("unexpected call to parse_pi()")
        match = piclose.search(rawdata, i + 2)
        if not match:
            return -1
        j = match.start(0)
        self.handle_pi(rawdata[i + 2 : j])
        j = match.end(0)
        return j - i

    def get_starttag_text(self) -> str | None:
        return self.__starttag_text

    # Internal -- handle starttag, return length or -1 if not terminated
    def parse_starttag(self, i: int) -> int:
        self.__starttag_text = None
        start_pos = i
        rawdata = self.rawdata
        if shorttagopen.match(rawdata, i):
            # SGML shorthand: <tag/data/ == <tag>data</tag>
            # XXX Can data contain &... (entity or char refs)?
            # XXX Can data contain < or > (tag characters)?
            # XXX Can there be whitespace before the first /?
            match = shorttag.match(rawdata, i)
            if not match:
                return -1
            tag, data = match.group(1, 2)
            self.__starttag_text = f"<{tag}/"
            tag = tag.lower()
            k = match.end(0)
            self.finish_shorttag(tag, data)
            self.__starttag_text = rawdata[start_pos : match.end(1) + 1]
            return k
        # XXX The following should skip matching quotes (' or ")
        # As a shortcut way to exit, this isn't so bad, but shouldn't
        # be used to locate the actual end of the start tag since the
        # < or > characters may be embedded in an attribute value.
        match = endbracket.search(rawdata, i + 1)
        if not match:
            return -1
        j = match.start(0)
        # Now parse the data between i+1 and j into a tag and attrs
        attrs = []
        if rawdata[i : i + 2] == "<>":
            # SGML shorthand: <> == <last open tag seen>
            k = j
            tag = self.lasttag
        else:
            match = tagfind.match(rawdata, i + 1)
            if not match:
                self.error("unexpected call to parse_starttag")
            k = match.end(0)
            tag = rawdata[i + 1 : k].lower()
            self.lasttag = tag
        while k < j:
            match = attrfind.match(rawdata, k)
            if not match:
                break
            attrname, rest, attrvalue = match.group(1, 2, 3)
            if not rest:
                attrvalue = attrname
            else:
                if (
                    attrvalue[:1] == "'" == attrvalue[-1:]
                    or attrvalue[:1] == '"' == attrvalue[-1:]
                ):
                    # strip quotes
                    attrvalue = attrvalue[1:-1]
                attrvalue = self.entity_or_charref.sub(self._convert_ref, attrvalue)
            attrs.append((attrname.lower(), attrvalue))
            k = match.end(0)
        if rawdata[j] == ">":
            j = j + 1
        self.__starttag_text = rawdata[start_pos:j]
        self.finish_starttag(tag, attrs)
        return j

    # Internal -- convert entity or character reference
    def _convert_ref(self, match: re.Match[str]) -> str:
        if match.group(2):
            return self.convert_charref(match.group(2)) or "&#%s%s" % match.groups()[1:]
        elif match.group(3):
            return self.convert_entityref(match.group(1)) or "&%s;" % match.group(1)
        else:
            return "&%s" % match.group(1)

    # Internal -- parse endtag
    def parse_endtag(self, i: int) -> int:
        rawdata = self.rawdata
        match = endbracket.search(rawdata, i + 1)
        if not match:
            return -1
        j = match.start(0)
        tag = rawdata[i + 2 : j].strip().lower()
        if rawdata[j] == ">":
            j = j + 1
        self.finish_endtag(tag)
        return j

    # Internal -- finish parsing of <tag/data/ (same as <tag>data</tag>)
    def finish_shorttag(self, tag: str, data: str) -> None:
        self.finish_starttag(tag, [])
        self.handle_data(data)
        self.finish_endtag(tag)

    # Internal -- finish processing of start tag
    # Return -1 for unknown tag, 0 for open-only tag, 1 for balanced tag
    def finish_starttag(
        self, tag: str, attrs: list[tuple[str, str]]
    ) -> t.Literal[-1, 0, 1]:
        try:
            method = getattr(self, "start_" + tag)
        except AttributeError:
            try:
                method = getattr(self, "do_" + tag)
            except AttributeError:
                self.unknown_starttag(tag, attrs)
                return -1
            else:
                self.handle_starttag(tag, method, attrs)
                return 0
        else:
            self.stack.append(tag)
            self.handle_starttag(tag, method, attrs)
            return 1

    # Internal -- finish processing of end tag
    def finish_endtag(self, tag: str) -> None:
        if (match := tagfind.match(tag)) is not None:
            tag = match.group(0)
        if not tag:
            found = len(self.stack) - 1
            if found < 0:
                self.unknown_endtag(tag)
                return
        else:
            if tag not in self.stack:
                try:
                    getattr(self, "end_" + tag)
                except AttributeError:
                    self.unknown_endtag(tag)
                else:
                    self.report_unbalanced(tag)
                return
            found = len(self.stack)
            for i, stack_tag in enumerate(reversed(self.stack)):
                if stack_tag == tag:
                    found = len(self.stack) - 1 - i
                    break
        while len(self.stack) > found:
            tag = self.stack[-1]
            method: t.Callable[[], t.Any] | None = getattr(self, "end_" + tag, None)
            if method is not None:
                self.handle_endtag(tag, method)
            else:
                self.unknown_endtag(tag)
            del self.stack[-1]

    # Overridable -- handle start tag
    def handle_starttag(
        self, tag: str, method: t.Callable[[t.Any], t.Any], attrs: list[tuple[str, str]]
    ) -> None:
        method(attrs)

    # Overridable -- handle end tag
    def handle_endtag(self, tag: str, method: t.Callable[[], t.Any]) -> None:
        method()

    # Example -- report an unbalanced </...> tag.
    def report_unbalanced(self, tag: str) -> None:
        if self.verbose:
            print("*** Unbalanced </" + tag + ">")
            print("*** Stack:", self.stack)

    def convert_charref(self, name: str) -> str | None:
        """Convert character reference, may be overridden."""
        try:
            n = int(name)
        except ValueError:
            return None
        if not 0 <= n <= 127:
            return None
        return self.convert_codepoint(n)

    def convert_codepoint(self, codepoint: int) -> str:
        return chr(codepoint)

    def handle_charref(self, name: str) -> None:
        """Handle character reference, no need to override."""
        replacement = self.convert_charref(name)
        if replacement is None:
            self.unknown_charref(name)
        else:
            self.handle_data(replacement)

    # Definition of entities -- derived classes may override
    entitydefs = {"lt": "<", "gt": ">", "amp": "&", "quot": '"', "apos": "'"}

    def convert_entityref(self, name: str) -> str | None:
        """Convert entity references.

        As an alternative to overriding this method; one can tailor the
        results by setting up the self.entitydefs mapping appropriately.
        """
        table = self.entitydefs
        if name in table:
            return table[name]
        else:
            return None

    def handle_entityref(self, name: str) -> None:
        """Handle entity references, no need to override."""
        replacement = self.convert_entityref(name)
        if replacement is None:
            self.unknown_entityref(name)
        else:
            self.handle_data(replacement)

    # Example -- handle data, should be overridden
    def handle_data(self, data: str) -> t.Any:
        pass

    # Example -- handle comment, could be overridden
    def handle_comment(self, data: str) -> t.Any:
        pass

    # Example -- handle declaration, could be overridden
    def handle_decl(self, decl: t.Any) -> t.Any:
        pass

    # Example -- handle processing instruction, could be overridden
    def handle_pi(self, data: str) -> t.Any:
        pass

    # To be overridden -- handlers for unknown objects
    def unknown_starttag(self, tag: str, attrs: list[tuple[str, str]]) -> t.Any:
        pass

    def unknown_endtag(self, tag: str) -> t.Any:
        pass

    def unknown_charref(self, ref: str) -> t.Any:
        pass

    def unknown_entityref(self, ref: str) -> t.Any:
        pass
