#!/usr/bin/env python3
"""Tests of the Svelte renderers: components bundled for the engine's QuickJS render, and respond to input, as the
pages and programs that they were ported from do."""

import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

from tools import dev
from tools.suite import corpus
from tools.suite.wpt import wpt
from tools.svelte import bundle

SHELL = wpt.SHELL
KIT_HOST = corpus.binary("svelte_kit_host")
DEMOS = Path(bundle.DIRECTORY) / "demos"
WORKLOADS = Path(dev.ROOT) / "suites" / "prax" / "workloads"

# The viewport that the pages of suites/prax are laid out for.
VIEWPORT = "600x800"


def host_page(workload, script):
    """Returns the page `workload` with its own styles and content removed and the module `script` in their place, so
    that a component which brings both is all that differs from the page."""
    text = workload.read_text(encoding="utf-8")
    text, styles = re.subn(r"<style>.*?</style>\n", "", text, flags=re.S)
    text, bodies = re.subn(r"<body>.*</body>", '<body>\n<script type="module" src="%s"></script>\n</body>' % script,
                           text, flags=re.S)
    assert styles == 1 and bodies == 1, workload
    return text.replace('href="support/', 'href="%s/' % (workload.parent / "support").as_uri())


@unittest.skipUnless(bundle.available(), "bundles the components with Bun")
@unittest.skipUnless(os.path.exists(SHELL), "runs the bundles in pr_shell")
class DomRendererTests(unittest.TestCase):
    def shell(self, *arguments):
        result = subprocess.run([SHELL, "--virtual-time", "--viewport", VIEWPORT, *arguments], capture_output=True,
                                encoding="utf-8", timeout=60)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stderr, "")
        return result.stdout

    def compare(self, demo, workload, prelude=None):
        """Bundles the demo and compares its rendering with the page that it was ported from, as a reftest compares a
        page with its reference. Both run `prelude` first, if one is given, which is how a test drives the two alike.
        Returns what the pages printed, the demo's lines and then the workload's."""
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            bundle.bundle(DEMOS / demo / "main.js", root)
            page = root / "host.html"
            page.write_text(host_page(WORKLOADS / workload, "main.js"), encoding="utf-8")
            options = []
            if prelude is not None:
                (root / "prelude.js").write_text(prelude, encoding="utf-8")
                options = ["--prelude", str(root / "prelude.js")]
            out = self.shell(*options, "--reference", str(WORKLOADS / workload), "--html", str(page))
        printed = [line for line in out.splitlines() if not line.startswith("REFTEST ")]
        compared = [json.loads(line[len("REFTEST "):]) for line in out.splitlines() if line.startswith("REFTEST ")]
        self.assertEqual(compared, [{"maxDifference": 0, "differingPixels": 0, "blank": False}])
        return printed

    def test_a_static_page_renders_as_the_page_it_was_ported_from(self):
        self.assertEqual(self.compare("pricing_table", "pricing-table.html"), [])

    def test_a_form_renders_as_the_page_it_was_ported_from(self):
        self.assertEqual(self.compare("survey", "survey.html"), [])

    def test_a_form_answers_an_input_event_as_the_page_it_was_ported_from(self):
        # The event that a drag of the first slider ends in, which each page answers by writing the slider's value to
        # its output. The pages are compared once they have answered it. The script sets the value and sends the event
        # itself, since a slider takes no pointer input in pr_shell.
        printed = self.compare("survey", "survey.html", prelude="""
            addEventListener('load', () => {
                const slider = document.getElementById('nps');
                slider.value = '3';
                slider.dispatchEvent(new Event('input', {bubbles: true}));
                queueMicrotask(() => print('slider', slider.value, 'output', document.getElementById('npsv').textContent));
            });
        """)
        self.assertEqual(printed, ["slider 3 output 3", "slider 3 output 3"])

    def test_a_component_updates_the_document_when_its_state_changes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "Counter.svelte").write_text("""
                <script>
                    let count = $state(0);
                    let entries = $state(['first']);
                </script>
                <button id="add" class:used={count > 0} onclick={() => entries.push('entry ' + ++count)}>Add</button>
                {#if count > 1}<p id="many">{count} added</p>{/if}
                <ul>{#each entries as entry (entry)}<li>{entry}</li>{/each}</ul>
                <style>
                    .used { margin-top: 5px }
                </style>
            """, encoding="utf-8")
            (root / "main.js").write_text("""
                import { mount, unmount } from 'prax-svelte';
                import { flushSync } from 'svelte';
                import Counter from './Counter.svelte';

                const list = () => [...document.querySelectorAll('li')].map((item) => item.textContent).join(',');
                const mounted = mount(Counter);
                const add = document.getElementById('add');
                print(list(), document.getElementById('many'), getComputedStyle(add).marginTop);
                add.click();
                add.click();
                flushSync();
                print(list(), document.getElementById('many').textContent, getComputedStyle(add).marginTop);
                await unmount(mounted);
                print(document.querySelectorAll('button, li, p').length);
            """, encoding="utf-8")
            out = self.shell("--html", bundle.bundle(root / "main.js", root / "out"))
        self.assertEqual(out.splitlines(), ["first null 0px", "first,entry 1,entry 2 2 added 5px", "0"])


