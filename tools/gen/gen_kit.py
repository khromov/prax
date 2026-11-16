#!/usr/bin/env python3
"""Generates the kinds of Studio Kit's QuickJS binding that the kit's headers describe
(lib/studio/kit/quickjs/src/kit_node_quickjs_kinds.cpp).

A widget of the kit is a function of a content and, for most, a style, whose fields are what a script sets as the
attributes of the widget's element. The generator reads the structs, the enumerations and the functions of the kit's
headers, and the list of kinds beside its output, which names each kind's function and says what the headers cannot.
For each kind it writes the data that a node holds, a table of the data's fields, and the call of the kit's function,
which the binding's tree sets and declares the kind from (JsKitField, JsKitWidget).

A field whose type has no attribute form fails the generator, so a kind that it cannot describe whole stays written by
hand."""

import glob
import os
import re
import textwrap

from tools.gen.gen_common import LINE_LIMIT, ROOT, banner, c_string, write

HEADERS = os.path.join(ROOT, "lib", "studio", "kit", "include", "pr")
SOURCE = os.path.join(ROOT, "lib", "studio", "kit", "quickjs", "src")
KINDS = os.path.join(SOURCE, "kit_node_quickjs_kinds.txt")
OUT = os.path.join(SOURCE, "kit_node_quickjs_kinds.cpp")

# The forms of an attribute, by the name of their JsKitFieldType, each with how the reference names a value of it, for
# one attribute and for several.
FORMS = {
    "Flag": ("a flag", "flags"),
    "String": ("a string", "strings"),
    "Icon": ("an icon's name", "icons' names"),
    "Float": ("a number", "numbers"),
    "Length": ("a length in px", "lengths in px"),
    "Uint": ("a whole number", "whole numbers"),
    "Edges": ("edges in px", "edges in px"),
    "Size": ("a size", "sizes"),
    "Color": ("a color", "colors"),
}

# The types of field that have one form whatever struct they are in. A float is a length in a style and a number
# elsewhere, unless its kind says otherwise, and a field of one of the kit's enumerations is a keyword.
TYPES = {
    "bool": "Flag",
    "PrStringView": "String",
    "PrKitIconKind": "Icon",
    "uint32_t": "Uint",
    "PrKitEdges": "Edges",
    "PrKitSize": "Size",
    "PrColor": "Color",
}

# What the data holds for a field whose value is not the field itself: the type of the data's member that the field's
# `owner` names, and the header that declares it.
OWNERS = {
    "String": ("PrRawString", "pr/raw_string.h"),
    "Color": ("JsKitColorValue", "studio/kit/quickjs/src/kit_node_quickjs.h"),
}

# What a kind's line may say of a field, by the line's first word.
FIELD_WORDS = ("text", "omit", "number", "length")


class Kit:
    """What the kit's headers declare, each name with the header that declares it."""

    def __init__(self):
        self.widths = {}     # an integer typedef's name -> the type that it names
        self.members = {}    # an enumeration's name -> the names of its members, without the enumeration's
        self.structs = {}    # a struct's name -> its fields, as (type, name)
        self.functions = {}  # a function's name -> its return type and its parameters, as (type, name)
        self.homes = {}      # a name -> its header, as an include names it

    def read(self, path):
        home = "pr/" + os.path.basename(path)
        with open(path, encoding="utf-8") as f:
            code = re.sub(r"//[^\n]*", "", f.read())
        for match in re.finditer(r"typedef (u?int\d+_t) (\w+);", code):
            self.widths[match.group(2)] = match.group(1)
            self.homes[match.group(2)] = home
        for match in re.finditer(r"enum \{(.*?)\};", code, re.S):
            for entry in match.group(1).split(","):
                member = re.match(r"\s*(\w+?)_(\w+)", entry)
                if member is not None:
                    self.members.setdefault(member.group(1), []).append(member.group(2))
        for match in re.finditer(r"typedef struct (\w+) \{(.*?)\} \1;", code, re.S):
            self.structs[match.group(1)] = [split_declaration(field) for field in match.group(2).split(";")
                                            if field.strip()]
            self.homes[match.group(1)] = home
        for match in re.finditer(r"^(\w[\w \*]*?)\b(prKit\w+)\(([^;{]*?)\);", code, re.M | re.S):
            parameters = [split_declaration(parameter) for parameter in match.group(3).split(",")]
            self.functions[match.group(2)] = (match.group(1).strip(), parameters)
            self.homes[match.group(2)] = home

    def keywords(self, type):
        """Returns the enumeration whose members are the keywords of a field of `type`, and whether the field takes
        several of them, as a set of flags does. Returns None for a type that is no enumeration of the kit's."""
        if self.widths.get(type) != "uint8_t":
            return None
        if type in self.members:
            return type, False
        if type.endswith("Flags") and type[:-1] in self.members:
            return type[:-1], True
        return None

    def values(self, enum):
        """Returns the members of an enumeration that are values, which a count of them is not."""
        return [member for member in self.members[enum] if member != "Count"]


