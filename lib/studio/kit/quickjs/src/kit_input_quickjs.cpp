// The kinds of Studio Kit's inputs (pr/kit_input.h): `number-field`, `vector-field`, `text-field`, `text-area`,
// `text-area-style`, `suggest-field`, `slider`, `inspector-group`, `inspector-row` and `asset-field`.
//
// A control that edits a value takes the value as its `value`, and queues `change` with the new value in each round in
// which the user edits it. The node keeps the edit, so a control works with no listener, and a script that sets `value`
// to what an event gave it changes nothing. A script that sets another value replaces the edit, as a caller of the kit
// does that gives a control another value than the control reported. The text that a text control edits is its `value`
// too, which keeps every space. The text of the control's node is not shown.
//
// An attribute below takes one of the forms that every kind's attributes take (kit_node_quickjs.h), or one of two more.
// A number is finite, and is written as a script's conversion of a number to a string writes it, with an exponent if it
// has one. Lines are one string with a line feed between each two, since a line of a text field holds none. A keyword
// is one of the table that its attribute names.
//
// number-field (prKitNumberField)
//   Attributes
//     value          a number
//     type           the type that the kit stores the value in, a keyword of kScalarKinds, such as `i32`
//     step           a number that is zero or more
//     min, max       a number each, which bounds the value while it is set
//     axis, unit     strings, the tags before and after the value
//     precision      a whole number up to 255, the fraction digits that a float shows
//     disabled       a flag
//     size           a control size
//     axis-tone      a tone
//   Events
//     change         an edit, with the new number as `value`
//   The kit stores a value that its type cannot hold as the nearest value that it can, which is whole for an integer.
//   The node keeps the number that the script gave, for a later `type` that holds it. A 64-bit integer has the
//   precision of a script's number.
//
// vector-field (prKitVectorField)
//   Attributes
//     value          two to four numbers separated by spaces, one for each component, or two zeros until it is set
//     type, step, min, max, precision, disabled, size
//                    as a number field takes them, for every component
//     untagged       a flag
//   Events
//     change         an edit of a component, with every component as `values`
//   It takes no `axis-tone`, which the kit ignores for a vector.
//
// text-field (prKitTextField)
//   Attributes
//     value          the text
//     placeholder    a string
//     icon           an icon
//     history        lines, the earlier ones that Up and Down recall, oldest first
//     clearable, select-on-focus, disabled, mono, bare
//                    flags
//     input-kind     the kind of text, a keyword of kInputKinds, such as `password`
//     size           a control size
//   Events
//     change         an edit, a recalled line or a cleared field, with the new text as `text`
//     submit         Enter pressed while the field has focus, which the kit leaves to its caller, with the field's
//                    text as `text`
//
// text-area (prKitTextArea)
//   Attributes
//     value          the text
//     placeholder    a string
//     rows           a whole number
//     disabled, mono flags
//     size           a control size
//     runs           the styles of the text's ranges: pairs of whole numbers separated by spaces, each the position
//                    at which a run starts and the run's style. A run reaches to the next run's start. A position
//                    counts the UTF-16 code units before it, as a script's string does, and the starts ascend.
//   Children
//     text-area-style, one for each style that a run names, in order, with style zero first
//   Events
//     change         an edit, with the new text as `text`
//   It takes no `bare`, which the kit ignores for an area. While the area has runs, a buffer of the node's holds its
//   text (PrTextEditBuffer), and moves the runs with each edit: typed text takes the style of the text before it. A
//   `change` gives the text alone. A script that styles what the text holds, as a highlighter does, sets `runs` again
//   for the new text. A `value` that the script sets later is styled by the runs that the script gave last, counted
//   from the start of the new text.
//
// text-area-style (PrKitTextAreaStyle), which its text area reads, and which declares nothing itself
//   Attributes
//     role           a text role
//     tone           a tone
//
// suggest-field (prKitSuggestField)
//   Attributes
//     suggestions    lines, the values that the field's list offers. The script filters them as the text changes.
//     and those of a text field
//   Events
//     those of a text field. A chosen suggestion is a `change`.
//
// slider (prKitSlider)
//   Attributes
//     value, type    as a number field takes them
//     min, max       a number each, the ends of the range, which is 0 to 1 while both are zero
//     step           a number that is zero or more, the fraction of the track that a key press moves
//     scale          how the track maps onto the range, a keyword of kSliderScales, such as `ratio`
//     disabled       a flag
//     size           a control size
//   Events
//     change         an edit, with the new number as `value`
//   A ratio scale needs `min` above zero and `max` above `min`. The slider is linear until its range is so.
//
// inspector-group (prKitInspectorGroup)
//   Text             the group's name
//   Attributes
//     open           a flag, which the kit flips as the header is activated. A group is closed until it is set.
//     badge          a string, the tag after the name
//     badge-variant  a keyword of kBadgeVariants, such as `accent`
//   Children         the group's rows, which are declared while the group is open
//   Events
//     change         an activation of the header, with 1 as `value` if it opened the group, and 0 if it closed it
//   The placement attributes place the column of the rows, which is the node that the kit returns for a group.
//
// inspector-row (prKitInspectorRow)
//   Text             the row's label
//   Attributes
//     mono           a flag
//   Children         the row's controls
//   The placement attributes place the cell of the controls, which is the node that the kit returns for a row.
//
// asset-field (prKitAssetField)
//   Text             the asset's name
//   Attributes
//     icon           an icon
//     tint           a color (jsKitSetColor)
//     kind           the kind of payload that the field takes, a whole number of up to 64 bits
//     missing, empty, disabled
//                    flags
//   Events
//     browse, locate, clear
//                    an activation of that button of the field's gutter
//     drop           a payload of the field's kind dropped on the field, with the payload's value as `value`, and
//                    the value's decimal digits as `text`, since a script's number holds only 53 bits of the value
//   A payload comes from a drag that another widget starts (prUiDragStart).
//
// A text field's buffer is not bound. A text field shows a buffer's text plain, which is what `value` gives it.

