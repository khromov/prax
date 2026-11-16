<!-- The gallery's sectionDialogs (examples/studio_gallery/section_feedback.cpp).

     Each overlay is under the button that opens it and is `triggered`, so that the button's activation opens it in
     the round of the activation, as the example's call does. Its `toggle` tells the state here of that, and of Escape,
     of a press outside it and of the dialog's close button. -->
<script>
    import Card from '../parts/Card.svelte';
    import DemoPanel from '../parts/DemoPanel.svelte';
    import SectionFrame from '../parts/SectionFrame.svelte';
    import { space, text } from '../tokens.js';
    import SpecimenRow from './feedback/SpecimenRow.svelte';

    // Whether each overlay is open, which the kit keeps on its trigger in the example.
    let confirmOpen = $state(false);
    let sheetOpen = $state(false);
    let modalOpen = $state(false);

    // The confirm dialog's option (gallery_state.h DialogState).
    let deleteOrphans = $state(false);

    const names = ['Player . 0x7FF8_A2B0', 'Enemy_04 . 0x7FF8_B001', 'LightProbe_Grid . 0x7FF8_B112'];

    const description =
        'A dialog is a modal carrying a titled card: the scrim dims the page and takes the presses that miss ' +
        'it, the close control and Escape take it down, and the footer holds the buttons that answer it.';
</script>

<SectionFrame number="17" title="Dialogs" lede="A blocking portal for what cannot be undone." {description}>
  <DemoPanel title="CONFIRM" tags="CENTER">
    <SpecimenRow gap={space[3]}>
      <button variant="danger">
        Delete 3 Entities...
        <dialog triggered open={confirmOpen} ontoggle={(event) => (confirmOpen = event.value === 1)}>
          Delete the selected entities?
          <label {...text.bodySecondary}>
            Three entities and their children go with this. References from other scenes are left broken.
          </label>
          <panel background="header" gap={space[1]} padding="{space[2]} {space[4]}">
            {#each names as name (name)}
              <label {...text.monoSecondary}>{name}</label>
            {/each}
          </panel>
          <checkbox value={deleteOrphans} onchange={(event) => (deleteOrphans = event.value === 1)}>
            Also delete orphaned assets
          </checkbox>
          <dialog-footer>
            <button onclick={() => (confirmOpen = false)}>Cancel</button>
            <button variant="danger" onclick={() => (confirmOpen = false)}>Delete 3 Entities</button>
          </dialog-footer>
        </dialog>
      </button>
      <label {...text.bodySecondary}>the card carries a header, a body and a footer</label>
    </SpecimenRow>
  </DemoPanel>
  <DemoPanel title="TOP SHEET" tags="TOP">
    <SpecimenRow gap={space[3]}>
      <button>
        Open Top Sheet...
        <dialog placement="top" triggered open={sheetOpen} ontoggle={(event) => (sheetOpen = event.value === 1)}>
          Go to File
          <label {...text.bodySecondary}>
            A top-placed card reads as a launcher rather than a question, which is the shape the command palette
            takes.
          </label>
          <dialog-footer><button onclick={() => (sheetOpen = false)}>Close</button></dialog-footer>
        </dialog>
      </button>
      <label {...text.bodySecondary}>a topped dialog drops from the top edge, as a launcher does</label>
    </SpecimenRow>
  </DemoPanel>
  <!-- A modal places and dims, and its content is the caller's own. -->
  <DemoPanel title="PLAIN MODAL" tags="SCRIM . CONTENT">
    <SpecimenRow gap={space[3]}>
      <button>
        Open Modal...
        <modal triggered open={modalOpen} ontoggle={(event) => (modalOpen = event.value === 1)}>
          <Card gap={space[4]} padding={space[8]} width={360}>
            <box direction="row" gap={space[3]}>
              <icon icon="info" size={16} tone="accent"></icon>
              <label {...text.strong}>A frame of the caller's own</label>
            </box>
            <label {...text.bodySecondary}>The modal places and dims; everything inside it is declared here.</label>
            <box direction="row" justify="end" gap={space[3]}>
              <button onclick={() => (modalOpen = false)}>Close</button>
            </box>
          </Card>
        </modal>
      </button>
      <label {...text.bodySecondary}>no header and no footer: the content is the caller's own</label>
    </SpecimenRow>
  </DemoPanel>
</SectionFrame>
