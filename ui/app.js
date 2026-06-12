const api = (p, o) => fetch(p, o).then(r => r.json());
const el = document.getElementById("app");
let state = { artifact: null, run: null };

function nav(view) {
  document.querySelectorAll("nav a").forEach(a => a.classList.toggle("active", a.dataset.view === view));
  views[view]();
}
document.querySelectorAll("nav a").forEach(a => a.onclick = () => nav(a.dataset.view));

function vtag(v) { return `<span class="tag ${v}">${v}</span>`; }

const PRI = ["SAFE","WATCH","REVIEW","CRITICAL","HALT"];
function priBadge(p, label) { return `<span class="pri p${p}">P${p} ${label||PRI[p]||""}</span>`; }
function csBadge(cs) { return `<span class="pill">CS ${(+cs).toFixed(3)}</span>`; }
function msBar(lo, hi, ks = 0) {
  const safe = lo > ks;
  const clamp = x => Math.max(0, Math.min(100, (x + 1) * 50));
  const a = clamp(lo), b = clamp(hi);
  return `<div class="ms-bar"><div class="ms-range ${safe?'safe':'unsafe'}" style="left:${Math.min(a,b)}%;width:${Math.max(2,Math.abs(b-a))}%"></div><div class="ms-zero"></div></div>`;
}
function diffBadge(d) {
  if (d < -0.05) return '<span class="badge improve">improved</span>';
  if (d > 0.05) return '<span class="badge regress">regression</span>';
  return '<span class="badge">unchanged</span>';
}
function statusLine(light, status, message) {
  return `<div class="status ${light}"><span class="dot"></span><span>${status||light}</span><span class="muted">${message||""}</span></div>`;
}

const views = {};

views.upload = async () => {
  el.innerHTML = `
    <div class="card">
      <h2>Upload engineering artifact</h2>
      <p class="muted">xlsx, xlsm, xlsb, or image. The platform lifts it to a formal model and verifies the margin of safety.</p>
      <div class="row">
        <input type="file" id="file">
        <button id="up">Upload &amp; analyze</button>
      </div>
      <div id="upres"></div>
    </div>
    <div class="card"><h2>Artifacts</h2><div id="list">loading…</div></div>`;
  document.getElementById("up").onclick = doUpload;
  loadList();
};

async function loadList() {
  const a = await api("/api/artifacts");
  const box = document.getElementById("list");
  if (!a.length) { box.innerHTML = '<p class="muted">none yet</p>'; return; }
  box.innerHTML = `<table><tr><th>id</th><th>name</th><th></th></tr>` +
    a.map(x => `<tr><td class="mono">${x.id}</td><td>${x.name}</td>
      <td><button class="ghost" onclick="pick('${x.id}')">open</button></td></tr>`).join("") + `</table>`;
}

window.pick = (id) => { state.artifact = id; nav("dashboard"); };

async function doUpload() {
  const f = document.getElementById("file").files[0];
  if (!f) return;
  const buf = await f.arrayBuffer();
  const r = await fetch("/api/upload", { method: "POST", headers: { "X-Filename": f.name }, body: buf }).then(r => r.json());
  state.artifact = r.artifact_id;
  const an = await api(`/api/artifacts/${r.artifact_id}/analyze`, { method: "POST" });
  document.getElementById("upres").innerHTML =
    `<p>Uploaded <span class="mono">${r.artifact_id}</span> — analysis: ${an.ok ? "ok" : "failed"}.</p>
     <button onclick="nav('dashboard')">Verify now →</button>`;
  loadList();
}

views.dashboard = async () => {
  if (!state.artifact) { el.innerHTML = `<div class="card"><p class="muted">Upload an artifact first.</p></div>`; return; }
  el.innerHTML = `<div class="card"><h2>Verify margin of safety</h2>
    <div class="row">
      <span class="mono">${state.artifact}</span>
      <label class="muted">rel. deviation</label>
      <input type="number" id="rel" value="0.05" step="0.01" style="width:90px">
      <button id="go">Run verification</button>
    </div></div>
    <div id="res"></div>`;
  document.getElementById("go").onclick = runVerify;
};