#include "studio/kit/quickjs/src/kit_input_quickjs.h"

#include "pr/float.h"
#include "pr/float_convert.h"
#include "pr/integer.h"
#include "pr/kit_button.h"
#include "pr/kit_input.h"
#include "pr/mem.h"
#include "pr/raw_array.h"
#include "pr/raw_string.h"

namespace pr_impl {

namespace {

// ---------------------------------------------------------------------------------------------------------------------
// Numbers

// Reads a finite number that is the whole of `text`, with an exponent if it has one.
bool numberFrom(PrStringView text, double* out) {
    double number = 0;
    if (text.size == 0 || prF64ParsePrefix(text, &number) != text.size || !prF64IsFinite(number))
        return false;
    *out = number;
    return true;
}

// Read a number, and a number that is zero or more. With `reset`, each writes zero.
bool setNumber(PrStringView value, bool reset, double* out) {
    if (reset) {
        *out = 0;
        return true;
    }
    return numberFrom(value, out);
}
bool setAmount(PrStringView value, bool reset, double* out) {
    double amount = 0;
    if (!setNumber(value, reset, &amount) || amount < 0)
        return false;
    *out = amount;
    return true;
}

// Reads a bound of a range, which holds while it is set, or with `reset` removes it.
bool setBound(PrStringView value, bool reset, double* out, bool* has) {
    if (!setNumber(value, reset, out))
        return false;
    *has = !reset;
    return true;
}

// Sets `step`, `min` or `max` of a numeric field's range, one of which `name` is.
bool rangeSet(PrKitNumberRange& range, PrStringView name, PrStringView value, bool reset) {
    if (name == "step")
        return setAmount(value, reset, &range.step);
    if (name == "min")
        return setBound(value, reset, &range.min, &range.hasMin);
    return setBound(value, reset, &range.max, &range.hasMax);
}

// Reads the number of fraction digits that a numeric field shows.
bool setPrecision(PrStringView value, bool reset, uint8_t* out) {
    uint32_t digits = 0;
    if (!jsKitSetUint(value, reset, &digits) || digits > 255)
        return false;
    *out = uint8_t(digits);
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Scalars

constexpr JsKitKeyword kScalarKinds[] = {
    {"f32", PrKitScalarKind_F32},
    {"f64", PrKitScalarKind_F64},
    {"i8", PrKitScalarKind_I8},
    {"i16", PrKitScalarKind_I16},
    {"i32", PrKitScalarKind_I32},
    {"i64", PrKitScalarKind_I64},
    {"u8", PrKitScalarKind_U8},
    {"u16", PrKitScalarKind_U16},
    {"u32", PrKitScalarKind_U32},
    {"u64", PrKitScalarKind_U64},
};

// The range of values that a scalar type holds. The range of a 64-bit integer ends at the doubles within its limits.
struct ScalarRange {
    double low;
    double high;
    bool whole;                     // the type holds integers
};

constexpr ScalarRange kScalarRanges[] = { // by PrKitScalarKind
    {-3.4028234663852886e38, 3.4028234663852886e38, false},
    {-1.7976931348623157e308, 1.7976931348623157e308, false},
    {-128, 127, true},
    {-32768, 32767, true},
    {-2147483648.0, 2147483647.0, true},
    {-9223372036854775808.0, 9223372036854774784.0, true},
    {0, 255, true},
    {0, 65535, true},
    {0, 4294967295.0, true},
    {0, 18446744073709549568.0, true},
};

// The most numbers that a control's value has, which is the components of a vector.
constexpr uint32_t kMaxComponents = 4;

// The values that the kit edits through a PrKitScalar: up to four of one type, one after another.
union ScalarStorage {
    float floats[kMaxComponents];
    double doubles[kMaxComponents];
    int8_t int8s[kMaxComponents];
    int16_t int16s[kMaxComponents];
    int32_t int32s[kMaxComponents];
    int64_t int64s[kMaxComponents];
    uint8_t uint8s[kMaxComponents];
    uint16_t uint16s[kMaxComponents];
    uint32_t uint32s[kMaxComponents];
    uint64_t uint64s[kMaxComponents];
};

// The value of a numeric control.
struct ScalarValue {
    // The numbers as the script set them or the kit last edited them. The storage holds the nearest value of its type
    // to each, so a number that one type cannot hold is still here for a type that can.
    double numbers[kMaxComponents];

    ScalarStorage storage;
    PrKitScalarKind type;           // the type of the storage's values
};

// Writes `number` as value `index` of the storage, in the type `kind`: the nearest value that the type holds.
void scalarStore(ScalarStorage& storage, PrKitScalarKind kind, uint32_t index, double number) {
    const ScalarRange& range = kScalarRanges[kind];
    double held = range.whole ? prF64Round(number) : number;
    held = held < range.low ? range.low : held > range.high ? range.high : held;
    switch (kind) {
        case PrKitScalarKind_F32:
            storage.floats[index] = float(held);
            return;
        case PrKitScalarKind_I8:
            storage.int8s[index] = int8_t(held);
            return;
        case PrKitScalarKind_I16:
            storage.int16s[index] = int16_t(held);
            return;
        case PrKitScalarKind_I32:
            storage.int32s[index] = int32_t(held);
            return;
        case PrKitScalarKind_I64:
            storage.int64s[index] = int64_t(held);
            return;
        case PrKitScalarKind_U8:
            storage.uint8s[index] = uint8_t(held);
            return;
        case PrKitScalarKind_U16:
            storage.uint16s[index] = uint16_t(held);
            return;
        case PrKitScalarKind_U32:
            storage.uint32s[index] = uint32_t(held);
            return;
        case PrKitScalarKind_U64:
            storage.uint64s[index] = uint64_t(held);
            return;
        default:
            storage.doubles[index] = held;
            return;
    }
}

// Returns value `index` of the storage, whose values are of the type `kind`.
double scalarLoad(const ScalarStorage& storage, PrKitScalarKind kind, uint32_t index) {
    switch (kind) {
        case PrKitScalarKind_F32:
            return double(storage.floats[index]);
        case PrKitScalarKind_I8:
            return double(storage.int8s[index]);
        case PrKitScalarKind_I16:
            return double(storage.int16s[index]);
        case PrKitScalarKind_I32:
            return double(storage.int32s[index]);
        case PrKitScalarKind_I64:
            return double(storage.int64s[index]);
        case PrKitScalarKind_U8:
            return double(storage.uint8s[index]);
        case PrKitScalarKind_U16:
            return double(storage.uint16s[index]);
        case PrKitScalarKind_U32:
            return double(storage.uint32s[index]);
        case PrKitScalarKind_U64:
            return double(storage.uint64s[index]);
        default:
            return storage.doubles[index];
    }
}

// Writes the value's numbers to its storage, in its type.
void scalarSync(ScalarValue& scalar) {
    scalar.storage = ScalarStorage{};
    for (uint32_t i = 0; i < kMaxComponents; ++i)
        scalarStore(scalar.storage, scalar.type, i, scalar.numbers[i]);
}

// Sets the type that a value is stored in, which keeps the value's numbers.
bool scalarSetType(ScalarValue& scalar, PrStringView value, bool reset) {
    if (!jsKitSetKeyword(kScalarKinds, value, reset, &scalar.type))
        return false;
    scalarSync(scalar);
    return true;
}

// Sets a value's numbers, which spaces separate, and writes how many there are to `count`. There must be `low` to
// `high` of them. With `reset`, writes `low` zeros.
bool scalarSetNumbers(ScalarValue& scalar, PrStringView value, bool reset, uint32_t low, uint32_t high, uint8_t* count) {
    double numbers[kMaxComponents] = {};
    uint32_t read = reset ? low : 0;
    size_t at = 0;
    while (!reset && at < value.size) {
        if (value.data[at] == ' ') {
            ++at;
            continue;
        }
        size_t end = at;
        while (end < value.size && value.data[end] != ' ')
            ++end;
        if (read == high || !numberFrom(PrStringView{value.data + at, end - at}, &numbers[read]))
            return false;
        ++read;
        at = end;
    }
    if (read < low)
        return false;
    for (uint32_t i = 0; i < kMaxComponents; ++i)
        scalar.numbers[i] = numbers[i];
    *count = uint8_t(read);
    scalarSync(scalar);
    return true;
}

// The events of a control that edits one value, by their index.
enum { ValueEvent_Change };
constexpr PrStringView kValueEvents[] = {"change"};

// Queues a change with the first `count` values of the storage, if the kit's control edited them since `before` was
// copied from the storage, and takes them as the value's numbers.
void scalarReport(const JsKitRound& round, JsKitNodeIndex node, ScalarValue& scalar, const ScalarStorage& before,
        uint32_t count) {
    if (prMemEquals(&before, &scalar.storage, sizeof before))
        return;
    JsKitDetail detail{.valueCount = count};
    for (uint32_t i = 0; i < count; ++i)
        detail.values[i] = scalar.numbers[i] = scalarLoad(scalar.storage, scalar.type, i);
    jsKitEventPush(round, node, ValueEvent_Change, &detail);
}

// ---------------------------------------------------------------------------------------------------------------------
// Numeric fields

struct NumberData {
    ScalarValue value;
    PrKitNumberRange range;
    PrRawString axis;
    PrRawString unit;
    PrKitNumberStyle style;
    uint8_t precision;
    bool disabled;
};

void numberDestroy(void* data) {
    NumberData& number = *static_cast<NumberData*>(data);
    prRawStringReset(&number.axis);
    prRawStringReset(&number.unit);
}

bool numberSet(void* data, PrStringView name, PrStringView value, bool reset) {
    NumberData& number = *static_cast<NumberData*>(data);
    uint8_t count = 0;
    if (name == "value")
        return scalarSetNumbers(number.value, value, reset, 1, 1, &count);
    if (name == "type")
        return scalarSetType(number.value, value, reset);
    if (name == "step" || name == "min" || name == "max")
        return rangeSet(number.range, name, value, reset);
    if (name == "axis")
        return jsKitSetString(value, reset, &number.axis);
    if (name == "unit")
        return jsKitSetString(value, reset, &number.unit);
    if (name == "precision")
        return setPrecision(value, reset, &number.precision);
    if (name == "disabled")
        return jsKitSetFlag(value, reset, &number.disabled);
    if (name == "size")
        return jsKitSetKeyword(jsKitControlSizes(), value, reset, &number.style.size);
    if (name == "axis-tone")
        return jsKitSetKeyword(jsKitTones(), value, reset, &number.style.axisTone);
    return false;
}

PrLayoutNode numberDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    NumberData& number = jsKitData<NumberData>(round, node);
    ScalarStorage before = number.value.storage;
    PrKitNumber content{
        .key = jsKitNodeKey(round, node),
        .value = {&number.value.storage, number.value.type},
        .range = number.range,
        .axis = prRawStringView(&number.axis),
        .unit = prRawStringView(&number.unit),
        .precision = number.precision,
        .disabled = number.disabled,
    };
    PrLayoutNode made = prKitNumberField(round.kit, parent, content, number.style);
    scalarReport(round, node, number.value, before, 1);
    return made;
}

struct VectorData {
    ScalarValue value;
    PrKitNumberRange range;
    PrKitNumberStyle style;
    uint8_t count;                  // the components that the script gave, or 0 before it gave any
    uint8_t precision;
    bool untagged;
    bool disabled;
};

// The fewest components that a vector has, which a vector field has before the script gives it a value.
constexpr uint32_t kMinComponents = 2;

bool vectorSet(void* data, PrStringView name, PrStringView value, bool reset) {
    VectorData& vector = *static_cast<VectorData*>(data);
    if (name == "value")
        return scalarSetNumbers(vector.value, value, reset, kMinComponents, kMaxComponents, &vector.count);
    if (name == "type")
        return scalarSetType(vector.value, value, reset);
    if (name == "step" || name == "min" || name == "max")
        return rangeSet(vector.range, name, value, reset);
    if (name == "precision")
        return setPrecision(value, reset, &vector.precision);
    if (name == "untagged")
        return jsKitSetFlag(value, reset, &vector.untagged);
    if (name == "disabled")
        return jsKitSetFlag(value, reset, &vector.disabled);
    if (name == "size")
        return jsKitSetKeyword(jsKitControlSizes(), value, reset, &vector.style.size);
    return false;
}

PrLayoutNode vectorDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    VectorData& vector = jsKitData<VectorData>(round, node);
    ScalarStorage before = vector.value.storage;
    uint32_t count = vector.count != 0 ? vector.count : kMinComponents;
    PrKitVector content{
        .key = jsKitNodeKey(round, node),
        .value = {&vector.value.storage, vector.value.type},
        .count = count,
        .range = vector.range,
        .precision = vector.precision,
        .untagged = vector.untagged,
        .disabled = vector.disabled,
    };
    PrLayoutNode made = prKitVectorField(round.kit, parent, content, vector.style);
    scalarReport(round, node, vector.value, before, count);
    return made;
}

// ---------------------------------------------------------------------------------------------------------------------
// Text

// The lines that an attribute gives as one string, with a line feed between each two.
struct Lines {
    PrRawString text;
    RawArray<PrStringView> lines;   // ranges of `text`
};

bool linesSet(Lines& lines, PrStringView value, bool reset) {
    jsKitSetString(value, reset, &lines.text);
    lines.lines.clear();
    PrStringView text = prRawStringView(&lines.text);
    size_t start = 0;
    for (size_t at = 0; text.size != 0 && at <= text.size; ++at) {
        if (at < text.size && text.data[at] != '\n')
            continue;
        lines.lines.push(PrStringView{text.data + start, at - start});
        start = at + 1;
    }
    return true;
}

void linesReset(Lines& lines) {
    prRawStringReset(&lines.text);
    lines.lines.reset();
}

// Returns the text of a control's value, which is empty before the script sets it.
PrStringView valueText(const PrRawString& value) {
    PrStringView text = prRawStringView(&value);
    return text.isNull() ? PrStringView("") : text;
}

// Reads the text that a control edits, which is no longer than an edit's buffer holds.
bool setText(PrStringView value, bool reset, PrRawString* out) {
    return value.size <= size_t(kPrTextEditBufferMaxBytes) && jsKitSetString(value, reset, out);
}

// Sets an attribute of a text control's style. Returns false if `name` is none, or `value` does not suit it.
bool fieldStyleSet(PrKitFieldStyle& style, PrStringView name, PrStringView value, bool reset) {
    if (name == "size")
        return jsKitSetKeyword(jsKitControlSizes(), value, reset, &style.size);
    if (name == "mono")
        return jsKitSetFlag(value, reset, &style.mono);
    return false;
}

constexpr JsKitKeyword kInputKinds[] = {
    {"text", PrAppTextInputKind_Text},
    {"password", PrAppTextInputKind_Password},
    {"number", PrAppTextInputKind_Number},
    {"decimal", PrAppTextInputKind_Decimal},
    {"phone", PrAppTextInputKind_Phone},
    {"email", PrAppTextInputKind_Email},
    {"url", PrAppTextInputKind_Url},
    {"search", PrAppTextInputKind_Search},
};

// The events of a text field and a suggest field, by their index.
enum { FieldEvent_Change, FieldEvent_Submit };
constexpr PrStringView kFieldEvents[] = {"change", "submit"};

// What a text field and a suggest field take.
struct FieldData {
    PrRawString value;
    PrRawString placeholder;
    Lines history;
    Lines suggestions;              // of a suggest field
    PrKitFieldStyle style;
    PrKitIconKind icon;
    PrAppTextInputKind inputKind;
    bool clearable;
    bool selectOnFocus;
    bool disabled;
};

void fieldDestroy(void* data) {
    FieldData& field = *static_cast<FieldData*>(data);
    prRawStringReset(&field.value);
    prRawStringReset(&field.placeholder);
    linesReset(field.history);
    linesReset(field.suggestions);
}

bool fieldSet(void* data, PrStringView name, PrStringView value, bool reset) {
    FieldData& field = *static_cast<FieldData*>(data);
    if (name == "value")
        return setText(value, reset, &field.value);
    if (name == "placeholder")
        return jsKitSetString(value, reset, &field.placeholder);
    if (name == "icon")
        return jsKitSetIcon(value, reset, &field.icon);
    if (name == "history")
        return linesSet(field.history, value, reset);
    if (name == "clearable")
        return jsKitSetFlag(value, reset, &field.clearable);
    if (name == "select-on-focus")
        return jsKitSetFlag(value, reset, &field.selectOnFocus);
    if (name == "disabled")
        return jsKitSetFlag(value, reset, &field.disabled);
    if (name == "input-kind")
        return jsKitSetKeyword(kInputKinds, value, reset, &field.inputKind);
    if (name == "bare")
        return jsKitSetFlag(value, reset, &field.style.bare);
    return fieldStyleSet(field.style, name, value, reset);
}

bool suggestSet(void* data, PrStringView name, PrStringView value, bool reset) {
    if (name == "suggestions")
        return linesSet(static_cast<FieldData*>(data)->suggestions, value, reset);
    return fieldSet(data, name, value, reset);
}

// Returns what the kit's text field shows and edits for a node's data.
PrKitTextField fieldContent(const JsKitRound& round, JsKitNodeIndex node, const FieldData& field) {
    return PrKitTextField{
        .key = jsKitNodeKey(round, node),
        .text = valueText(field.value),
        .placeholder = prRawStringView(&field.placeholder),
        .icon = field.icon,
        .history = field.history.lines.data,
        .historyCount = field.history.lines.count,
        .clearable = field.clearable,
        .selectOnFocus = field.selectOnFocus,
        .disabled = field.disabled,
        .inputKind = field.inputKind,
    };
}

// Takes the text that the kit's control `made` changed to in the round, if it changed, as the node's value, and queues
// the change at index `event` of the kind's events.
void textReport(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode made, PrRawString& value, uint32_t event) {
    PrStringView changed = {};
    if (!prKitTextChanged(round.kit, made, &changed))
        return;
    prRawStringReplace(&value, changed);
    JsKitDetail detail{.hasText = true, .text = valueText(value)};
    jsKitEventPush(round, node, event, &detail);
}

// Queues what a round did to a text field or a suggest field: a change of its text, and a submit if Enter was pressed
// in it, which the kit leaves to its caller.
void fieldReport(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode made, FieldData& field) {
    textReport(round, node, made, field.value, FieldEvent_Change);
    PrUiEvents events = prUiEvents(round.kit->ui, made);
    for (uint32_t i = 0; i < events.count; ++i) {
        const PrAppEvent& event = events.events[i];
        if (event.kind != PrAppEventKind_KeyDown || event.keyboard.key != PrAppKey_Enter || event.keyboard.composing)
            continue;
        JsKitDetail detail{.hasText = true, .text = valueText(field.value)};
        jsKitEventPush(round, node, FieldEvent_Submit, &detail);
    }
}

PrLayoutNode fieldDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    FieldData& field = jsKitData<FieldData>(round, node);
    PrLayoutNode made = prKitTextField(round.kit, parent, fieldContent(round, node, field), field.style);
    fieldReport(round, node, made, field);
    return made;
}

