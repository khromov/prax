// The Studio Kit gallery (examples/studio_gallery) as a Svelte application for the kit backend. The host's argument
// `section` names one section to show alone, as `--native gallery:<section>` shows the example's.

import { mount } from 'prax-svelte';
import Section from './Section.svelte';
import { sections } from './sections/index.js';

const name = globalThis.hostArguments?.section;
if (!(name in sections))
    throw new Error('the gallery has no section named ' + name + ': give --arg section=<name>');
mount(Section, { props: { name } });
