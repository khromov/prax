<!-- suites/prax/workloads/pricing-table.html as a component: its three plans are one block over the plans' data. -->
<script>
    const plans = [
        {
            name: 'Starter',
            audience: 'One developer, side projects, and evaluation.',
            price: '€0',
            terms: 'free while your team is one person',
            features: ['3 concurrent builds', '2 GB cache', '7 day log retention'],
            missing: ['Shared runners only', 'Email support'],
            action: 'Start free',
        },
        {
            name: 'Team',
            badge: 'Most chosen',
            audience: 'Small teams with a release cadence to keep.',
            price: '€29',
            unit: 'per seat',
            terms: 'billed yearly, €36 monthly',
            features: [
                '20 concurrent builds', '50 GB cache', '90 day log retention', 'Dedicated runners',
                'Email support, one business day', 'SSO with Google and Okta',
            ],
            missing: [],
            action: 'Start 14 day trial',
        },
        {
            name: 'Scale',
            audience: 'Organisations with audit and residency rules.',
            price: '€89',
            unit: 'per seat',
            terms: 'billed yearly, minimum 25 seats',
            features: [
                'Unlimited concurrent builds', '500 GB cache', '2 year log retention', 'Isolated runners, EU or US',
                'Support with a 2 hour target', 'Audit log export and SCIM',
            ],
            missing: [],
            action: 'Talk to sales',
        },
    ];
</script>

<div class="intro">
  <h2>Plans for teams that ship</h2>
  <p>Every plan includes the build cache, unlimited public projects, and the
  command line tools. Change or cancel at any time.</p>
  <div class="switch"><span class="on">Yearly, save 20%</span><span>Monthly</span></div>
</div>

<div class="plans">
  {#each plans as plan (plan.name)}
    <div class={['plan', plan.badge && 'mid']}>
      {#if plan.badge}
        <div class="badge">{plan.badge}</div>
      {/if}
      <h3>{plan.name}</h3>
      <div class="for">{plan.audience}</div>
      <div class="price">{#if plan.unit}{plan.price} <small>{plan.unit}</small>{:else}{plan.price}{/if}</div>
      <div class="per">{plan.terms}</div>
      <ul>
        {#each plan.features as feature (feature)}
          <li>{feature}</li>
        {/each}
        {#each plan.missing as feature (feature)}
          <li class="no">{feature}</li>
        {/each}
      </ul>
      <div class="grow"></div>
      <span class="btn">{plan.action}</span>
    </div>
  {/each}
</div>

<div class="foot">
  <div><b>Seats are people, not machines.</b> Runners and build minutes are never
  counted against a seat.</div>
  <div><b>Yearly plans are refundable.</b> Cancel in the first 30 days and we
  return the whole amount.</div>
</div>

<style>
.intro { text-align:center; margin-bottom:12px }
.intro h2 { font-size:20px; margin:0 0 3px; letter-spacing:-.01em }
.intro p { font-size:12px; color:#5d6673; max-width:420px; margin:0 auto }
.switch { display:inline-flex; margin-top:8px; border:1px solid #ccd3da; border-radius:99px; background:#fff; font-size:10px; overflow:hidden }
.switch span { padding:4px 12px; color:#5d6673 }
.switch span.on { background:#22303f; color:#fff; font-weight:bold }
.plans { display:flex; align-items:stretch; gap:10px; margin-bottom:12px }
.plan { flex:1; background:#fff; border:1px solid #d3d8de; border-radius:10px; padding:12px 13px; display:flex; flex-direction:column; color:#22282f }
.plan.mid { border:2px solid #3d82c4; box-shadow:0 3px 10px rgba(61,130,196,.18); margin-top:-8px; padding-top:16px; padding-bottom:16px }
.badge { position:relative; top:-24px; align-self:center; background:#3d82c4; color:#fff; font-size:9px; font-weight:bold; letter-spacing:.06em; text-transform:uppercase; border-radius:99px; padding:2px 10px; margin-bottom:-16px }
.plan h3 { font-size:13px; margin:0 }
.plan .for { font-size:10px; color:#8a93a0; margin-bottom:7px; min-height:26px }
.price { font-size:26px; font-weight:bold; letter-spacing:-.02em; line-height:1 }
.price small { font-size:11px; font-weight:normal; color:#8a93a0 }
.per { font-size:10px; color:#8a93a0; margin-bottom:9px }
ul { list-style:none; font-size:11px; line-height:1.5; margin-bottom:10px }
li { padding-left:16px; position:relative; margin-bottom:4px; color:#3f4954 }
li::before { content:"\2713"; position:absolute; left:0; color:#3a9c62; font-weight:bold }
li.no { color:#a9b1bb }
li.no::before { content:"\2013"; color:#c3c9d1 }
.grow { flex:1 }
.btn { display:block; text-align:center; font-size:11px; font-weight:bold; border-radius:6px; padding:6px 0; border:1px solid #ccd3da; color:#3f4954; background:#fff }
.plan.mid .btn { background:#3d82c4; border-color:#2f6ba6; color:#fff }
.foot { background:#fff; border:1px solid #d3d8de; border-radius:10px; padding:9px 14px; font-size:11px; color:#5d6673; display:flex; gap:18px }
.foot div { flex:1 }
.foot b { display:block; color:#22282f; font-size:11px; margin-bottom:1px }
</style>