def split_declaration(text):
    """Returns the type and the name of a field or a parameter. An array's extent is part of the type."""
    match = re.match(r"(.*?)(\w+)((?:\[[^\]]*\])*)$", " ".join(text.split()))
    return match.group(1).strip() + match.group(3), match.group(2)


def kebab(name):
    """Returns a field's or an enumeration member's name as an attribute or a keyword spells it: in lower case, with a
    hyphen between its words."""
    return re.sub(r"(?<=[a-z0-9])(?=[A-Z])", "-", name).lower()


class Kind:
    """A kind as the list of kinds gives it."""

    def __init__(self, name, function, line):
        self.name = name
        self.function = function
        self.line = line
        self.says = {word: [] for word in FIELD_WORDS}  # the fields that the kind's lines name, by the line's word
        self.renames = {}                                # a field's name -> its attribute
        self.events = []                                 # (type, query)
        self.under = None
        self.leaf = False
        self.pascal = "".join(word.capitalize() for word in name.split("-"))
        self.camel = self.pascal[0].lower() + self.pascal[1:]


def fail(line, message):
    raise SystemExit("%s:%d: %s" % (os.path.relpath(KINDS, ROOT), line, message))


def read_kinds():
    kinds = []
    with open(KINDS, encoding="utf-8") as f:
        lines = f.read().split("\n")
    for number, line in enumerate(lines, 1):
        words = line.split("#")[0].split()
        if not words:
            continue
        if not line[0].isspace():
            if len(words) != 2:
                fail(number, "a kind is the name of its element and the kit's function")
            if any(kind.name == words[0] for kind in kinds):
                fail(number, "the kind %s is listed twice" % words[0])
            kinds.append(Kind(words[0], words[1], number))
            continue
        if not kinds:
            fail(number, "a line that is indented follows a kind")
        kind = kinds[-1]
        if words[0] in FIELD_WORDS and len(words) == 2:
            kind.says[words[0]].append(words[1])
        elif words[0] == "attribute" and len(words) == 3:
            kind.renames[words[2]] = words[1]
        elif words[0] == "event" and len(words) == 3:
            kind.events.append((words[1], words[2]))
        elif words[0] == "under" and len(words) == 2 and kind.under is None:
            kind.under = words[1]
        elif words == ["leaf"]:
            kind.leaf = True
        else:
            fail(number, "not something that a kind says: %s" % " ".join(words))
    return kinds


class Field:
    """A field of a kind's data that the kit's function takes: a parameter, or a field of a parameter's struct."""

    def __init__(self, member, path, name, type):
        self.member = member  # the member of the kind's data that holds the parameter
        self.path = path      # the structs from the parameter's down to the field's, each with its member, as text
        self.name = name
        self.type = type
        self.attribute = kebab(name)
        self.form = None      # the name of its JsKitFieldType
        self.enum = None      # Keyword and Keywords: the enumeration

    def offset(self, kind):
        return " + ".join(["offsetof(%sData, %s)" % (kind.pascal, self.member)]
                          + ["offsetof(%s, %s)" % step for step in self.path])

    def owner(self):
        """Returns the name of the member of the kind's data that holds the field's value, for a form with one."""
        return self.name + "Value"


