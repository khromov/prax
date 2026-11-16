<!-- The gallery's sectionInputs (examples/studio_gallery/section_controls.cpp). -->
<script>
    import { sameFrame } from 'prax-svelte';
    import CaptionRow from '../parts/CaptionRow.svelte';
    import DemoPanel from '../parts/DemoPanel.svelte';
    import FillLabel from '../parts/FillLabel.svelte';
    import Grid from '../parts/Grid.svelte';
    import SectionFrame from '../parts/SectionFrame.svelte';
    import {
        areas, inputs, redoAvailable, revisions, submitCommand, undoAvailable, undoBegin, undoCommit, undoStep,
    } from '../state/inputs.svelte.js';
    import { space, text } from '../tokens.js';

    // The width of a field's caption, in px.
    const captionWidth = 92;

    // The styles of the styled text area, by the style that a run names, and the names of their buttons.
    const styledLooks = [
        { role: 'body', tone: 'auto' },
        { role: 'strong', tone: 'auto' },
        { role: 'body-italic', tone: 'auto' },
        { role: 'body', tone: 'accent' },
        { role: 'heading', tone: 'auto' },
    ];
    const styledNames = ['Plain', 'Strong', 'Italic', 'Accent', 'Heading'];

    // Runs set chords in their own role within the sentence around them.
    const chord = { role: 'mono-strong', tone: 'accent' };
    const strongRun = { role: 'strong' };
    const plainRun = { tone: 'secondary' };
    const gestures = [
        { text: 'Double-click', ...strongRun },
        { text: ' selects a word, ', ...plainRun },
        { text: 'triple-click', ...strongRun },
        {
            text: ' the line, and a fourth click everything; hold the drag and the selection grows by that unit. ',
            ...plainRun,
        },
        { text: 'Cmd Left', ...chord },
        { text: ' and ', ...plainRun },
        { text: 'Cmd Right', ...chord },
        { text: ' move to the line edges, ', ...plainRun },
        { text: 'Cmd Up', ...chord },
        { text: ' and ', ...plainRun },
        { text: 'Cmd Down', ...chord },
        { text: ' to the ends of the text, each with ', ...plainRun },
        { text: 'Shift', ...chord },
        { text: ' to extend the selection.', ...plainRun },
    ];

    // The typed scalars: each a caption, the type that the kit stores the value in, and the value of the inputs that
    // it edits.
    const typedCells = [
        { caption: 'I8', type: 'i8', value: 'tiny' },
        { caption: 'U8', type: 'u8', value: 'level' },
        { caption: 'I16', type: 'i16', value: 'offset' },
        { caption: 'U16', type: 'u16', value: 'stride' },
        { caption: 'F32', type: 'f32', value: 'scalar' },
        { caption: 'I32', type: 'i32', value: 'subdivisions' },
        { caption: 'U32', type: 'u32', value: 'seed' },
        { caption: 'I64', type: 'i64', value: 'budget' },
        { caption: 'U64', type: 'u64', value: 'cycles' },
        { caption: 'F64', type: 'f64', value: 'density' },
    ];

    const sliderRows = [
        { caption: 'STRENGTH', type: 'f32', value: 'scalar', min: 0, max: 1, scale: 'linear', precision: 2 },
        { caption: 'MASS', type: 'f32', value: 'mass', min: 0, max: 200, scale: 'linear', unit: 'kg', precision: 1 },
        { caption: 'STEPS', type: 'i32', value: 'subdivisions', min: 0, max: 8, scale: 'linear', precision: 0 },
        {
            caption: '20-20K HZ',
            type: 'f32',
            value: 'frequency',
            min: 20,
            max: 20000,
            scale: 'ratio',
            unit: 'Hz',
            precision: 1,
        },
        {
            caption: 'SHUTTER',
            type: 'f32',
            value: 'shutterSeconds',
            min: 1.0 / 8000,
            max: 30,
            scale: 'ratio',
            unit: 's',
            precision: 4,
        },
    ];

    const undo = $derived(inputs.undo);
    const undoReadouts = $derived([
        { label: 'BEGIN', count: undo.begins },
        { label: 'CHANGE', count: undo.changes },
        { label: 'COMMIT', count: undo.commits },
        { label: 'STATES', count: undo.history.length },
        { label: 'AT', count: undo.cursor },
    ]);

    const description =
        'Every field here is live. A numeric field scrubs under a horizontal drag, and steps under Up and Down ' +
        'while it has focus; a slider follows the pointer, and its track maps a ratio if its range spans decades.';

    // Records the bound controls' gestures as states of the undo history. The row that holds the controls hears each
    // gesture's phases: its begin before the change that carries the gesture's first write, and its commit after the
    // change that carries its last.
    function undoEdit(event) {
        switch (event.type) {
            case 'editbegin':
                undoBegin(undo);
                ++undo.begins;
                break;
            case 'editchange':
                undo.wrote = true;
                ++undo.changes;
                break;
            default:
                undoCommit(undo);
                ++undo.commits;
                break;
        }
    }

    // What the toolbar of the styled area has asked of the area, as the values of the area's `style-selection`, `undo`,
    // `redo` and `focus`. The area does what one of them names once, each time its value changes, so each value holds
    // the number of the ask that set it. `count` is the number of the last ask, which is the area's `focus`. `style`
    // is the style that the selection takes, with the number of the ask. `undo` and `redo` are zero before their
    // first ask.
    const styledAsks = $state({ count: 0, style: null, undo: 0, redo: 0 });

    // Asks the styled area to give what its selection holds a style, or to undo or redo a step of its history, which
    // the example's toolbar does through the area's view (prTextEditViewSetStyle, prTextEditHistoryUndo). A button
    // takes focus from the area as it is pressed, and every ask gives it back, with the area's selection as it stood.
    // The area does what it is asked in the round after the click, and has focus in the round after that, so this
    // asks for the frame of the click to show both, as the example's frame does.
    function styledAsk(name, style) {
        const count = ++styledAsks.count;
        styledAsks[name] = name === 'style' ? [style, count] : count;
        sameFrame();
    }

    // Sets a value that several controls show. The example gives them one variable, so each shows another's edit in
    // the frame that made it.
    function share(name, value) {
        inputs[name] = value;
        sameFrame();
    }
