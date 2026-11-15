"""Bundles Svelte applications for the engine's QuickJS with build.js, which Bun runs."""

import os
import shutil
import subprocess

from tools import dev

# This directory, which holds the vendored Svelte, the renderers and build.js.
DIRECTORY = os.path.join(dev.ROOT, "tools", "svelte")

# Where Svelte and what it depends on are installed, which build.js reads them from. Like everything under
# third_party, it is fetched and never committed.
INSTALLED = os.path.join(dev.ROOT, "third_party", "svelte")


def available():
    """Returns true if Bun, which compiles and bundles the applications, is on the path."""
    return shutil.which("bun") is not None


def install():
    """Installs the vendored Svelte and what it depends on under third_party, as the lockfile pins them, unless they
    are there already. A change of the lockfile installs again. To change what is pinned, edit package.json, run
    `bun install` in third_party/svelte, and copy its bun.lock back here."""
    lock = os.path.join(DIRECTORY, "bun.lock")
    stamp = os.path.join(INSTALLED, ".installed")
    if os.path.exists(stamp) and os.path.getmtime(stamp) >= os.path.getmtime(lock):
        return
    # The manifest names the vendored package by a path beside it, so the package goes along.
    shutil.rmtree(INSTALLED, ignore_errors=True)
    os.makedirs(INSTALLED)
    for name in ("package.json", "bun.lock"):
        shutil.copy(os.path.join(DIRECTORY, name), INSTALLED)
    shutil.copytree(os.path.join(DIRECTORY, "vendor"), os.path.join(INSTALLED, "vendor"))
    subprocess.run(["bun", "install", "--frozen-lockfile"], cwd=INSTALLED, check=True, capture_output=True)
    with open(stamp, "w"):
        pass


def bundle(entry, out, backend="dom"):
    """Bundles the application whose entry module is `entry` into the directory `out`, for the renderer `backend`, and
    returns what its host loads: the page that loads the dom backend's bundle, or the kit backend's script. A failure
    raises CalledProcessError with what the build printed."""
    install()
    entry = os.path.abspath(entry)
    out = os.path.abspath(out)
    subprocess.run(["bun", "build.js", "--backend", backend, entry, out], cwd=DIRECTORY, check=True,
                   capture_output=True, encoding="utf-8")
    return os.path.join(out, os.path.splitext(os.path.basename(entry))[0] + (".html" if backend == "dom" else ".js"))
