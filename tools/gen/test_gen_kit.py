#!/usr/bin/env python3
"""Tests of the generator of Studio Kit's described kinds: that its output is the one in the tree, and that it refuses
what it cannot describe.

Usage: python3 -m tools.gen.test_gen_kit
"""

import contextlib
import os
import tempfile
import unittest
from unittest.mock import patch

from tools.gen import gen_common
from tools.gen import gen_kit


@contextlib.contextmanager
def kinds_file(text):
    """Makes the generator read `text` as its list of kinds."""
    with tempfile.TemporaryDirectory() as root:
        path = os.path.join(root, "kinds.txt")
        with open(path, "w", encoding="utf-8") as f:
            f.write(text)
        with patch.object(gen_kit, "KINDS", path):
            yield


def described(text):
    """Returns the kinds that the generator describes from a list of them, by their names."""
    kit = gen_kit.read_kit()
    with kinds_file(text):
        kinds = gen_kit.read_kinds()
        for kind in kinds:
            gen_kit.describe(kit, kind)
    return {kind.name: kind for kind in kinds}


class Output(unittest.TestCase):
    def test_the_source_in_the_tree_is_what_the_generator_writes(self):
        # A header of the kit that gains a field changes a kind's attributes, so the source is generated again with it.
        with open(gen_kit.OUT, encoding="utf-8") as f:
            current = f.read()
        self.assertEqual(gen_common.rendered(gen_kit.OUT, gen_kit.source()), current,
                         "run ./dev.sh gen kit and commit what it writes")


class Describing(unittest.TestCase):
    def test_names_are_spelled_with_hyphens_between_words(self):
        self.assertEqual(gen_kit.kebab("noteTone"), "note-tone")
        self.assertEqual(gen_kit.kebab("ScrollY"), "scroll-y")
        self.assertEqual(gen_kit.kebab("Md"), "md")

    def test_a_fields_form_follows_its_type_and_its_struct(self):
        kinds = described("button prKitButton\n    text text\n    event click prKitActivated\n"
                          "icon prKitIcon\n    leaf\nmeter prKitMeter\npanel prKitPanel\n")
        forms = {name: {field.attribute: field.form for field in kind.fields} for name, kind in kinds.items()}
        self.assertEqual(forms["button"], {"icon": "Icon", "selected": "Flag", "disabled": "Flag",
                                           "variant": "Keyword", "size": "Keyword"})
        self.assertEqual(kinds["button"].text.name, "text")
        self.assertEqual(kinds["button"].key.name, "key")
        # A float is a length in a style, and a number in a content.
        self.assertEqual(forms["icon"]["size"], "Length")
        self.assertEqual(forms["meter"]["value"], "Float")
        # A style that holds a box's gives the box's fields as its own, and its place is the node's.
        self.assertEqual(forms["panel"]["gap"], "Length")
        self.assertEqual(forms["panel"]["rules"], "Keywords")
        self.assertEqual(kinds["panel"].place.name, "place")
        self.assertNotIn("place", forms["panel"])

    def test_a_kinds_lines_change_what_its_fields_are(self):
        kinds = described("label prKitLabel\n    text text\n    attribute text-align align\n"
                          "segmented prKitSegmented\n    omit direction\nspinner prKitSpinner\n    number size\n")
        self.assertIn("text-align", [field.attribute for field in kinds["label"].fields])
        self.assertEqual([field.attribute for field in kinds["segmented"].fields], ["disabled", "size"])
        self.assertEqual(kinds["spinner"].fields[0].form, "Float")

    def test_what_it_cannot_describe_is_refused(self):
        refused = (
            "checkbox prKitCheckbox\n",                           # a pointer has no attribute form
            "button prKitNoSuchWidget\n",                         # no such function
            "button prKitButton\n    text icon\n",                # the text is not a string
            "button prKitButton\n    omit colour\n",              # no such field
            "button prKitButton\n    event click prKitStepped\n",  # a query that takes more than the widget's node
            "button prKitButton\nbutton prKitButton\n",           # a kind twice
            "    text label\n",                                   # a line under no kind
            "button prKitButton\n    shows text\n",               # a word that says nothing
        )
        for text in refused:
            with self.subTest(text=text), self.assertRaises(SystemExit):
                described(text)


if __name__ == "__main__":
    unittest.main()