def fields_of(kit, member, struct, path=()):
    """Yields the fields of a parameter's struct, with those of the structs that it holds, which are the kit's too."""
    for type, name in kit.structs[struct]:
        step = path + ((struct, name),)
        if type in kit.structs and type not in TYPES and type != "PrKitPlace":
            yield from fields_of(kit, member, type, step)
        else:
            yield Field(member, step, name, type)


def describe(kit, kind):
    """Resolves what a kind's lines name against the kit's headers. Gives the kind the parameters of its function that
    its data holds, its attributes, and the fields that are its key, its text and its place, each None if it has
    none."""
    if kind.function not in kit.functions:
        fail(kind.line, "the kit's headers declare no %s" % kind.function)
    returns, parameters = kit.functions[kind.function]
    if returns != "PrLayoutNode" or [type for type, _ in parameters[:2]] != ["PrKit*", "PrLayoutNode"]:
        fail(kind.line, "%s is not a function of the kit and a parent that returns a node" % kind.function)
    kind.parameters = parameters[2:]
    kind.fields = []
    kind.key = kind.text = kind.place = None
    named = {word: list(names) for word, names in kind.says.items()}
    renamed = dict(kind.renames)
    for index, (type, member) in enumerate(kind.parameters):
        whole = [Field(member, (), member, type)]
        for field in fields_of(kit, member, type) if type in kit.structs and type not in TYPES else whole:
            if field.name in named["omit"]:
                named["omit"].remove(field.name)
            elif field.name in named["text"]:
                named["text"].remove(field.name)
                if field.type != "PrStringView" or kind.text is not None:
                    fail(kind.line, "the text %s is not one string" % field.name)
                kind.text = field
            elif index == 0 and len(field.path) == 1 and (field.type, field.name) == ("PrUiKey", "key"):
                kind.key = field
            elif (field.type, field.name) == ("PrKitPlace", "place") and kind.place is None:
                kind.place = field
            elif field.name.startswith("_pad"):
                continue
            else:
                keywords = kit.keywords(field.type)
                if field.type == "float":
                    field.form = "Length" if field.path and field.path[-1][0].endswith("Style") else "Float"
                    for form in ("number", "length"):
                        if field.name in named[form]:
                            named[form].remove(field.name)
                            field.form = "Float" if form == "number" else "Length"
                elif field.type in TYPES:
                    field.form = TYPES[field.type]
                elif keywords is not None:
                    field.enum, several = keywords
                    field.form = "Keywords" if several else "Keyword"
                else:
                    fail(kind.line, "no attribute has the form of %s, a %s" % (field.name, field.type))
                field.attribute = renamed.pop(field.name, field.attribute)
                kind.fields.append(field)
    for word, names in named.items():
        for name in names:
            fail(kind.line, "`%s %s` names no field that it applies to" % (word, name))
    for name in renamed:
        fail(kind.line, "`attribute %s %s` names no field" % (renamed[name], name))
    for names in ([field.attribute for field in kind.fields], [field.owner() for field in kind.fields]):
        for name in names:
            if names.count(name) > 1:
                fail(kind.line, "two fields are %s" % name)
    for _, query in kind.events:
        declared = kit.functions.get(query)
        if declared is None or declared[0] != "bool" or [type for type, _ in declared[1]] != ["const PrKit*",
                                                                                               "PrLayoutNode"]:
            fail(kind.line, "%s is not a query of the kit and a widget's node" % query)


def keywords_name(enum):
    return "k%sKeywords" % enum.removeprefix("PrKit")


def listed(names):
    """Returns names as a sentence lists them."""
    names = ["`%s`" % name for name in names]
    return names[0] if len(names) == 1 else "%s and %s" % (", ".join(names[:-1]), names[-1])


