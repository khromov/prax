<!-- suites/prax/workloads/survey.html as a component: the rating grid and the check group are blocks over data, and each
     slider's output is state that its input event sets, where the page's inline handlers wrote to the output. -->
<script>
    const grades = ['Poor', 'Fair', 'Good', 'Great', 'Superb'];

    // The parts that the grid rates, each with the index of its chosen grade and of a grade that cannot be chosen.
    const parts = [
        { label: 'Finding the product', name: 'find', chosen: 3 },
        { label: 'Checkout', name: 'pay', chosen: 1 },
        { label: 'Delivery time', name: 'ship', chosen: 4 },
        { label: 'Packaging', aside: '(skipped)', name: 'box', disabled: 4 },
    ];

    const purchases = [
        { label: 'Shelving', checked: true },
        { label: 'Lighting' },
        { label: 'Storage boxes', checked: true },
        { label: 'Desk accessories' },
        { label: 'Something else', id: 'mixed', indeterminate: true },
        { label: 'Gift card (always)', checked: true, disabled: true },
    ];

    let recommend = $state('8');
    let orders = $state('4');
</script>

<form class="sv" action="survey.html" onsubmit={(event) => event.preventDefault()}>
  <div class="top">
    <h2>How are we doing?</h2>
    <label for="done">3 of 5 answered</label>
    <progress id="done" max="5" value="3"></progress>
  </div>

  <fieldset>
    <legend>Rate each part of your last order</legend>
    <table class="lk">
      <tbody>
        <tr>
          <th></th>
          {#each grades as grade (grade)}
            <th>{grade}</th>
          {/each}
        </tr>
        {#each parts as part (part.name)}
          <tr>
            <td>{#if part.aside}{part.label} <small>{part.aside}</small>{:else}{part.label}{/if}</td>
            {#each grades as grade, index (grade)}
              <td>
                <input type="radio" name={part.name} checked={index === part.chosen}
                    disabled={index === part.disabled} />
              </td>
            {/each}
          </tr>
        {/each}
      </tbody>
    </table>
  </fieldset>

  <fieldset>
    <legend>A few numbers</legend>
    <div class="row"><span>How likely are you to recommend us?</span>
      <input type="range" id="nps" min="0" max="10" value={recommend}
          oninput={(event) => (recommend = event.currentTarget.value)} />
      <output id="npsv" for="nps">{recommend}</output></div>
    <div class="row"><span>Orders a month</span>
      <input type="range" id="freq" min="0" max="20" step="2" value={orders}
          oninput={(event) => (orders = event.currentTarget.value)} />
      <output id="freqv" for="freq">{orders}</output></div>
    <div class="row"><span>Your profile is this complete</span>
      <meter min="0" max="100" low="40" high="80" optimum="100" value="64">64%</meter>
      <output>64%</output></div>
  </fieldset>

  <fieldset>
    <legend>What did you buy, and how should we reach you?</legend>
    <div class="opts">
      {#each purchases as purchase (purchase.label)}
        <label><input type="checkbox" id={purchase.id} checked={purchase.checked === true}
            disabled={purchase.disabled === true} indeterminate={purchase.indeterminate === true} /> {purchase.label}</label>
      {/each}
    </div>
    <div class="row"><span>Follow up by email</span><input type="checkbox" switch="switch" checked="checked" /></div>
    <div class="row"><span>Theme for your receipts</span>
      <span class="pick"><input type="color" value="#3d82c4" /> Brand blue</span></div>
  </fieldset>

  <div class="foot">
    <span class="fine">Answers are anonymous. We read every one.</span>
    <span class="btns"><input type="reset" /><button type="button">Save for later</button><input type="submit" value="Send answers" /></span>
  </div>
</form>

<style>
.sv { background:#fff; border:1px solid #d3d8de; border-radius:10px; color:#22282f; font-size:12px; padding:12px 14px }
.top { display:flex; align-items:center; gap:10px; margin-bottom:10px }
.top h2 { font-size:15px; flex:1 }
.top label { font-size:10px; color:#6d7783 }
.top progress { width:120px; accent-color:#3d82c4 }
fieldset { border:1px solid #e6e9ed; border-radius:8px; padding:8px 10px 10px; margin-bottom:10px }
legend { font-size:9px; text-transform:uppercase; letter-spacing:.09em; color:#8a93a0; padding:0 4px }
table.lk { width:100%; border-collapse:collapse; font-size:11px }
.lk th { font-weight:normal; color:#6d7783; font-size:9px; padding:0 2px 4px; text-align:center }
.lk th:first-child { text-align:start }
.lk td { padding:3px 2px; text-align:center; border-top:1px solid #f0f2f5 }
.lk td:first-child { text-align:start; color:#3f4954 }
.row { display:flex; align-items:center; gap:10px; margin-top:6px; font-size:11px }
.row > span:first-child { flex:1; color:#3f4954 }
.row input[type=range] { width:180px }
.row output { width:28px; text-align:end; font-weight:bold; font-variant-numeric:tabular-nums }
.row meter { width:120px }
.opts { display:flex; flex-wrap:wrap; gap:6px 14px; font-size:11px }
.opts label { display:flex; align-items:center; gap:4px }
.pick { display:flex; align-items:center; gap:6px; font-size:11px; color:#3f4954 }
.foot { display:flex; justify-content:space-between; align-items:center; margin-top:4px }
.foot .fine { font-size:10px; color:#8a93a0 }
.foot .btns { display:flex; gap:6px }
</style>
