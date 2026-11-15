<!-- The section of a name, alone on the gallery's page. -->
<script>
    import SectionPage from './SectionPage.svelte';
    import { sections } from './sections/index.js';

    let { name } = $props();
    const Section = $derived(sections[name]);
</script>

<SectionPage>
  <Section />
</SectionPage>