def reference(kind):
    """Returns the sentences that say what a kind shows, takes and queues."""
    out = ["%s." % kind.function]
    if kind.under is not None:
        out.append("Under a `%s` alone." % kind.under)
    if kind.text is not None:
        out.append("Shows its node's text as `%s`." % kind.text.name)
    takes = []
    at = 0
    while at < len(kind.fields):
        field = kind.fields[at]
        if field.form == "Keyword":
            takes.append("`%s` (%s)" % (field.attribute, field.enum))
        elif field.form == "Keywords":
            takes.append("`%s`, one or more of %s" % (field.attribute, field.enum))
        if field.enum is not None:
            at += 1
            continue
        # Fields of one form that follow one another are named together.
        end = at
        while end < len(kind.fields) and kind.fields[end].form == field.form:
            end += 1
        one, several = FORMS[field.form]
        takes.append("%s, %s" % (listed([f.attribute for f in kind.fields[at:end]]), one if end - at == 1 else several))
        at = end
    if takes:
        out.append("Takes %s." % "; ".join(takes))
    if kind.place is not None:
        out.append("Its style's place is the node's placement.")
    if kind.leaf:
        out.append("Holds no nodes.")
    if kind.events:
        out.append("Queues %s." % " and ".join("`%s` (%s)" % event for event in kind.events))
    return " ".join(out)


def reference_table(kinds):
    """Returns the comment lines of the kinds' reference: each kind's name, and beside it what the kind shows, takes
    and queues."""
    width = max(len(kind.name) for kind in kinds) + 3
    out = []
    for kind in kinds:
        lines = textwrap.wrap(reference(kind), LINE_LIMIT - len("//   ") - width, break_on_hyphens=False)
        out.append("//   %s%s\n" % (kind.name.ljust(width), lines[0]))
        out += ["//   %s%s\n" % (" " * width, line) for line in lines[1:]]
    return out


def kind_source(kit, kind):
    """Returns the definitions of one kind: its data, its fields, the call of its widget's function, and its events."""
    data = kind.pascal + "Data"
    out = ["\n// %s\n" % kind.name]
    if kind.parameters:
        out.append("\nstruct %s {\n" % data)
        out += ["    %s %s;\n" % parameter for parameter in kind.parameters]
        out += ["    %s %s;\n" % (OWNERS[field.form][0], field.owner()) for field in kind.fields
                if field.form in OWNERS]
        out.append("};\n")

    if kind.fields:
        out.append("\nconstexpr JsKitField k%sFields[] = {\n" % kind.pascal)
        for field in kind.fields:
            out += ["    {\n", "        .name = %s,\n" % c_string(field.attribute),
                    "        .type = JsKitFieldType_%s,\n" % field.form,
                    "        .offset = %s,\n" % field.offset(kind)]
            if field.form in OWNERS:
                out.append("        .owner = offsetof(%s, %s),\n" % (data, field.owner()))
            if field.enum is not None:
                out.append("        .keywords = %s,\n" % keywords_name(field.enum))
                out.append("        .keywordCount = %d,\n" % len(kit.values(field.enum)))
            out.append("    },\n")
        out.append("};\n")

    arguments = "".join(", held.%s" % member for _, member in kind.parameters)
    out.append("\nPrLayoutNode %sCreate(PrKit* kit, PrLayoutNode parent, const void*%s) {\n"
               % (kind.camel, " data" if kind.parameters else ""))
    if kind.parameters:
        out.append("    const %s& held = *static_cast<const %s*>(data);\n" % (data, data))
    out += ["    return %s(kit, parent%s);\n" % (kind.function, arguments), "}\n"]

    if kind.events:
        out.append("\nconstexpr PrStringView k%sEvents[] = {%s};\n"
                   % (kind.pascal, ", ".join(c_string(type) for type, _ in kind.events)))
        out.append("constexpr JsKitQueryFn* k%sQueries[] = {%s};\n"
                   % (kind.pascal, ", ".join(query for _, query in kind.events)))

    out += ["\nconstexpr JsKitWidget k%sWidget = {\n" % kind.pascal, "    .create = %sCreate,\n" % kind.camel]
    for name, field in (("key", kind.key), ("text", kind.text), ("place", kind.place)):
        out.append("    .%s = %s,\n" % (name, field.offset(kind) if field is not None else "kJsKitOffsetNone"))
    if kind.under is not None:
        out.append("    .under = %s,\n" % c_string(kind.under))
    if kind.events:
        out.append("    .queries = k%sQueries,\n" % kind.pascal)
    out.append("};\n")
    return out


