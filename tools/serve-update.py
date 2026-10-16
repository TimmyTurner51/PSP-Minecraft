#!/usr/bin/env python3
"""
Serve an update to a PSP over the local network.

Why this exists
---------------
The alternative is pulling the memory stick out of the handheld and copying a new build onto it every
time a change needs looking at on real hardware. That is the loop this replaces: build, run this,
pick Update on the PSP.

The PSP half lives in Scripts/Updater.cpp and Scripts/StateUpdate.cpp. It fetches two things and
nothing else:

  GET /update.txt      six lines: the available version, the package's name, its size and crc32
  GET /<package>.zip   the package itself, which carries its own manifest.txt

So this script is a plain static file server over one folder. It is deliberately not clever: no
templating, no generated index, no authentication. What it does do is print the two addresses that
have to be typed in, because getting the LAN address wrong is the single most likely reason for a
first attempt to fail.

Usage
-----
  python tools/serve-update.py --dir update-server

Then on the PSP: Options -> Update, and make sure update_url.txt beside EBOOT.PBP contains the URL
this prints. If update_url.txt is absent the game falls back to http://192.168.1.100:8000/, which is a
guess and probably the wrong one.

Requirements, all on the PSP's side and none of them satisfiable from here
-------------------------------------------------------------------------
  * the WLAN switch on the console has to be up;
  * the PSP needs a saved connection to the same network and this machine, using WPA (not WPA2) or
    an open network -- that is a hardware limitation of the console, not of this script;
  * the PSP picks the access point itself through the system's own dialog, because homebrew cannot
    read the saved connection list.

What this does not do
---------------------
It is not a GitHub replacement and it is not meant to be public. It binds to every interface so the
PSP can reach it, which means anything else on the same network can read the folder too. That is the
correct trade for a LAN test tool and the wrong one for a release channel, which is why the GitHub
path is a separate piece of work.

Checked
-------
Not machine-checked -- it is a test harness. It does refuse to start without an update.txt, and it
refuses to serve from a folder where the package named in that file is missing, because both of those
produce a PSP that reports "could not reach the server" for a reason that has nothing to do with the
network and would send you looking in the wrong place.
"""

import argparse
import http.server
import os
import socket
import sys
import threading

# One line, the same keys make-update-package.py writes and ParseOffer reads.
OFFER_NAME = "update.txt"

# The new shape: a list of releases and the zips beside it, with no per-client instructions in it. The
# server's whole job becomes "hand over files"; every decision about what to fetch stays on the console.
# A directory with a releases.txt and no update.txt is served as-is rather than refused.
RELEASES_NAME = "releases.txt"


def lan_address():
    """This machine's address on the network the PSP is (hopefully) also on.

    Opening a UDP socket to a routable address and reading back the local end selects the interface
    the kernel would use to leave the machine, which is the interface the PSP needs to reach. Nothing
    is sent, so this works with no internet connection -- it is a routing lookup, not a connection.
    """
    probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        probe.connect(("192.0.2.1", 9))   # TEST-NET-1: reserved, never routed, nothing is sent
        return probe.getsockname()[0]
    except OSError:
        return None
    finally:
        probe.close()


def local_addresses():
    """Every non-loopback IPv4 address, as a fallback when the probe above is inconclusive."""
    found = set()
    try:
        for info in socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET):
            found.add(info[4][0])
    except OSError:
        pass
    return sorted(a for a in found if not a.startswith("127."))