PrLayoutNode suggestDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    FieldData& field = jsKitData<FieldData>(round, node);
    PrKitSuggestField content{
        .field = fieldContent(round, node, field),
        .suggestions = field.suggestions.lines.data,
        .count = field.suggestions.lines.count,
    };
    PrLayoutNode made = prKitSuggestField(round.kit, parent, content, field.style);
    fieldReport(round, node, made, field);
    return made;
}

// What a text area takes. Its text is a value, as a text field's is, until the script styles ranges of it. From then on
// a buffer holds the text, because the styles of a text's ranges are a buffer's.
struct AreaData {
    PrRawString value;                      // the text, while there is no buffer
    PrRawString placeholder;
    PrKitFieldStyle style;
    uint32_t rows;
    bool disabled;

    PrTextEditBuffer* PR_NULLABLE buffer;   // the text and the styles of its ranges, owned
    RawArray<PrTextEditStyleRun> runs;      // as the script gave them, each start in UTF-16 code units
    RawArray<PrKitTextAreaStyle> styles;    // by PrTextEditStyle, from the node's children in each round
};

void areaDestroy(void* data) {
    AreaData& area = *static_cast<AreaData*>(data);
    prRawStringReset(&area.value);
    prRawStringReset(&area.placeholder);
    prTextEditBufferDestroy(area.buffer);
    area.runs.reset();
    area.styles.reset();
}