async function runVerify() {
  const rel = document.getElementById("rel").value;
  document.getElementById("res").innerHTML = `<div class="card">verifying…</div>`;
  const r = await api(`/api/artifacts/${state.artifact}/verify?rel=${rel}`, { method: "POST" });
  state.run = r.run_id;
  if (!r.ok) { document.getElementById("res").innerHTML = `<div class="card">error: ${r.error || "?"}</div>`; return; }
  document.getElementById("res").innerHTML = `<div class="card"><h2>Verdicts</h2>
    <table><tr><th>cell</th><th>label</th><th>verdict</th><th>priority</th><th>MS enclosure</th><th></th><th>method</th></tr>` +
    r.targets.map(t => `<tr><td class="mono">${t.cell}</td><td>${t.label}</td><td>${vtag(t.verdict)}</td>
      <td>${priBadge(t.priority, t.priority_label)} ${csBadge(t.cs)}</td>
      <td class="mono">[${(+t.ms_enclosure[0]).toPrecision(4)}, ${(+t.ms_enclosure[1]).toPrecision(4)}]</td>
      <td style="width:120px">${msBar(+t.ms_enclosure[0], +t.ms_enclosure[1])}</td>
      <td class="pill">${t.method}</td></tr>` +
      (t.abstain_report ? `<tr><td></td><td colspan="6"><div class="abstain"><b>${t.abstain_report.summary}</b>` +
        (t.abstain_report.reasons||[]).map(x => `<div class="reason">• ${x.kind} (${(x.width_share*100).toFixed(0)}%) — ${x.instruction}</div>`).join("") +
        `</div></td></tr>` : "")).join("") +
    `</table><p class="pill">run ${r.run_id}</p></div>`;
}

views.graph = async () => {
  if (!state.artifact) { el.innerHTML = `<div class="card"><p class="muted">Upload an artifact first.</p></div>`; return; }
  el.innerHTML = `<div class="card"><h2>Semantic dependency graph</h2><div id="g">loading…</div></div>`;
  const g = await api(`/api/artifacts/${state.artifact}/graph`);
  if (!g.ok) { document.getElementById("g").innerHTML = "error"; return; }
  drawGraph(g);
};

function drawGraph(g) {
  const W = 940, H = 440, n = g.nodes.length;
  const pos = {};
  const ins = g.nodes.filter(x => x.input), outs = g.nodes.filter(x => !x.input);
  const place = (arr, x) => arr.forEach((nd, i) => pos[nd.id] = { x, y: 40 + (H - 80) * (arr.length === 1 ? 0.5 : i / (arr.length - 1)) });
  place(ins, 120); place(outs, W - 120);
  let svg = `<svg viewBox="0 0 ${W} ${H}">`;
  g.edges.forEach(e => {
    const a = pos[e.from], b = pos[e.to];
    if (a && b) svg += `<line class="edge" x1="${a.x}" y1="${a.y}" x2="${b.x}" y2="${b.y}" stroke-width="${e.origin==='both'?2:1}" />`;
  });
  g.nodes.forEach(nd => {
    const p = pos[nd.id]; if (!p) return;
    svg += `<circle class="${nd.input?'node-in':'node-out'}" cx="${p.x}" cy="${p.y}" r="9" stroke-width="2"/>`;
    svg += `<text x="${p.x + (nd.input?-14:14)}" y="${p.y+4}" fill="#cdd3e0" font-size="11" text-anchor="${nd.input?'end':'start'}">${nd.label||nd.id}</text>`;
  });
  svg += `</svg><p class="pill">${n} nodes · ${g.edges.length} edges · blue=input, magenta=derived, thick=syntactic+interventional</p>`;
  document.getElementById("g").innerHTML = svg;
}

views.cert = async () => {
  if (!state.run) { el.innerHTML = `<div class="card"><p class="muted">Run a verification first.</p></div>`; return; }
  el.innerHTML = `<div class="card"><h2>Certificate &amp; verdicts</h2><div id="c">loading…</div></div>`;
  const r = await api(`/api/runs/${state.run}`);
  let html = `<p class="pill">run ${r.id} · artifact ${r.artifact} · audit chain ${r.audit_chain_ok ? "intact" : "broken"}</p>`;
  html += r.targets.map(t => `<div class="card"><div class="row">
      <b>${t.label}</b> ${vtag(t.verdict)} <span class="pill">${t.method}</span></div>
      <table>
        <tr><th>cell</th><td class="mono">${t.cell}</td></tr>
        <tr><th>MS enclosure</th><td class="mono">[${t.ms_enclosure[0]}, ${t.ms_enclosure[1]}]</td></tr>
        <tr><th>holes</th><td>${t.holes}</td></tr>
        <tr><th>certificate</th><td class="mono">${t.cert}</td></tr>
        <tr><th>validated</th><td>${t.cert_valid ? "yes" : "no"}</td></tr>
      </table></div>`).join("");
  document.getElementById("c").innerHTML = html;
};

