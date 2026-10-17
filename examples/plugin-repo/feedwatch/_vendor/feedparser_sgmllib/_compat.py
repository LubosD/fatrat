# Manage compatibility across supported Python versions.

import sys

__all__ = [
    "StringIO",
]


if sys.version_info >= (3, 12):
    from io import StringIO
else:
    # In Python >=3.12, `io.StringIO` has good memory allocation performance,
    # which makes it a good candidate for collecting unparsed input data
    # (see the comment in `SGMLParser.__init__()` about CPU and memory usage).
    #
    # However, the performance of `io.StringIO` is poor in Python 3.11 and below.
    # For these lower Python versions, bytearray exhibits good performance
    # but requires manual tracking of the length of the decoded string
    # as well as calls to `.encode()` and `.decode()`.
    #
    # Therefore, the `io.StringIO` interface is mimicked here
    # and backed by a bytearray implementation.
    class StringIO:
        def __init__(self) -> None:
            self._buffer = bytearray()
            self._length = 0

        def write(self, text: str) -> int:
            self._buffer.extend(text.encode())
            self._length += len(text)
            return len(text)

        def getvalue(self) -> str:
            return self._buffer.decode()

        def tell(self) -> int:
            return self._length