// Gives the ranges of the buffer's text the styles of the script's runs. A script counts a position in a string in
// UTF-16 code units, and the buffer in bytes of UTF-8. A start inside a code point, or past the text, is the nearest
// position before it.
void areaStyleRuns(AreaData& area) {
    PrStringView text = prTextEditBufferText(area.buffer);
    uint32_t size = uint32_t(text.size);
    prTextEditBufferSetStyle(area.buffer, 0, size, 0);
    uint32_t at = 0;
    uint32_t units = 0;
    for (uint32_t i = 0; i < area.runs.count; ++i) {
        // A code point of four bytes is two code units, and any other is one.
        while (at < size) {
            uint8_t lead = uint8_t(text.data[at]);
            uint32_t bytes = lead < 0xC0 ? 1 : lead < 0xE0 ? 2 : lead < 0xF0 ? 3 : 4;
            if (units + (bytes == 4 ? 2 : 1) > area.runs.data[i].start || at + bytes > size)
                break;
            units += bytes == 4 ? 2 : 1;
            at += bytes;
        }
        // The style holds up to the next run's start, where the next run replaces it.
        prTextEditBufferSetStyle(area.buffer, at, size, area.runs.data[i].style);
    }
}

// Replaces the buffer's text with `text` by the one change that turns the old text into the new, so that a caret
// outside the change stays with its text.
void areaReplaceText(AreaData& area, PrStringView text) {
    PrStringView held = prTextEditBufferText(area.buffer);
    size_t prefix = 0;
    while (prefix < held.size && prefix < text.size && held.data[prefix] == text.data[prefix])
        ++prefix;
    size_t suffix = 0;
    while (suffix < held.size - prefix && suffix < text.size - prefix &&
            held.data[held.size - 1 - suffix] == text.data[text.size - 1 - suffix])
        ++suffix;
    // The change begins and ends between code points, which the bytes that the texts share need not.
    while (prefix > 0 && prefix < text.size && (uint8_t(text.data[prefix]) & 0xC0) == 0x80)
        --prefix;
    while (suffix > 0 && (uint8_t(text.data[text.size - suffix]) & 0xC0) == 0x80)
        --suffix;
    PrTextEditChange change{
        .start = uint32_t(prefix),
        .end = uint32_t(held.size - suffix),
        .text = {text.data + prefix, text.size - suffix - prefix},
    };
    prTextEditBufferApply(area.buffer, &change, 1, PrTextEditOrigin_Program);
}

