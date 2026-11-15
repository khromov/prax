<!-- examples/hello_style as a component: Studio Kit's widgets as elements, their style structs as attributes, and the
     example's count of clicks as state. The block is the example's node of lib/layout's own. -->
<script>
    let clicks = $state(0);
</script>

<box align="start" gap="16" padding="32">
  <label role="display">Hello, world</label>
  <label tone="secondary">Kit text names a role and a tone, and the theme gives the face and the color.</label>

  <box direction="row" align="center" gap="8">
    <button variant="primary" size="lg" onclick={() => (clicks += 1)}>Click me</button>
    <badge variant={clicks > 0 ? 'accent' : 'neutral'}>{clicks} clicks</badge>
  </box>

  <label width="320">This label is placed by its caller, at 320 px wide, so it wraps onto a second line.</label>

  <block border="2" padding="16" radius="12" background="accent-weak" border-color="accent" font="heading"
      color="text1">A node styled with lib/layout's groups</block>
</box>
