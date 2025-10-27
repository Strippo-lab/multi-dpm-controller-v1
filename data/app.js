// Shared helpers used by all pages

export async function getJSON(url) {
  const r = await fetch(url, {cache:"no-store"});
  if (!r.ok) throw new Error(`${url} → ${r.status}`);
  return r.json();
}
export async function postJSON(url, body) {
  const r = await fetch(url, {method:"POST", headers:{"Content-Type":"application/json"}, body:JSON.stringify(body)});
  if (!r.ok) throw new Error(`${url} → ${r.status}`);
  return r.json().catch(()=> ({}));
}
export function $(sel, root=document){ return root.querySelector(sel); }
export function $all(sel, root=document){ return [...root.querySelectorAll(sel)]; }

export function setActiveNav(id){
  $all(".nav a").forEach(a => a.classList.toggle("active", a.id===id));
}

export function renderDPMTable(container, dpms) {
  if (!dpms || Object.keys(dpms).length===0) { container.innerHTML = `<div class="small">No DPM data.</div>`; return; }
  let rows = '';
  Object.keys(dpms).sort().forEach(key=>{
    const a = dpms[key]; // [state, dpm_state, volt_act, cur_act, temp_act, remain, volt_set, cur_set, idle, last_ms, runtime]
    rows += `
      <tr class="card">
        <td><b>${key}</b></td>
        <td>${a[2]} mV</td>
        <td>${a[3]} mA</td>
        <td>${a[4]} °C</td>
        <td>${a[5]} s</td>
        <td><span class="badge">${a[0]}</span></td>
        <td>${a[6]}/${a[7]} (idle ${a[8]})</td>
      </tr>`;
  });
  container.innerHTML = `
    <table class="table">
      <thead><tr><th>DPM</th><th>Volt</th><th>Cur</th><th>Temp</th><th>Remain</th><th>State</th><th>Set (V/I)</th></tr></thead>
      <tbody>${rows}</tbody>
    </table>`;
}