// Sets the text. The script's runs style the new text, since they count from its start.
bool areaSetValue(AreaData& area, PrStringView value, bool reset) {
    if (area.buffer == nullptr)
        return setText(value, reset, &area.value);
    PrStringView text = reset ? PrStringView("") : value;
    if (text.size > size_t(kPrTextEditBufferMaxBytes))
        return false;
    if (prTextEditBufferText(area.buffer) == text)
        return true;
    areaReplaceText(area, text);
    areaStyleRuns(area);
    return true;
}

// Sets the styles of the text's ranges: pairs of whole numbers separated by spaces, each the position at which a run
// starts and the run's style, with the starts ascending. The text moves into a buffer while it has any runs.
bool areaSetRuns(AreaData& area, PrStringView value, bool reset) {
    RawArray<PrTextEditStyleRun> runs;
    uint32_t numbers = 0;
    size_t at = 0;
    while (!reset && at < value.size) {
        if (value.data[at] == ' ') {
            ++at;
            continue;
        }
        size_t end = at;
        while (end < value.size && value.data[end] != ' ')
            ++end;
        uint32_t number = 0;
        bool start = numbers % 2 == 0;
        if (!jsKitSetUint(PrStringView{value.data + at, end - at}, false, &number) ||
                (start && runs.count != 0 && number <= runs.last().start)) {
            runs.reset();
            return false;
        }
        if (start)
            runs.push(PrTextEditStyleRun{.start = number});
        else
            runs.last().style = number;
        ++numbers;
        at = end;
    }
    if (numbers % 2 != 0) {
        runs.reset();
        return false;
    }
    area.runs.reset();
    area.runs = runs;
    if (runs.count == 0) {
        if (area.buffer != nullptr)
            prRawStringReplace(&area.value, prTextEditBufferText(area.buffer));
        prTextEditBufferDestroy(area.buffer);
        area.buffer = nullptr;
        return true;
    }
    if (area.buffer == nullptr) {
        area.buffer = prTextEditBufferCreate(valueText(area.value));
        prRawStringReset(&area.value);
    }
    areaStyleRuns(area);
    return true;
}