@unittest.skipUnless(bundle.available(), "bundles the components with Bun")
@unittest.skipUnless(os.path.exists(KIT_HOST), "runs the bundles in svelte_kit_host")
class KitRendererTests(unittest.TestCase):
    def host(self, root, name, *arguments):
        """Runs the host with no window and returns the PNG that it wrote of its last frame, with what it printed."""
        written = root / (name + ".png")
        result = subprocess.run([KIT_HOST, "--headless", "--screenshot", str(written), *arguments],
                                capture_output=True, encoding="utf-8", timeout=60)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stderr, "")
        return written.read_bytes(), result.stdout

    def render(self, root, name, component, *arguments):
        """Bundles a component from its source and returns what the host wrote and printed for it."""
        source = root / name
        source.mkdir()
        (source / "App.svelte").write_text(component, encoding="utf-8")
        (source / "main.js").write_text(
            "import { mount } from 'prax-svelte';\nimport App from './App.svelte';\nmount(App);\n", encoding="utf-8")
        return self.host(root, name, *arguments, bundle.bundle(source / "main.js", source / "out", backend="kit"))

    def test_a_program_of_kit_widgets_renders_and_answers_clicks_as_the_program_it_was_ported_from(self):
        # The button of examples/hello_style, whose clicks the badge beside it counts.
        button = ["--click", "68,143"]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            script = bundle.bundle(DEMOS / "hello_style" / "main.js", root, backend="kit")
            pictures = []
            for clicks in (0, 1, 3):
                ported, _ = self.host(root, "ported%d" % clicks, *button * clicks, script)
                native, _ = self.host(root, "native%d" % clicks, *button * clicks, "--native", "hello_style")
                self.assertEqual(ported, native, "after %d clicks" % clicks)
                pictures.append(native)
        self.assertEqual(len(set(pictures)), 3, "each count of clicks shows its own badge")

    def test_a_components_blocks_add_move_and_remove_widgets_as_its_state_changes(self):
        # Each click of the button takes the list a step on: a row and a badge added, the rows reversed, which moves
        # each keyed row, then two rows and the badge removed. What each step shows is compared with a component that
        # declares it outright, under the same clicks, which leave its button hovered alike.
        stepped = """
            <script>
                let rows = $state(['one', 'two', 'three']);
                let marked = $state(false);
                let step = 0;
                function advance() {
                    step += 1;
                    if (step === 1) {
                        rows.push('four');
                        marked = true;
                    } else if (step === 2) {
                        rows.reverse();
                    } else {
                        rows.splice(1, 2);
                        marked = false;
                    }
                }
            </script>
            <box gap="8" padding="16" align="start">
              <button onclick={advance}>Advance</button>
              {#if marked}<badge variant="success">marked</badge>{/if}
              {#each rows as row (row)}
                <label>{row}</label>
              {/each}
            </box>
        """
        outright = """
            <box gap="8" padding="16" align="start">
              <button>Advance</button>
              %s
            </box>
        """
        marked = '<badge variant="success">marked</badge>'
        steps = [
            "<label>one</label><label>two</label><label>three</label>",
            marked + "<label>one</label><label>two</label><label>three</label><label>four</label>",
            marked + "<label>four</label><label>three</label><label>two</label><label>one</label>",
            "<label>four</label><label>one</label>",
        ]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            pictures = []
            for clicks, widgets in enumerate(steps):
                button = ["--click", "40,29"] * clicks
                changed, _ = self.render(root, "stepped%d" % clicks, stepped, *button)
                declared, _ = self.render(root, "outright%d" % clicks, outright % widgets, *button)
                self.assertEqual(changed, declared, "after %d clicks" % clicks)
                pictures.append(declared)
        self.assertEqual(len(set(pictures)), 4, "each step shows its own widgets")

    def test_a_components_script_runs_its_timers_between_frames(self):
        waiting = """
            <script>
                let text = $state('waiting');
                console.log('started');
                setTimeout(() => {
                    text = 'done';
                    console.log('fired');
                }, 250);
            </script>
            <box padding="16"><label role="title">{text}</label></box>
        """
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            timed, printed = self.render(root, "timed", waiting)
            declared, _ = self.render(root, "declared", '<box padding="16"><label role="title">done</label></box>')
        self.assertEqual(printed.splitlines(), ["started", "fired"])
        self.assertEqual(timed, declared)

    def test_an_element_or_attribute_that_the_kit_lacks_warns_and_leaves_the_rest_declared(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "odd"
            source.mkdir()
            (source / "App.svelte").write_text(
                '<box padding="16"><label role="enormous" tone="accent">kept</label><table></table></box>',
                encoding="utf-8")
            (source / "main.js").write_text(
                "import { mount } from 'prax-svelte';\nimport App from './App.svelte';\nmount(App);\n",
                encoding="utf-8")
            script = bundle.bundle(source / "main.js", source / "out", backend="kit")
            result = subprocess.run([KIT_HOST, "--headless", "--screenshot", str(root / "odd.png"), script],
                                    capture_output=True, encoding="utf-8", timeout=60)
            odd = (root / "odd.png").read_bytes()
            plain, _ = self.render(root, "plain", '<box padding="16"><label tone="accent">kept</label><box></box></box>')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stderr.splitlines(), [
            'prax-svelte: <label> takes no role="enormous"',
            "prax-svelte: there is no <table>, which is declared as a <box>",
        ])
        self.assertEqual(odd, plain)


if __name__ == "__main__":
    unittest.main()