</script>

<SectionFrame number="06" title="Inputs" lede="Text, numeric, vector, slider and color." {description}>
  <DemoPanel title="TEXT" tags="EDITABLE . SELECT ON FOCUS . DISABLED">
    <box direction="row" align="start" gap={space[4]}>
      <box gap={space[2]} flex="fill">
        <label {...text.caption}>ENTITY NAME</label>
        <text-field value={inputs.name} placeholder="Unnamed entity" onchange={(event) => (inputs.name = event.text)}>
        </text-field>
      </box>
      <box gap={space[2]} flex="fill">
        <label {...text.caption}>SELECT ON FOCUS</label>
        <text-field value={inputs.search} placeholder="Search query" icon="search" clearable select-on-focus
            onchange={(event) => (inputs.search = event.text)}></text-field>
      </box>
      <box disabled gap={space[2]} flex="fill">
        <label {...text.caption}>DISABLED</label>
        <text-field value={inputs.locked} onchange={(event) => (inputs.locked = event.text)}></text-field>
      </box>
    </box>
  </DemoPanel>
  <DemoPanel title="HISTORY" tags="ENTER SUBMITS . UP AND DOWN RECALL">
    <CaptionRow caption="COMMAND" labelWidth={captionWidth}>
      <!-- The field leaves Enter to its listener, which submits the line. -->
      <text-field value={inputs.command} placeholder="type a line, submit it, then press Up"
          history={inputs.commands.slice(0, 16).join('\n')} mono onchange={(event) => (inputs.command = event.text)}
          onsubmit={submitCommand}></text-field>
      <label {...text.caption}>{inputs.commands.length} submitted</label>
    </CaptionRow>
  </DemoPanel>
  <DemoPanel title="SUGGEST" tags="FREE TEXT . KNOWN VALUES">
    <CaptionRow caption="AREA" labelWidth={captionWidth}>
      <suggest-field value={inputs.area} placeholder="Area path" icon="search" select-on-focus
          suggestions={areas.join('\n')} mono onchange={(event) => (inputs.area = event.text)}></suggest-field>
    </CaptionRow>
    <CaptionRow caption="REVISION" labelWidth={captionWidth}>
      <suggest-field value={inputs.revision} placeholder="Branch, tag or commit" icon="branch" select-on-focus
          suggestions={revisions.join('\n')} mono onchange={(event) => (inputs.revision = event.text)}></suggest-field>
    </CaptionRow>
    <label {...text.bodySecondary}>the caller filters the items, so the list shows what it hands over</label>
  </DemoPanel>
  <DemoPanel title="TEXT AREA" tags="SOFT WRAP . SHADER SNIPPET">
    <text>
      {#each gestures as run}
        <run role={run.role} tone={run.tone}>{run.text}</run>
      {/each}
    </text>
    <box direction="row" align="start" gap={space[4]}>
      <box gap={space[2]} flex="fill">
        <label {...text.caption}>NOTES</label>
        <text-area value={inputs.prose} placeholder="Notes" rows={5} onchange={(event) => (inputs.prose = event.text)}>
        </text-area>
      </box>
      <box gap={space[2]} flex="fill">
        <label {...text.caption}>SHADER SNIPPET</label>
        <text-area value={inputs.snippet} rows={5} mono onchange={(event) => (inputs.snippet = event.text)}></text-area>
      </box>
    </box>
  </DemoPanel>
  <DemoPanel title="STYLED TEXT" tags="STYLE RUNS IN ONE BUFFER">
    <box direction="row" gap={space[2]}>
      {#each styledNames as name, style (name)}
        <button onclick={() => styledAsk('style', style)}>{name}</button>
      {/each}
      <button onclick={() => styledAsk('undo')}>Undo</button>
      <button onclick={() => styledAsk('redo')}>Redo</button>
    </box>
    <!-- The area holds the text and its runs, and tells each as an edit, a style or a step of the history changes
         it. Giving the area what it told changes nothing, so the caret and the selection stay where they are. -->
    <text-area value={inputs.styled.text} runs={inputs.styled.runs} rows={5} style-selection={styledAsks.style}
        undo={styledAsks.undo || null} redo={styledAsks.redo || null} focus={styledAsks.count || null}
        onchange={(event) => (inputs.styled.text = event.text)} onruns={(event) => (inputs.styled.runs = event.text)}>
      {#each styledLooks as look}
        <text-area-style role={look.role} tone={look.tone}></text-area-style>
      {/each}
    </text-area>
  </DemoPanel>
  <DemoPanel title="NUMERIC . VECTORS" tags="DRAG TO SCRUB">
    <CaptionRow caption="UV" labelWidth={captionWidth}>
      <vector-field type="f32" value={inputs.uv} step={0.01} precision={3}
          onchange={(event) => (inputs.uv = event.values)}></vector-field>
    </CaptionRow>
    <CaptionRow caption="POSITION" labelWidth={captionWidth}>
      <vector-field type="f32" value={inputs.position} step={0.05}
          onchange={(event) => (inputs.position = event.values)}></vector-field>
    </CaptionRow>
    <CaptionRow caption="ROTATION" labelWidth={captionWidth}>
      <vector-field type="f32" value={inputs.rotation} step={0.5} precision={1}
          onchange={(event) => (inputs.rotation = event.values)}></vector-field>
    </CaptionRow>
    <CaptionRow caption="ORIENTATION" labelWidth={captionWidth}>
      <vector-field type="f32" value={inputs.quaternion} step={0.01} precision={3}
          onchange={(event) => (inputs.quaternion = event.values)}></vector-field>
    </CaptionRow>
    <CaptionRow caption="SCALAR" labelWidth={captionWidth}>
      <number-field type="f32" value={inputs.scalar} step={0.01} min={0} max={1} width={120} flex="fixed"
          onchange={(event) => share('scalar', event.value)}></number-field>
    </CaptionRow>
    <CaptionRow caption="UNITS" labelWidth={captionWidth}>
      <number-field type="f32" value={inputs.mass} step={0.5} min={0} unit="kg" precision={1} width={120} flex="fixed"
          onchange={(event) => share('mass', event.value)}></number-field>
      <number-field type="f32" value={inputs.shutter} step={0.001} min={0} unit="s" precision={4} width={120}
          flex="fixed" onchange={(event) => (inputs.shutter = event.value)}></number-field>
      <number-field type="u32" value={inputs.seed} step={1} unit="Hz" width={120} flex="fixed"
          onchange={(event) => share('seed', event.value)}></number-field>
    </CaptionRow>
  </DemoPanel>
  <DemoPanel title="TYPED SCALARS" tags="I8-U64 . F32 . F64">
    <Grid items={typedCells} columns={5} gap={space[4]}>
      {#snippet cell(typed)}
        <box gap={space[2]}>
          <label {...text.caption}>{typed.caption}</label>
          <number-field type={typed.type} value={inputs[typed.value]} step={0.5}
              onchange={(event) => share(typed.value, event.value)}></number-field>
        </box>
      {/snippet}
    </Grid>
    <label {...text.bodySecondary}>an integer kind rounds what it writes and never leaves its own range</label>
  </DemoPanel>
  <DemoPanel title="SLIDERS" tags="TRACK . FIELD . RATIO">
    {#each sliderRows as row (row.caption)}
      <CaptionRow caption={row.caption} labelWidth={72}>
        <slider type={row.type} value={inputs[row.value]} min={row.min} max={row.max} scale={row.scale}
            onchange={(event) => share(row.value, event.value)}></slider>
        <number-field type={row.type} value={inputs[row.value]} min={row.min} max={row.max} unit={row.unit}
            precision={row.precision} width={88} flex="fixed" onchange={(event) => share(row.value, event.value)}>
        </number-field>
      </CaptionRow>
    {/each}
  </DemoPanel>
  <DemoPanel title="EDIT LIFECYCLE" tags="SINK . UNDO . REDO">
    <box direction="row" align="end" gap={space[6]} oneditbegin={undoEdit} oneditchange={undoEdit}
        oneditcommit={undoEdit}>
      <box gap={space[2]} flex="fill">
        <label {...text.caption}>STRENGTH</label>
        <slider type="f32" value={undo.values.strength} min={0} max={1}
            onchange={(event) => (undo.values.strength = event.value)}></slider>
      </box>
      <box gap={space[2]} flex="fill">
        <label {...text.caption}>STEPS</label>
        <number-field type="i32" value={undo.values.steps} step={1} min={0} max={64}
            onchange={(event) => (undo.values.steps = event.value)}></number-field>
      </box>
      <box gap={space[2]} flex="fill">
        <label {...text.caption}>ENABLED</label>
        <toggle value={undo.values.enabled} onchange={(event) => (undo.values.enabled = event.value !== 0)}></toggle>
      </box>
    </box>
    <box direction="row" align="center" gap={space[8]}>
      {#each undoReadouts as readout (readout.label)}
        <box gap={space[1]}>
          <label {...text.monoStrong}>{readout.count}</label>
          <label {...text.caption}>{readout.label}</label>
        </box>
      {/each}
      <button icon="undo" disabled={!undoAvailable(undo)} onclick={() => undoStep(undo, false)}>Undo</button>
      <button icon="redo" disabled={!redoAvailable(undo)} onclick={() => undoStep(undo, true)}>Redo</button>
      <FillLabel {...text.bodySecondary}>
        a scrub across many frames is one state, and an edit after an undo forks
      </FillLabel>
    </box>
  </DemoPanel>
  <DemoPanel title="COLOR" tags="FIELD . HEX">
    <box direction="row" align="center" gap={space[3]}>
      <color-field value={inputs.tint} width={180} flex="fixed" onchange={(event) => (inputs.tint = event.values)}>
      </color-field>
      <label {...text.bodySecondary}>the field shows the color and its hex, and raises a picker when activated</label>
    </box>
  </DemoPanel>
</SectionFrame>