views.audit = async () => {
  el.innerHTML = `<div class="card"><h2>Audit &amp; provenance</h2><div id="a">loading…</div></div>`;
  const ok = await api("/api/audit/verify");
  const h = await api("/api/audit");
  let html = `<p>chain integrity: ${ok.audit_ok ? vtag("SAFE").replace("SAFE","intact") : "broken"}</p>`;
  html += `<table><tr><th>seq</th><th>type</th><th>subject</th><th>prev</th></tr>` +
    h.slice(-30).map(e => `<tr><td>${e.seq}</td><td>${e.type}</td><td class="mono">${e.subject||""}</td>
      <td class="mono">${(e.prev||"").slice(0,12)}</td></tr>`).join("") + `</table>`;
  document.getElementById("a").innerHTML = html;
};

views.production = async () => {
  el.innerHTML = `<div class="card"><h2>Production dashboard</h2><div id="dash">loading…</div></div>
    <div class="card"><h2>Report a production event</h2>
      <div class="row">
        <input id="ev_section" placeholder="section e.g. fuselage/section_655" style="width:260px">
        <input id="ev_defect" placeholder="defect e.g. stringer overstress" style="width:240px">
        <button id="ev_go">Route event</button>
      </div><div id="ev_res"></div></div>`;
  const d = await api("/api/dashboard/production");
  document.getElementById("dash").innerHTML =
    `<div class="row">
      <span class="pri p4">HALT ${d.halt}</span>
      <span class="pri p3">CRIT ${d.critical}</span>
      <span class="pri p2">REVIEW ${d.review}</span>
      <span class="pri p1">WATCH ${d.watch}</span>
      <span class="pri p0">SAFE ${d.safe}</span>
      <span class="pill">open work orders: ${d.open_work_orders}</span>
    </div>`;
  document.getElementById("ev_go").onclick = async () => {
    const section = document.getElementById("ev_section").value;
    const defect = document.getElementById("ev_defect").value;
    document.getElementById("ev_res").innerHTML = '<div class="card muted">Проверяем...</div>';
    const r = await api("/api/production/report", { method: "POST", headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ part_id: section || "manual", section, issue_type: defect }) });
    document.getElementById("ev_res").innerHTML = `<div class="card">
      ${statusLine(r.light, r.status, r.message)}
      ${r.eta_minutes ? `<p class="muted">Инженер прибудет в течение ~${r.eta_minutes} мин.</p>` : ""}
      ${r.work_order_id ? `<p class="pill">work order: ${r.work_order_id}</p>` : ""}
      <p class="pill">event: ${r.event_id||""}</p></div>`;
    views.production();
  };
};

views.workorders = async () => {
  el.innerHTML = `<div class="card"><h2>Work orders</h2><div id="wo">loading…</div></div>`;
  const d = await api("/api/work_orders");
  const box = document.getElementById("wo");
  if (!d.work_orders.length) { box.innerHTML = '<p class="muted">queue empty</p>'; return; }
  box.innerHTML = d.work_orders.map(w => `<div class="card">
    <div class="row"><b>${w.id}</b> ${priBadge(w.priority)} <span class="pill">${w.status}</span> <span class="mono">${w.section||""}</span></div>
    <p class="muted">Why: ${w.abstain_why||""}</p></div>`).join("");
};

views.history = async () => {
  if (!state.artifact) { el.innerHTML = `<div class="card"><p class="muted">Open a file first.</p></div>`; return; }
  el.innerHTML = `<div class="card"><h2>Version history</h2><div id="h">loading…</div></div><div id="diffbox"></div>`;
  const h = await api(`/api/artifacts/${state.artifact}/history`);
  const runs = h.runs || [];
  document.getElementById("h").innerHTML = runs.length
    ? `<table><tr><th>run</th><th>verdict</th><th>max CS</th><th>targets</th></tr>` +
      runs.map(r => `<tr><td class="mono">${r.run_id}</td><td>${vtag(r.verdict)}</td><td>${csBadge(r.max_cs)}</td><td>${r.n_targets}</td></tr>`).join("") +
      `</table>` + (runs.length >= 2
        ? `<div class="row"><button id="diffbtn">Diff oldest → newest</button></div>` : "")
    : '<p class="muted">no runs yet — run a verification</p>';
  if (runs.length >= 2) document.getElementById("diffbtn").onclick = async () => {
    const a = runs[0].run_id, b = runs[runs.length - 1].run_id;
    const d = await api(`/api/diff?run_a=${a}&run_b=${b}`);
    document.getElementById("diffbox").innerHTML = `<div class="card"><h2>Diff ${d.safe ? "" : "⚠ regression"}</h2>
      <p>${d.summary}</p>
      <table><tr><th>cell</th><th>transition</th><th>ΔCS</th><th></th></tr>` +
      d.targets.map(t => `<tr><td class="mono">${t.cell}</td><td>${t.transition}</td>
        <td class="mono">${(t.delta_cs>=0?'+':'')+t.delta_cs.toFixed(3)}</td><td>${diffBadge(t.delta_cs)}</td></tr>`).join("") +
      `</table></div>`;
  };
};

