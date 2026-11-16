// The kinds of Studio Kit's text (pr/kit_text.h) that are written by hand: `text` and `run`. Neither queues an event. A
// `label` is described (kit_node_quickjs_kinds.txt).
//
// A `text` (prKitText) is a paragraph, which takes the fields of PrKitTextStyle as a label does: `role`, `tone` and
// `wrap`, each a keyword of its field's enumeration, `text-align`, a keyword of PrKitTextAlign, and `preserve`, a flag.
// Its runs are the `run` nodes under it, in the tree's order. Its own text, if it has any, is a run before them, in the
// paragraph's role and tone. A node of any other kind under it is not declared.
//
// A `run` is a PrKitTextRun of the `text` that it is directly under. It shows its node's text, exactly as the script
// gave it, so the spaces between two runs' words are in a run's text. It takes `role` and `tone`, which are its own:
// reset, they are the body role and that role's tone, whatever the paragraph's are. A run under any other node declares
// nothing, and the nodes under a run are never declared.

#include "studio/kit/quickjs/src/kit_text_quickjs.h"

#include "pr/kit_text.h"
#include "pr/raw_array.h"

namespace pr_impl {

namespace {

constexpr JsKitKeyword kWraps[] = {
    {"auto", PrKitWrap_Auto},
    {"wrap", PrKitWrap_Wrap},
    {"nowrap", PrKitWrap_Nowrap},
    {"clip", PrKitWrap_Clip},
};

constexpr JsKitKeyword kTextAlignments[] = {
    {"start", PrKitTextAlign_Start},
    {"center", PrKitTextAlign_Center},
    {"end", PrKitTextAlign_End},
};

// What a paragraph is declared from.
struct TextData {
    PrKitTextStyle style;

    // The runs that the last round declared, whose texts are valid only during that round's call. The array is kept so
    // that a round allocates nothing once it has grown to the paragraph's runs.
    RawArray<PrKitTextRun> runs;
};

struct RunData {
    PrKitTextRole role;
    PrKitTone tone;
};

// Sets an attribute of how a label or a paragraph sets its text.
bool styleSet(PrKitTextStyle& style, PrStringView name, PrStringView value, bool reset) {
    if (name == "role")
        return jsKitSetKeyword(jsKitRoles(), value, reset, &style.role);
    if (name == "tone")
        return jsKitSetKeyword(jsKitTones(), value, reset, &style.tone);
    if (name == "wrap")
        return jsKitSetKeyword(kWraps, value, reset, &style.wrap);
    if (name == "text-align")
        return jsKitSetKeyword(kTextAlignments, value, reset, &style.align);
    if (name == "preserve")
        return jsKitSetFlag(value, reset, &style.preserve);
    return false;
}

bool runSet(void* data, PrStringView name, PrStringView value, bool reset) {
    RunData& run = *static_cast<RunData*>(data);
    if (name == "role")
        return jsKitSetKeyword(jsKitRoles(), value, reset, &run.role);
    if (name == "tone")
        return jsKitSetKeyword(jsKitTones(), value, reset, &run.tone);
    return false;
}

// A run declares nothing of its own: the paragraph that it is under reads it.
PrLayoutNode runDeclare(const JsKitRound&, JsKitNodeIndex, PrLayoutNode) {
    return kPrLayoutNodeNone;
}

void textDestroy(void* data) {
    static_cast<TextData*>(data)->runs.reset();
}

bool textSet(void* data, PrStringView name, PrStringView value, bool reset) {
    return styleSet(static_cast<TextData*>(data)->style, name, value, reset);
}

PrLayoutNode textDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    TextData& text = jsKitData<TextData>(round, node);
    const JsKitNodeTree* tree = round.tree;
    text.runs.clear();
    PrStringView own = jsKitNodeText(tree, node);
    if (own.size > 0)
        text.runs.push({.text = own, .role = text.style.role, .tone = text.style.tone});
    for (JsKitNodeIndex child = jsKitNodeFirstChild(tree, node); child != kJsKitNodeNone;
            child = jsKitNodeNextSibling(tree, child)) {
        if (jsKitNodeKind(tree, child)->declare != runDeclare)
            continue;
        const RunData& run = *static_cast<const RunData*>(jsKitNodeData(tree, child));
        text.runs.push({.text = jsKitNodeText(tree, child), .role = run.role, .tone = run.tone});
    }
    PrKitText paragraph{.key = jsKitNodeKey(round, node), .runs = text.runs.data, .count = text.runs.count};
    PrLayoutNode made = prKitText(round.kit, parent, paragraph, text.style);
    // The paragraph holds its runs alone, so it is placed here, and nothing is declared under it.
    if (jsKitNodeIsPlaced(tree, node))
        prKitPlace(round.kit, made, jsKitNodePlace(tree, node));
    return kPrLayoutNodeNone;
}

constexpr JsKitKind kKinds[] = {
    {.name = "text", .dataSize = sizeof(TextData), .destroy = textDestroy, .set = textSet, .declare = textDeclare},
    {.name = "run", .dataSize = sizeof(RunData), .set = runSet, .declare = runDeclare},
};

} // namespace

Span<const JsKitKind> jsKitTextKinds() {
    return kKinds;
}

} // namespace pr_impl
