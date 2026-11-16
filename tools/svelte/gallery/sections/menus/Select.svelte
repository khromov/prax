<!-- A select of labeled options (section_controls select). The option at index `selected` is the chosen one. Choosing
     an option, or stepping to it with Up and Down, writes its index to `selected`, which a caller binds. It takes a
     select's attributes. -->
<script>
    let { labels, selected = $bindable(), ...field } = $props();

    function step(event) {
        if (event.value < 0 && selected > 0)
            --selected;
        else if (event.value > 0 && selected + 1 < labels.length)
            ++selected;
    }
</script>

<select {...field} onstep={step}>{labels[selected] ?? ''}<select-menu>
    {#each labels as label, index}
      <menu-item checked={selected === index} onclick={() => (selected = index)}>{label}</menu-item>
    {/each}
  </select-menu>
</select>