views.admissible = async () => {
  if (!state.artifact) { el.innerHTML = `<div class="card"><p class="muted">Open a file first.</p></div>`; return; }
  el.innerHTML = `<div class="card"><h2>Admissible region (SIVIA)</h2>
    <div class="row"><label class="muted">rel</label><input id="ar_rel" type="number" value="0.3" step="0.1" style="width:80px">
    <label class="muted">ε</label><input id="ar_eps" type="number" value="0.02" step="0.01" style="width:80px">
    <button id="ar_go">Compute</button></div><div id="ar_res"></div></div>`;
  document.getElementById("ar_go").onclick = async () => {
    const rel = document.getElementById("ar_rel").value, eps = document.getElementById("ar_eps").value;
    document.getElementById("ar_res").innerHTML = "computing set inversion…";
    const r = await api(`/api/artifacts/${state.artifact}/admissible_region?rel=${rel}&eps=${eps}`, { method: "POST" });
    document.getElementById("ar_res").innerHTML = r.ok
      ? `<div class="card"><p><b>target ${r.target}</b> · ${r.dims} dims · ${r.iterations} iterations</p>
         <p>provably safe fraction of the parameter box: <b>${(r.safe_fraction*100).toFixed(1)}%</b>
         (${r.n_safe} safe boxes, ${r.n_unknown} unknown)</p>
         <div class="ms-bar"><div class="ms-range safe" style="left:0;width:${(r.safe_fraction*100).toFixed(1)}%"></div></div></div>`
      : `<div class="card">error: ${r.error||"?"}</div>`;
  };
};

views.sections = async () => {
  el.innerHTML = `<div class="card"><h2>Sections</h2><div class="row"><button id="rebuild" class="ghost">Rebuild map</button></div><div id="sx">loading…</div></div>`;
  document.getElementById("rebuild").onclick = async () => { await api("/api/sections/map", { method: "POST" }); views.sections(); };
  const d = await api("/api/sections/status");
  const rows = d.sections || [];
  document.getElementById("sx").innerHTML = rows.length
    ? `<table><tr><th>section</th><th>files</th><th>domain</th><th>severity</th></tr>` +
      rows.map(s => { const p = s.max_priority; const lbl = p < 0 ? "—" : ["SAFE","WATCH","REVIEW","CRITICAL","HALT"][p];
        return `<tr><td class="mono">${s.section}</td><td>${s.files}</td><td>${s.domain||""}</td>
        <td>${p < 0 ? '<span class="muted">no events</span>' : priBadge(p, lbl)}</td></tr>`; }).join("") +
      `</table>`
    : '<p class="muted">no indexed files — crawl a tree first</p>';
};

views.coverage = async () => {
  el.innerHTML = `<div class="card"><h2>Pipeline coverage</h2>
    <p class="muted">Static scan of indexed files: which constructs the verifier does not yet cover, by frequency.</p>
    <div id="hm">loading…</div></div>
    <div class="card"><h2>Sample uncovered cells</h2>
      <div class="row"><input id="cov_reason" placeholder="reason e.g. unsupported_function:VLOOKUP" style="width:340px"><button id="cov_go">Sample</button></div>
      <div id="cov_res"></div></div>`;
  const h = await api("/api/coverage/heatmap");
  const c = h.constructs || [];
  document.getElementById("hm").innerHTML = c.length
    ? `<table><tr><th>construct</th><th>files</th><th>cells</th></tr>` +
      c.map(x => `<tr><td class="mono">${x.construct}</td><td>${x.file_count}</td><td>${x.cell_count}</td></tr>`).join("") + `</table>`
    : '<p class="muted">no uncovered constructs recorded — analyze files first</p>';
  document.getElementById("cov_go").onclick = async () => {
    const reason = document.getElementById("cov_reason").value;
    const s = await api(`/api/coverage/sample?reason=${encodeURIComponent(reason)}&n=10`);
    const rows = s.samples || [];
    document.getElementById("cov_res").innerHTML = rows.length
      ? `<table><tr><th>file</th><th>cell</th><th>formula</th></tr>` +
        rows.map(x => `<tr><td class="mono">${x.file_id}</td><td class="mono">${x.cell_ref}</td><td class="mono">${x.formula}</td></tr>`).join("") + `</table>`
      : '<p class="muted">no samples for that reason</p>';
  };
};

nav("production");