def parse_offer(serve_dir):
    """The offer's fields, or an error string. Kept deliberately loose: this script only needs to
    find the package name and to prove the file exists.

    An absent update.txt is NOT an error when a releases.txt is present. update.txt is the old
    per-client shape -- it names one version, one package and a force flag, which is the server making
    the decision the launcher is supposed to make. The new shape publishes a list and the zips and
    nothing else, so this accepts either and lets the console choose.
    """
    path = os.path.join(serve_dir, OFFER_NAME)
    if not os.path.isfile(path):
        if os.path.isfile(os.path.join(serve_dir, RELEASES_NAME)):
            return {}, None
        return None, (f"no {OFFER_NAME} and no {RELEASES_NAME} in {serve_dir}. Build one first:\n"
                      f"  python tools/make-update-package.py --version X.Y.Z --new "
                      f"\"Minecraft - Compiled\" --outdir \"{serve_dir}\"")

    fields = {}
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            line = line.strip()
            if not line:
                continue
            key, _, value = line.partition("|")
            fields[key.strip()] = value.strip()

    for required in ("version", "code", "file", "size", "crc"):
        if required not in fields:
            return None, f"{OFFER_NAME} has no '{required}' line, so the PSP will reject it"

    package = fields["file"]
    if "/" in package or "\\" in package:
        # The game refuses a filename with a separator in it, so this would fail on the console with a
        # message about the server rather than about the offer. Catch it here, where it is fixable.
        return None, f"{OFFER_NAME} names a package with a path in it: {package!r}"

    if not os.path.isfile(os.path.join(serve_dir, package)):
        return None, f"{OFFER_NAME} names {package}, but there is no such file in {serve_dir}"

    return fields, None


def parse_range(value, size):
    """The first byte range in a Range header as (start, end) inclusive, or None when the header is
    absent or is not a range this server understands.

    Raises ValueError when the header *is* a byte range but names nothing that exists, which the caller
    answers with 416. The two cases are kept apart on purpose: a header we do not understand is
    answered by sending the whole file, as RFC 7233 requires, while a header asking for bytes past the
    end of the file is an error and a client that gets the whole file back instead will look like it
    worked.

    Only the FIRST range is honoured. A PSP section fetch asks for one span, and answering a
    multi-range request correctly means a multipart response body -- real work for a case this is not
    built for. Taking the first is what the client asked for, and the alternative (refusing) would be
    worse than the small lie.
    """
    if not value:
        return None

    value = value.strip()
    if not value.lower().startswith("bytes="):
        return None                     # some other unit: ignore the header, send everything

    spec = value[6:].split(",")[0].strip()
    if "-" not in spec:
        return None

    first, _, last = spec.partition("-")
    first, last = first.strip(), last.strip()

    try:
        if not first:                   # "-N"  the LAST N bytes, which is how an index is asked for
            count = int(last)
            if count <= 0:
                return None
            start, end = max(0, size - count), size - 1
        elif not last:                  # "N-"  from N to the end
            start, end = int(first), size - 1
        else:                           # "N-M"
            start, end = int(first), int(last)
    except ValueError:
        return None                     # not integers: not a range we understand

    if start >= size or start > end:
        raise ValueError(f"unsatisfiable range {spec!r} for a {size}-byte file")
    return start, min(end, size - 1)