bool areaSet(void* data, PrStringView name, PrStringView value, bool reset) {
    AreaData& area = *static_cast<AreaData*>(data);
    if (name == "value")
        return areaSetValue(area, value, reset);
    if (name == "runs")
        return areaSetRuns(area, value, reset);
    if (name == "placeholder")
        return jsKitSetString(value, reset, &area.placeholder);
    if (name == "rows")
        return jsKitSetUint(value, reset, &area.rows);
    if (name == "disabled")
        return jsKitSetFlag(value, reset, &area.disabled);
    return fieldStyleSet(area.style, name, value, reset);
}

bool areaStyleSet(void* data, PrStringView name, PrStringView value, bool reset) {
    PrKitTextAreaStyle& style = *static_cast<PrKitTextAreaStyle*>(data);
    if (name == "role")
        return jsKitSetKeyword(jsKitRoles(), value, reset, &style.role);
    if (name == "tone")
        return jsKitSetKeyword(jsKitTones(), value, reset, &style.tone);
    return false;
}

// A style of a text area is what its area reads, and declares nothing of its own.
PrLayoutNode areaStyleDeclare(const JsKitRound&, JsKitNodeIndex, PrLayoutNode) {
    return kPrLayoutNodeNone;
}

PrLayoutNode areaDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    AreaData& area = jsKitData<AreaData>(round, node);
    area.styles.clear();
    for (JsKitNodeIndex child = jsKitNodeFirstChild(round.tree, node); child != kJsKitNodeNone;
            child = jsKitNodeNextSibling(round.tree, child)) {
        if (jsKitNodeKind(round.tree, child)->declare == areaStyleDeclare)
            area.styles.push(*static_cast<const PrKitTextAreaStyle*>(jsKitNodeData(round.tree, child)));
    }
    PrKitTextArea content{
        .key = jsKitNodeKey(round, node),
        .text = valueText(area.value),
        .buffer = area.buffer,
        .placeholder = prRawStringView(&area.placeholder),
        .rows = area.rows,
        .disabled = area.disabled,
        .styles = area.styles.data,
        .styleCount = area.styles.count,
    };
    PrLayoutNode made = prKitTextArea(round.kit, parent, content, area.style);
    if (area.buffer == nullptr) {
        textReport(round, node, made, area.value, ValueEvent_Change);
        return made;
    }
    // The kit's edit changed the buffer itself, so the node has the new text already.
    PrStringView changed = {};
    if (prKitTextChanged(round.kit, made, &changed)) {
        JsKitDetail detail{.hasText = true, .text = changed};
        jsKitEventPush(round, node, ValueEvent_Change, &detail);
    }
    return made;
}

