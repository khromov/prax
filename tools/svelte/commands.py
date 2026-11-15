"""`./dev.sh svelte`: Svelte applications bundled for the engine's QuickJS, and run by the default build's hosts."""

import os

from tools import dev
from tools.build import build as builds
from tools.svelte import bundle

USAGE = {
    "svelte": """\
./dev.sh svelte build [--backend dom|kit] <entry.js> <directory>
./dev.sh svelte run [--backend dom|kit] <entry.js>
./dev.sh svelte shot [--backend dom|kit] <entry.js> <out.png> [args...]
    a Svelte application, whose entry module mounts a component with `mount` from 'prax-svelte', compiled for the
    engine's custom renderer and bundled with Svelte's runtime into the one file that the engine evaluates
    (tools/svelte/build.js, which Bun runs). The dom backend, the default, renders through the DOM binding, with the
    components' styles in the cascade: its bundle is a module and a page that loads it. The kit backend renders with
    Studio Kit's widgets: its bundle is a script for svelte_kit_host. `build` writes the bundle into the directory.
    `run` opens it in a window, the browser's for dom and the host's for kit. `shot` writes a PNG of it without a
    window, and passes other arguments to what renders it: pr_shell for dom, as --viewport WxH, and svelte_kit_host
    for kit, as --extent WxH and --click X,Y""",
}

BACKENDS = ("dom", "kit")


def take_backend(argv):
    """Removes --backend and its value from `argv`, and returns the backend that it names, or dom."""
    if "--backend" not in argv:
        return "dom"
    at = argv.index("--backend")
    if at + 1 >= len(argv) or argv[at + 1] not in BACKENDS:
        dev.fail("--backend takes one of: %s" % " ".join(BACKENDS))
    backend = argv[at + 1]
    del argv[at:at + 2]
    return backend


def bundled(entry, backend, directory):
    """Bundles the entry into `directory` and returns what its host loads. A failed build fails the command with what
    the build printed."""
    if not bundle.available():
        dev.fail("bundling needs Bun on the path (https://bun.sh)")
    if not os.path.isfile(entry):
        dev.fail("no entry module at %s" % entry)
    try:
        return bundle.bundle(entry, directory, backend)
    except bundle.subprocess.CalledProcessError as failed:
        dev.fail("the bundle did not build:\n%s" % ((failed.stderr or "") + (failed.stdout or "")).strip())


def scratch(chosen, entry):
    """Returns the directory under the build's that `run` and `shot` bundle the entry into."""
    name = os.path.basename(os.path.dirname(os.path.abspath(entry))) or "application"
    return os.path.join(chosen.directory, "svelte", name)


def svelte(argv):
    dev.usage(argv, USAGE["svelte"], None if argv else 1)
    action, argv = argv[0], list(argv[1:])
    backend = take_backend(argv)
    if action == "build" and len(argv) == 2:
        print(bundled(argv[0], backend, argv[1]))
    elif action == "run" and len(argv) == 1:
        host = "browser" if backend == "dom" else "svelte_kit_host"
        chosen = builds.build(targets=[host])
        dev.run([dev.executable(chosen.directory, host), bundled(argv[0], backend, scratch(chosen, argv[0]))])
    elif action == "shot" and len(argv) >= 2:
        written = os.path.abspath(argv[1])
        if backend == "dom":
            chosen = builds.build(targets=["pr_shell"])
            page = bundled(argv[0], backend, scratch(chosen, argv[0]))
            dev.run([dev.executable(chosen.directory, "pr_shell"), *argv[2:], "--screenshot", written, "--html", page])
        else:
            chosen = builds.build(targets=["svelte_kit_host"])
            script = bundled(argv[0], backend, scratch(chosen, argv[0]))
            dev.run([dev.executable(chosen.directory, "svelte_kit_host"), "--headless", *argv[2:], "--screenshot",
                     written, script])
    else:
        dev.usage(argv, USAGE["svelte"], 1)