class Handler(http.server.SimpleHTTPRequestHandler):
    """Static files, with a request log that is worth reading.

    The log is the point of the tool as much as the transfer is: watching the PSP ask for update.txt
    and then the package, with byte counts, is how you find out whether a failure was the network, the
    offer, or the package.
    """

    # Keep-alive, so the two requests share the connection. SimpleHTTPRequestHandler always sets
    # Content-Length, which is what makes HTTP/1.1 safe here -- and it is also what lets the PSP's own
    # client report a total for its progress bar.
    protocol_version = "HTTP/1.1"
    server_version = "MineCraftPSPUpdate/1.0"

    def log_message(self, fmt, *args):
        sys.stderr.write(f"  {self.address_string():15s}  {fmt % args}\n")

    def log_error(self, fmt, *args):
        # Overridden so a 404 goes through the same line format as everything else instead of
        # vanishing; a PSP asking for something we do not serve is exactly the thing you want to see.
        self.log_message(fmt, *args)

    def end_headers(self):
        # No caching. The whole point of this server is to hand over whatever was just built, and a
        # cached update.txt would have the PSP reporting the previous version as current.
        self.send_header("Cache-Control", "no-store, must-revalidate")
        # Advertised on every response, including the full-file ones, because that is how the client
        # learns it may ask for a span at all.
        self.send_header("Accept-Ranges", "bytes")
        super().end_headers()

    def do_GET(self):
        """Range requests, answered here because the base class does not implement them.

        This is the load-bearing part of the tool now. A client that wants a zip's index asks for the
        last N bytes of it, and SimpleHTTPRequestHandler responds by sending the WHOLE file with a 200
        -- which a zip reader accepts, because a 200 body is a valid zip. So the feature silently
        degrades into a 135 MB download instead of a 130 KB one, and the only evidence is the clock.
        That failure is worse than an error, which is why the header is checked explicitly here.
        """
        if not self.headers.get("Range"):
            return super().do_GET()

        path = self.translate_path(self.path)
        if os.path.isdir(path):
            return super().do_GET()
        if not os.path.isfile(path):
            return self.send_error(404, "File not found")

        size = os.path.getsize(path)
        try:
            span = parse_range(self.headers["Range"], size)
        except ValueError as problem:
            self.send_response(416)                     # Range Not Satisfiable
            self.send_header("Content-Range", f"bytes */{size}")
            self.send_header("Content-Length", "0")
            self.end_headers()
            self.log_message("416 %s -- %s", self.path, problem)
            return

        if span is None:
            return super().do_GET()

        start, end = span
        length = end - start + 1
        self.send_response(206)                         # Partial Content
        self.send_header("Content-Type", self.guess_type(path))
        self.send_header("Content-Range", f"bytes {start}-{end}/{size}")
        self.send_header("Content-Length", str(length))
        self.end_headers()

        sent = 0
        with open(path, "rb") as handle:
            handle.seek(start)
            while sent < length:
                chunk = handle.read(min(65536, length - sent))
                if not chunk:
                    break
                self.wfile.write(chunk)
                sent += len(chunk)

        # Logged like a full transfer, because the byte count IS the measurement: "206 130 KB of a
        # 135 MB file" is the proof that a diff cost an index and not a download.
        self.log_message("206 %s  bytes %d-%d/%d  (%d bytes sent)", self.path, start, end, size, sent)

    def do_HEAD(self):
        # A HEAD is cheap and the PSP's HTTP client may use one; let the base class answer it.
        super().do_HEAD()


def main():
    parser = argparse.ArgumentParser(description="Serve an update to a PSP on the local network.")
    parser.add_argument("--dir", default="update-server",
                        help="the folder holding update.txt and the package (default: update-server)")
    parser.add_argument("--port", type=int, default=8000, help="port to listen on (default: 8000)")
    parser.add_argument("--bind", default="0.0.0.0",
                        help="interface to bind (default: 0.0.0.0, every interface)")
    args = parser.parse_args()

    serve_dir = os.path.abspath(args.dir)
    if not os.path.isdir(serve_dir):
        raise SystemExit(f"error: no such folder: {serve_dir}")

    fields, problem = parse_offer(serve_dir)
    if problem:
        raise SystemExit(f"error: {problem}")

    address = lan_address() or (local_addresses() or ["<this machine>"])[0]

    print()
    print(f"serving      {serve_dir}")
    if fields:
        print(f"offering     MineCraft v{fields['version']}  ({fields['file']}, "
              f"{int(fields['size'])} bytes, crc {int(fields['crc']):08x})")
    else:
        print(f"offering     {RELEASES_NAME} -- no {OFFER_NAME}, so the launcher decides from the list")
        for name in sorted(n for n in os.listdir(serve_dir) if n.lower().endswith(".zip")):
            print(f"               {name}  {os.path.getsize(os.path.join(serve_dir, name)):,} bytes")
    print()
    print(f"  release list  http://{address}:{args.port}/{RELEASES_NAME if not fields else OFFER_NAME}")
    if fields:
        print(f"  package       http://{address}:{args.port}/{fields['file']}")
    print()
    print("Range requests are answered with 206, which is what lets the launcher read a zip's index")
    print("instead of downloading the zip. Watch the sizes below: an index fetch is tens of KB.")
    print()
    print("On the PSP, put this one line in update_url.txt, beside EBOOT.PBP:")
    print()
    print(f"    http://{address}:{args.port}/")
    print()
    print("(The trailing slash matters to nobody -- the game adds one -- but the address does. If it")
    print(" is wrong the game will say it cannot reach the server.)")
    print()
    print("Then: Options -> Update -> Check for update. Ctrl-C here to stop.")
    print()

    handler = lambda *a, **kw: Handler(*a, directory=serve_dir, **kw)   # noqa: E731
    server = http.server.ThreadingHTTPServer((args.bind, args.port), handler)
    server.daemon_threads = True

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nstopped")
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