// ---------------------------------------------------------------------------------------------------------------------
// Sliders

constexpr JsKitKeyword kSliderScales[] = {{"linear", PrKitSliderScale_Linear}, {"ratio", PrKitSliderScale_Ratio}};

struct SliderData {
    ScalarValue value;
    double min;
    double max;
    float step;
    PrKitSliderScale scale;
    bool disabled;
    PrKitSliderStyle style;
};

bool sliderSet(void* data, PrStringView name, PrStringView value, bool reset) {
    SliderData& slider = *static_cast<SliderData*>(data);
    uint8_t count = 0;
    double step = 0;
    if (name == "value")
        return scalarSetNumbers(slider.value, value, reset, 1, 1, &count);
    if (name == "type")
        return scalarSetType(slider.value, value, reset);
    if (name == "min")
        return setNumber(value, reset, &slider.min);
    if (name == "max")
        return setNumber(value, reset, &slider.max);
    if (name == "step") {
        if (!setAmount(value, reset, &step))
            return false;
        slider.step = float(step);
        return true;
    }
    if (name == "scale")
        return jsKitSetKeyword(kSliderScales, value, reset, &slider.scale);
    if (name == "disabled")
        return jsKitSetFlag(value, reset, &slider.disabled);
    if (name == "size")
        return jsKitSetKeyword(jsKitControlSizes(), value, reset, &slider.style.size);
    return false;
}

PrLayoutNode sliderDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    SliderData& slider = jsKitData<SliderData>(round, node);
    ScalarStorage before = slider.value.storage;
    // The kit's ratio scale divides by the low end of a range that rises from above zero, which a script's attributes
    // need not give, and never in one step.
    bool ratio = slider.scale == PrKitSliderScale_Ratio && slider.min > 0 && slider.max > slider.min;
    PrKitSlider content{
        .key = jsKitNodeKey(round, node),
        .value = {&slider.value.storage, slider.value.type},
        .min = slider.min,
        .max = slider.max,
        .step = slider.step,
        .scale = PrKitSliderScale(ratio ? PrKitSliderScale_Ratio : PrKitSliderScale_Linear),
        .disabled = slider.disabled,
    };
    PrLayoutNode made = prKitSlider(round.kit, parent, content, slider.style);
    scalarReport(round, node, slider.value, before, 1);
    return made;
}

// ---------------------------------------------------------------------------------------------------------------------
// The inspector

constexpr JsKitKeyword kBadgeVariants[] = {
    {"neutral", PrKitBadgeVariant_Neutral},
    {"accent", PrKitBadgeVariant_Accent},
    {"success", PrKitBadgeVariant_Success},
    {"warning", PrKitBadgeVariant_Warning},
    {"danger", PrKitBadgeVariant_Danger},
};

struct GroupData {
    PrRawString badge;
    PrKitBadgeVariant badgeVariant;
    bool open;                      // the flag that the kit flips as the header is activated
};

void groupDestroy(void* data) {
    prRawStringReset(&static_cast<GroupData*>(data)->badge);
}

bool groupSet(void* data, PrStringView name, PrStringView value, bool reset) {
    GroupData& group = *static_cast<GroupData*>(data);
    if (name == "open")
        return jsKitSetFlag(value, reset, &group.open);
    if (name == "badge")
        return jsKitSetString(value, reset, &group.badge);
    if (name == "badge-variant")
        return jsKitSetKeyword(kBadgeVariants, value, reset, &group.badgeVariant);
    return false;
}

PrLayoutNode groupDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    GroupData& group = jsKitData<GroupData>(round, node);
    bool before = group.open;
    PrKitInspectorGroup content{
        .key = jsKitNodeKey(round, node),
        .name = jsKitNodeText(round.tree, node),
        .badge = prRawStringView(&group.badge),
        .badgeVariant = group.badgeVariant,
        .open = &group.open,
    };
    PrLayoutNode body = prKitInspectorGroup(round.kit, parent, content);
    if (group.open != before) {
        JsKitDetail detail{.values = {group.open ? 1.0 : 0.0}, .valueCount = 1};
        jsKitEventPush(round, node, ValueEvent_Change, &detail);
    }
    return body;
}

