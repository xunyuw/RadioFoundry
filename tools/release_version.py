#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 The RadioFoundry Authors
"""Release names (docs/dev/release.md): calendar versions, so a user sees at a glance how old the firmware is.

  vYY.M          the first release of a month (v26.11)
  vYY.M.N        the next ones that month (v26.11.1, v26.11.2)
  ...bN          a pre-release of that name (v26.12b1); the final keeps the name even if it ships later
  ...-NAME       a build of someone else's tree (v26.11-bi4aoq): a fork's release, never one of ours. It sorts
                 before its own base name, so a fork's v26.11-bi4aoq is older than our v26.11 and newer than our
                 v26.10, and every consumer that offers a "latest release" passes over it

The month has no leading zero and the count starts at 1, so v26.11.3 never reads as March. The Makefile (the
RFOUNDRY_RELEASE the radio shows), tools/release_build.sh, the release workflow and the web site's build all ask this
file, so the rule lives in one place.

  tools/release_version.py check TAG       exit 0 for a release name, else 1 with the reason
  tools/release_version.py own TAG         exit 0 when TAG is one of this project's own names (no -NAME)
  tools/release_version.py pick TAG...     print the newest release name among TAGs (other tags ignored), or nothing
  tools/release_version.py prerelease TAG  exit 0 when TAG is a pre-release (GitHub and this project mark it so)
  tools/release_version.py --self-test
"""
import os
import re
import subprocess
import sys

# the year in two digits (26 is 2026, saving room on small displays), from the first release on. A fork's name adds
# "-" and a short lowercase word, which keeps a user's build from ever reading as ours on the radio or in a listing.
# The word may not be a bare "b" and digits: that is this project's own beta suffix written after a dash, and a name
# that spells both ways is one no reader can tell apart
NAME_RE = (r"v(2[6-9]|[3-9][0-9])\.(1[0-2]|[1-9])(?:\.([1-9][0-9]{0,2}))?(?:b([1-9][0-9]{0,2}))?"
           r"(?:-(?!b[0-9]+$)([a-z][a-z0-9]{1,15}))?")
NAME = re.compile(NAME_RE)


def parse(tag):
    """The sort key of a release name (newer is larger), or None when TAG isn't one."""
    m = NAME.fullmatch(tag or "")
    if m is None:
        return None
    year, month, n, beta, fork = m.groups()
    # A beta sorts before its final (v26.12b2 < v26.12), and a fork's name before its base: v26.11-bi4aoq is who
    # else's build of v26.11, so it must never win over v26.11 itself in pick(), the site's latest-release choice, or
    # the radio's own release line
    return (int(year), int(month), int(n or 0), (0, int(beta)) if beta else (1, 0), 0 if fork else 1)


def is_fork(tag):
    """A build of someone else's tree (v26.11-bi4aoq): a release name with a -NAME suffix."""
    m = NAME.fullmatch(tag or "")
    return m is not None and m.group(5) is not None


def is_prerelease(tag):
    return parse(tag) is not None and ("b" in tag or is_fork(tag))


def newest(tags, own_only=False):
    names = [t for t in tags if parse(t) is not None and not (own_only and is_fork(t))]
    return max(names, key=parse) if names else None


