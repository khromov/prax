// The Studio Kit gallery (examples/studio_gallery) as a Svelte application for the kit backend. The host's argument
// `section` names one section to show alone, as `--native gallery:<section>` shows the example's. With no argument, it
// is the whole gallery.

import { mount } from 'prax-svelte';
import Gallery from './Gallery.svelte';
import Section from './Section.svelte';
import { sections } from './sections/index.js';

const name = globalThis.hostArguments?.section;
if (name === undefined)
    mount(Gallery);
else if (name in sections)
    mount(Section, { props: { name } });
else
    throw new Error('the gallery has no section named ' + name);