bool rowSet(void* data, PrStringView name, PrStringView value, bool reset) {
    PrKitInspectorRowStyle& style = *static_cast<PrKitInspectorRowStyle*>(data);
    if (name == "mono")
        return jsKitSetFlag(value, reset, &style.mono);
    return false;
}

PrLayoutNode rowDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    PrKitInspectorRow content{.key = jsKitNodeKey(round, node), .label = jsKitNodeText(round.tree, node)};
    return prKitInspectorRow(round.kit, parent, content, jsKitData<PrKitInspectorRowStyle>(round, node));
}

// The events of an asset field, by their index.
enum { AssetEvent_Browse, AssetEvent_Locate, AssetEvent_Clear, AssetEvent_Drop };
constexpr PrStringView kAssetEvents[] = {"browse", "locate", "clear", "drop"};

struct AssetData {
    PrColor tint;
    uint64_t kind;
    PrKitIconKind icon;
    bool missing;
    bool empty;
    bool disabled;
};

bool assetSet(void* data, PrStringView name, PrStringView value, bool reset) {
    AssetData& asset = *static_cast<AssetData*>(data);
    if (name == "icon")
        return jsKitSetIcon(value, reset, &asset.icon);
    if (name == "tint")
        return jsKitSetColor(value, reset, &asset.tint);
    if (name == "kind") {
        uint64_t kind = 0;
        if (!reset && !prU64Parse(value.data, value.size, &kind, 10))
            return false;
        asset.kind = kind;
        return true;
    }
    if (name == "missing")
        return jsKitSetFlag(value, reset, &asset.missing);
    if (name == "empty")
        return jsKitSetFlag(value, reset, &asset.empty);
    if (name == "disabled")
        return jsKitSetFlag(value, reset, &asset.disabled);
    return false;
}

PrLayoutNode assetDeclare(const JsKitRound& round, JsKitNodeIndex node, PrLayoutNode parent) {
    const AssetData& asset = jsKitData<AssetData>(round, node);
    PrKitAssetField content{
        .key = jsKitNodeKey(round, node),
        .name = jsKitNodeText(round.tree, node),
        .icon = asset.icon,
        .tint = asset.tint,
        .kind = asset.kind,
        .missing = asset.missing,
        .empty = asset.empty,
        .disabled = asset.disabled,
    };
    PrLayoutNode made = prKitAssetField(round.kit, parent, content);
    if (prKitBrowsed(round.kit, made))
        jsKitEventPush(round, node, AssetEvent_Browse, nullptr);
    if (prKitLocated(round.kit, made))
        jsKitEventPush(round, node, AssetEvent_Locate, nullptr);
    if (prKitCleared(round.kit, made))
        jsKitEventPush(round, node, AssetEvent_Clear, nullptr);
    uint64_t dropped = 0;
    if (prKitDropped(round.kit, made, &dropped)) {
        char digits[kPrU64TextMaxBytes];
        size_t size = prU64Format(dropped, digits, 10);
        JsKitDetail detail{.values = {double(dropped)}, .valueCount = 1, .hasText = true, .text = {digits, size}};
        jsKitEventPush(round, node, AssetEvent_Drop, &detail);
    }
    return made;
}

constexpr JsKitKind kKinds[] = {
    {
        .name = "number-field",
        .dataSize = sizeof(NumberData),
        .events = kValueEvents,
        .eventCount = 1,
        .destroy = numberDestroy,
        .set = numberSet,
        .declare = numberDeclare,
    },
    {
        .name = "vector-field",
        .dataSize = sizeof(VectorData),
        .events = kValueEvents,
        .eventCount = 1,
        .set = vectorSet,
        .declare = vectorDeclare,
    },
    {
        .name = "text-field",
        .dataSize = sizeof(FieldData),
        .events = kFieldEvents,
        .eventCount = 2,
        .destroy = fieldDestroy,
        .set = fieldSet,
        .declare = fieldDeclare,
    },
    {
        .name = "text-area",
        .dataSize = sizeof(AreaData),
        .events = kValueEvents,
        .eventCount = 1,
        .destroy = areaDestroy,
        .set = areaSet,
        .declare = areaDeclare,
    },
    {
        .name = "text-area-style",
        .dataSize = sizeof(PrKitTextAreaStyle),
        .set = areaStyleSet,
        .declare = areaStyleDeclare,
    },
    {
        .name = "suggest-field",
        .dataSize = sizeof(FieldData),
        .events = kFieldEvents,
        .eventCount = 2,
        .destroy = fieldDestroy,
        .set = suggestSet,
        .declare = suggestDeclare,
    },
    {
        .name = "slider",
        .dataSize = sizeof(SliderData),
        .events = kValueEvents,
        .eventCount = 1,
        .set = sliderSet,
        .declare = sliderDeclare,
    },
    {
        .name = "inspector-group",
        .dataSize = sizeof(GroupData),
        .events = kValueEvents,
        .eventCount = 1,
        .destroy = groupDestroy,
        .set = groupSet,
        .declare = groupDeclare,
    },
    {.name = "inspector-row", .dataSize = sizeof(PrKitInspectorRowStyle), .set = rowSet, .declare = rowDeclare},
    {
        .name = "asset-field",
        .dataSize = sizeof(AssetData),
        .events = kAssetEvents,
        .eventCount = 4,
        .set = assetSet,
        .declare = assetDeclare,
    },
};

} // namespace

Span<const JsKitKind> jsKitInputKinds() {
    return kKinds;
}

} // namespace pr_impl