def self_test():
    ok = True

    def want(cond, what):
        nonlocal ok
        if not cond:
            print("FAIL:", what)
            ok = False

    for good in ("v26.11", "v26.1", "v26.11.1", "v26.12b1", "v27.3.2b10", "v26.11-bi4aoq", "v26.11.2-bi4aoq"):
        want(parse(good) is not None, "accepts " + good)
    for bad in ("v0.1.0", "v26.011", "v26.13", "v26.0", "v26.11.0", "2026.11", "v26.11-rc.1",
                "v26.11b0", "v26.11b", "v26.11B1", "v26.12-beta.1", "v26.12-b1", "v2026", "v26.11.1.1", "v2026.11", "v25.12", "v026.1", "",
                "v26.11-", "v26.11-BI4AOQ", "v26.11-b", "v26.11-b1234567890123456", "v26.11bi4aoq"):
        want(parse(bad) is None, "refuses " + repr(bad))
    want(newest(["v26.11", "v26.12b1", "v26.11.1", "v0.9.0"]) == "v26.12b1", "newest with a beta")
    want(newest(["v26.12b2", "v26.12", "v26.12b10"]) == "v26.12", "the final after its betas")
    want(newest(["v26.2", "v26.10"]) == "v26.10", "months sort as numbers")
    want(newest(["main", "v1.0.0"]) is None, "no release name")
    want(is_prerelease("v26.12b1") and not is_prerelease("v26.12"), "pre-release")
    # a fork's name never wins over its own base, and own_only hides it: the site's latest-release choice and the
    # radio's release line must not show someone else's build as this project's
    want(newest(["v26.11-bi4aoq", "v26.11"]) == "v26.11", "a fork name sorts under its base")
    want(newest(["v26.11-bi4aoq", "v26.10"]) == "v26.11-bi4aoq", "a fork name sorts over an older base")
    want(newest(["v26.11-bi4aoq", "v26.10"], own_only=True) == "v26.10", "own_only skips a fork name")
    want(is_fork("v26.11-bi4aoq") and not is_fork("v26.11"), "fork detection")
    want(is_prerelease("v26.11-bi4aoq"), "a fork name is a pre-release to a listing")

    # the Makefile takes RELEASE= only when it is a release name
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")

    # any board will do (print-vars builds nothing): the first in boards/
    board = sorted(d for d in os.listdir(os.path.join(root, "boards"))
                   if os.path.isfile(os.path.join(root, "boards", d, "board.mk")))[0]

    def make(*args):
        return subprocess.run(["make", "-s", "-C", root, "BOARD=" + board, "print-vars", "VARS=RFOUNDRY_RELEASE"] + list(args),
                              capture_output=True, text=True, timeout=120)
    r = make("RELEASE=v26.11.1")
    want(r.returncode == 0 and "RFOUNDRY_RELEASE=v26.11.1" in r.stdout.splitlines(), "make RELEASE=v26.11.1: " + r.stdout + r.stderr)
    r = make("RELEASE=v1.2.0")
    want(r.returncode != 0 and "not a release name" in r.stderr, "make RELEASE=v1.2.0 refused: " + r.stdout + r.stderr)
    r = make("RELEASE=")
    want(r.returncode == 0 and "RFOUNDRY_RELEASE=" in r.stdout.splitlines(), "make RELEASE= (none): " + r.stdout + r.stderr)
    # tools/release_build.sh refuses a tag that isn't one before it builds anything
    r = subprocess.run([os.path.join(root, "tools", "release_build.sh"), "--toolchain", "arm", "--version", "v1.2.0",
                        "--out", os.devnull], capture_output=True, text=True, timeout=120)
    want(r.returncode == 2 and "not a release name" in r.stderr, "release_build.sh v1.2.0 refused: " + r.stdout + r.stderr)

    print("release_version self-test: %s" % ("passed" if ok else "FAILED"))
    return ok


def main(argv):
    if argv[:1] == ["--self-test"]:
        return 0 if self_test() else 1
    if len(argv) == 2 and argv[0] == "check":
        if parse(argv[1]) is None:
            print("%r is not a release name: vYY.M, vYY.M.N, either with bN, or a fork's with -NAME "
                  "(docs/dev/release.md)" % argv[1], file=sys.stderr)
            return 1
        return 0
    if argv[:1] == ["pick"]:
        n = newest(argv[1:])
        if n:
            print(n)
        return 0
    if len(argv) == 2 and argv[0] == "prerelease":
        return 0 if is_prerelease(argv[1]) else 1
    if len(argv) == 2 and argv[0] == "own":
        return 0 if parse(argv[1]) is not None and not is_fork(argv[1]) else 1
    print("usage: release_version.py check TAG | own TAG | pick TAG... | prerelease TAG | --self-test", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
