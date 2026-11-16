<!-- A square frame for a picture on the viewport's background, which shows where a fit leaves room
     (section_feedback.cpp framed). -->
<script>
    let { children } = $props();

    // The side of the square frames that the pictures are fitted into, in px.
    const frameSize = 148;
</script>

<panel background="viewport" width={frameSize} height={frameSize} flex="fixed" align-self="start">
  {@render children?.()}
</panel>
