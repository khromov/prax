#!/usr/bin/env python3
"""Tests of the Studio Kit gallery as a Svelte application: each section renders, and answers input, as the section of
examples/studio_gallery that it was ported from.

A case is an input file under gallery/inputs, named <section>.<case>.txt, of the host's actions
(tools/svelte/kit_host.cpp). The host runs the file twice, once declaring the example's section with the kit's own
calls and once running the application's bundle, and each screenshot that the file takes must hold the same pixels in
both. A file's first line may give the viewport, as `# extent 1100x900`. A section has the cases that its files give
it, so a section with no file is not tested.

The runs' screenshots are kept under the build's svelte/gallery directory, with an image that marks where a pair
differs, for whoever reads a failure."""

import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

from tools.suite import corpus
from tools.svelte import bundle
from tools.svelte import pngs

KIT_HOST = corpus.binary("svelte_kit_host")
GALLERY = Path(bundle.DIRECTORY) / "gallery"
INPUTS = GALLERY / "inputs"
KEPT = Path(corpus.BUILD) / "svelte" / "gallery"

# The viewport of a case whose file names none: wide enough for the gallery's panels, and taller than any section but
# the longest, so that a screenshot holds a whole section.
EXTENT = "1100x1400"


def extent_of(path):
    """Returns the viewport that an input file's first line gives, or the default."""
    with open(path, encoding="utf-8") as f:
        named = re.match(r"#\s*extent\s+(\d+x\d+)\s*$", f.readline())
    return named.group(1) if named else EXTENT


@unittest.skipUnless(bundle.available(), "bundles the components with Bun")
@unittest.skipUnless(os.path.exists(KIT_HOST), "runs the bundles in svelte_kit_host")
class GalleryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        cls.script = bundle.bundle(GALLERY / "main.js", cls.directory.name, backend="kit")

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    def run_host(self, case, side, *content):
        """Runs the host on a case's input file with the content that `content` names, and returns the directory of
        its screenshots, with what it printed."""
        out = KEPT / case.stem / side
        shutil.rmtree(out, ignore_errors=True)
        out.mkdir(parents=True)
        result = subprocess.run([KIT_HOST, "--headless", "--extent", extent_of(case), "--input", str(case), "--out",
                                 str(out), *content], capture_output=True, encoding="utf-8", timeout=120)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stderr, "", "the %s run of %s warned" % (side, case.name))
        return out, result.stdout

    def compare(self, case):
        section = case.name.split(".")[0]
        native, _ = self.run_host(case, "native", "--native", "gallery:" + section)
        ported, _ = self.run_host(case, "svelte", "--arg", "section=" + section, self.script)
        names = sorted(path.name for path in native.glob("*.png"))
        self.assertTrue(names, "%s takes no screenshot" % case.name)
        self.assertEqual(names, sorted(path.name for path in ported.glob("*.png")))
        failures = []
        for name in names:
            found = pngs.differences(native / name, ported / name, native.parent / ("differs-" + name))
            if found is not None:
                failures.append("%s: %s (see %s)" % (name, pngs.describe(found), native.parent))
        self.assertEqual(failures, [])


def case_test(case):
    return lambda self: self.compare(case)


for _case in sorted(INPUTS.glob("*.txt")):
    setattr(GalleryTests, "test_" + re.sub(r"\W", "_", _case.stem), case_test(_case))


if __name__ == "__main__":
    unittest.main()