def kind_entry(kind):
    """Returns a kind's entry in the table of kinds."""
    out = ["    {\n", "        .name = %s,\n" % c_string(kind.name)]
    if kind.parameters:
        out.append("        .dataSize = sizeof(%sData),\n" % kind.pascal)
    if kind.events:
        out += ["        .events = k%sEvents,\n" % kind.pascal, "        .eventCount = %d,\n" % len(kind.events)]
    flags = (["JsKitKindFlag_PlacesItself"] if kind.place is not None else []) + (
        ["JsKitKindFlag_DeclaresChildren"] if kind.leaf else [])
    if flags:
        out.append("        .flags = %s,\n" % " | ".join(flags))
    if kind.fields:
        out += ["        .fields = k%sFields,\n" % kind.pascal, "        .fieldCount = %d,\n" % len(kind.fields)]
    out += ["        .widget = &k%sWidget,\n" % kind.pascal, "        .declare = jsKitWidgetDeclare,\n", "    },\n"]
    return out


def read_kit():
    kit = Kit()
    for path in sorted(glob.glob(os.path.join(HEADERS, "kit*.h"))):
        kit.read(path)
    return kit


def source():
    """Returns the generated source, as the chunks of its text."""
    kit = read_kit()
    kinds = read_kinds()
    for kind in kinds:
        describe(kit, kind)

    # The enumerations that the kinds' fields are of, in the order that the kinds first name them, and the headers
    # that declare what the source names.
    enums = []
    homes = {"pr/kit.h", "pr/span.h", "pr/string_view.h"}
    for kind in kinds:
        homes.add(kit.homes[kind.function])
        homes.update(kit.homes[query] for _, query in kind.events)
        homes.update(kit.homes[type] for type, _ in kind.parameters if type in kit.homes)
        for field in kind.fields:
            if field.enum is not None and field.enum not in enums:
                enums.append(field.enum)
            if field.form in OWNERS:
                homes.add(OWNERS[field.form][1])

    out = [banner(__file__, "the headers of lib/studio/kit", os.path.basename(KINDS)),
              "\n// The kinds of Studio Kit's QuickJS binding that the kit's headers describe.\n//\n",
              "// A kind's widget is created by the function of the kit that is named beside it, from a content and,"
              " for most, a style. An attribute sets the field of its name in either, which the function's header"
              " describes: `note-tone` sets `noteTone`. Its value has the form that is given with it, which"
              " kit_node_quickjs.h says how to write. An attribute that is given with an enumeration takes the names"
              " of its members as keywords, in lower case with a hyphen between words, and a reset leaves the first"
              " of them. The nodes under a kind's node are declared in its widget, after the widget's own parts,"
              " unless the kind holds none. A node that is not under the kind that its kind goes under is not"
              " declared, and neither are the nodes under it. An event has no detail, and is queued in each round in"
              " which the kit's query that is named with it holds for the widget.\n//\n"]
    out += reference_table(kinds)
    out += ['\n#include "studio/kit/quickjs/src/kit_node_quickjs.h"\n\n']
    out += ['#include "%s"\n' % home for home in sorted(homes)]
    out += ["\nnamespace pr_impl {\n\nnamespace {\n"]
    for enum in enums:
        out.append("\nconstexpr JsKitKeyword %s[] = {\n" % keywords_name(enum))
        out += ["    {%s, %s_%s},\n" % (c_string(kebab(member)), enum, member) for member in kit.values(enum)]
        out.append("};\n")
    for kind in kinds:
        out += kind_source(kit, kind)
    out.append("\nconstexpr JsKitKind kKinds[] = {\n")
    for kind in kinds:
        out += kind_entry(kind)
    out += ["};\n", "\n} // namespace\n",
               "\nSpan<const JsKitKind> jsKitDescribedKinds() {\n    return kKinds;\n}\n",
               "\n} // namespace pr_impl\n"]
    return out


def main():
    write(OUT, source())


if __name__ == "__main__":
    main()
