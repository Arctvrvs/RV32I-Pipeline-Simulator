import createSimModule from './sim.js';

const $ = (id) => document.getElementById(id);

const EXAMPLES = [
  {
    name: 'Array Sum',
    file: 'examples/test.hex',
    desc: 'Write 100 integers from address 0x0100, then sum them. Shows store/load loops and basic arithmetic.',
  },
  {
    name: 'Fibonacci',
    file: 'examples/fibonacci.hex',
    desc: 'Compute fib[0..19] iteratively. Results are stored from 0x0100 while registers update each cycle.',
  },
  {
    name: 'Factorial',
    file: 'examples/factorial.hex',
    desc: 'Compute 10! = 3,628,800 using repeated-addition multiply (no MUL in base RV32I). Result at 0x0100.',
  },
  {
    name: 'Bubble Sort',
    file: 'examples/bubble_sort.hex',
    desc: 'Sort [5,3,8,1,9,2,7,4] at address 0x0100 → [1,2,3,4,5,7,8,9]. Many branches; watch the pipeline flush.',
  },
  {
    name: 'Load-Use Hazards',
    file: 'examples/hazard.hex',
    desc: 'Back-to-back load→use pairs. Each load is immediately consumed by the next instruction, forcing stall cycles. Watch the stall counter rise.',
  },
  {
    name: 'Branch Prediction',
    file: 'examples/branch_pred.hex',
    desc: '500-iteration tight loop. With weak-not-taken initialization, expect two mispredictions: first taken and final not-taken.',
  },
];

const el = {
  boot: $('boot'),
  runBadge: $('runBadge'),
  nowLine: $('nowLine'),
  statsLine: $('statsLine'),
  commitLine: $('commitLine'),
  followInfo: $('followInfo'),
  lensTitle: $('lensTitle'),
  pipeline: $('pipeline'),
  lens: $('lens'),
  disasm: $('disasm'),
  disasmRange: $('disasmRange'),
  regs: $('regs'),
  mem: $('mem'),
  memRange: $('memRange'),
  uart: $('uart'),

  toast: $('toast'),

  btnRun: $('btnRun'),
  btnStep: $('btnStep'),
  btnCommit: $('btnCommit'),
  btnReset: $('btnReset'),
  btnLoadExample: $('btnLoadExample'),
  exampleMenu: $('exampleMenu'),

  btnDisasmFind: $('btnDisasmFind'),
  btnDisasmJump: $('btnDisasmJump'),
  disasmQuery: $('disasmQuery'),

  btnMemJump: $('btnMemJump'),
  memJump: $('memJump'),

  cyclesPerFrame: $('cyclesPerFrame'),
  followPc: $('followPc'),
  autoMem: $('autoMem'),
  memBase: $('memBase'),
  density: $('density'),
  showRaw: $('showRaw'),
  showAbi: $('showAbi'),
  fileInput: $('fileInput'),
  fileLabel: $('fileLabel'),
};

const panels = {
  toolbar: document.querySelector('.toolbar'),
  pipelinePanel: document.querySelector('.pipelinePanel'),
  disasmPanel: document.querySelector('.disasmPanel'),
  lensPanel: document.querySelector('.lensPanel'),
  regsPanel: document.querySelector('.regsPanel'),
  memPanel: document.querySelector('.memPanel'),
  uartPanel: document.querySelector('.uartPanel'),
};

const StageOrder = ['IF', 'ID', 'EX', 'MEM', 'WB'];

function hex32(x) {
  const u = (x >>> 0).toString(16).padStart(8, '0');
  return '0x' + u;
}

function safeParseHex(s, fallback = 0) {
  try {
    const t = (s || '').trim().toLowerCase();
    if (t.startsWith('0x')) return parseInt(t.slice(2), 16) >>> 0;
    if (t === '') return fallback;
    return parseInt(t, 16) >>> 0;
  } catch {
    return fallback;
  }
}

function clamp(n, lo, hi) {
  return Math.max(lo, Math.min(hi, n));
}

// -------- UI helpers --------
const LS_KEY = 'rv32sim_ui_v1';
let toastTimer = null;

function showToast(msg, kind = '') {
  if (!el.toast) return;
  el.toast.textContent = String(msg);
  el.toast.classList.remove('hidden', 'good', 'bad');
  if (kind === 'good') el.toast.classList.add('good');
  if (kind === 'bad') el.toast.classList.add('bad');

  if (toastTimer) window.clearTimeout(toastTimer);
  toastTimer = window.setTimeout(() => {
    el.toast.classList.add('hidden');
  }, 2400);
}

function setTopbarHeightVar() {
  const topbar = document.querySelector('.topbar');
  const h = topbar ? topbar.offsetHeight : 58;
  document.documentElement.style.setProperty('--topbar-h', `${h}px`);
}

function parseMaybeHex(s) {
  const t = (s || '').trim().toLowerCase();
  if (!t) return null;
  if (t.startsWith('0x')) {
    const v = parseInt(t.slice(2), 16);
    return Number.isFinite(v) ? (v >>> 0) : null;
  }
  if (/^[0-9a-f]+$/i.test(t)) {
    const v = parseInt(t, 16);
    return Number.isFinite(v) ? (v >>> 0) : null;
  }
  return null;
}

function loadUISettings() {
  try {
    const raw = localStorage.getItem(LS_KEY);
    if (!raw) return null;
    return JSON.parse(raw);
  } catch {
    return null;
  }
}

let saveTimer = null;
function saveUISettingsSoon(extra = {}) {
  if (saveTimer) window.clearTimeout(saveTimer);
  saveTimer = window.setTimeout(() => {
    try {
      const collapsed = {};
      for (const [k, p] of Object.entries(panels)) {
        if (!p) continue;
        if (k === 'toolbar') continue;
        collapsed[k] = p.classList.contains('collapsed');
      }

      const maximized = Object.entries(panels).find(([,p]) => p && p.classList.contains('maximized'))?.[0] || '';

      const data = {
        cyclesPerFrame: String(el.cyclesPerFrame?.value ?? '50'),
        followPc: !!el.followPc?.checked,
        autoMem: !!el.autoMem?.checked,
        memBase: String(el.memBase?.value ?? '0x00000000'),
        density: String(el.density?.value ?? 'comfortable'),
        showRaw: !!el.showRaw?.checked,
        showAbi: !!el.showAbi?.checked,
        selectedStage: String(selectedStage || 'EX'),
        disasmQuery: String(el.disasmQuery?.value ?? ''),
        collapsed,
        maximized,
        ...extra,
      };
      localStorage.setItem(LS_KEY, JSON.stringify(data));
    } catch {}
  }, 250);
}

let Module;
let sim = {
  handle: 0,
  create: null,
  destroy: null,
  load_program: null,
  soft_reset: null,
  set_pc: null,
  step_cycles: null,
  step_commit: null,
  get_state_json: null,
  get_disasm_json: null,
  get_mem_json: null,
};

let running = false;
let selectedStage = 'EX';

let disasmBase = 0;
let memBase = 0;

let lastDisasmLines = [];
let disasmSearchSelPc = 0xFFFFFFFF;
let memJumpOnceAddr = null;
let disasmSuppressScrollRestore = false;
let memSuppressScrollRestore = false;
let lastBranchMisp = 0;
let haltedToastShown = false;

function applyInitialUISettings() {
  setTopbarHeightVar();
  window.addEventListener('resize', setTopbarHeightVar);

  const s = loadUISettings();
  if (s) {
    if (s.cyclesPerFrame) el.cyclesPerFrame.value = String(s.cyclesPerFrame);
    if (typeof s.followPc === 'boolean') el.followPc.checked = s.followPc;
    if (typeof s.autoMem === 'boolean') el.autoMem.checked = s.autoMem;
    if (s.memBase) el.memBase.value = String(s.memBase);
    if (s.density) el.density.value = String(s.density);
    if (typeof s.showRaw === 'boolean') el.showRaw.checked = s.showRaw;
    if (typeof s.showAbi === 'boolean') el.showAbi.checked = s.showAbi;

    if (s.selectedStage && StageOrder.includes(s.selectedStage)) selectedStage = s.selectedStage;
    if (typeof s.disasmQuery === 'string') el.disasmQuery.value = s.disasmQuery;

    if (s.collapsed && typeof s.collapsed === 'object') {
      for (const [k, v] of Object.entries(s.collapsed)) {
        if (k in panels) setPanelCollapsed(k, !!v);
      }
    }

    if (s.maximized && (s.maximized in panels)) {
      setPanelMaximized(s.maximized, true);
    }
  }

  applyDensityUI();
  applyShowRawUI();
  for (const k of Object.keys(panels)) updatePanelButtons(k);
}

applyInitialUISettings();

// Change tracking for "follow along" highlighting
let prevPc = 0;
let prevRegs = null;
let prevMemFocus = 0;

function isTypingInField() {
  const a = document.activeElement;
  if (!a) return false;
  const tag = (a.tagName || '').toLowerCase();
  if (tag === 'input' || tag === 'textarea' || tag === 'select') return true;
  // contenteditable
  if (a.isContentEditable) return true;
  return false;
}

function scrollIntoViewCenter(container, element) {
  if (!container || !element) return;
  // scrollIntoView usually picks the nearest scrollable ancestor; we want "center-ish"
  const cRect = container.getBoundingClientRect();
  const eRect = element.getBoundingClientRect();
  const above = eRect.top < cRect.top;
  const below = eRect.bottom > cRect.bottom;
  if (above || below) {
    // compute desired scrollTop
    const delta = (eRect.top - cRect.top) - (cRect.height/2 - eRect.height/2);
    container.scrollTop = container.scrollTop + delta;
  }
}

function abiName(i) {
  const map = {
    0:'zero',1:'ra',2:'sp',3:'gp',4:'tp',5:'t0',6:'t1',7:'t2',
    8:'s0/fp',9:'s1',10:'a0',11:'a1',12:'a2',13:'a3',14:'a4',15:'a5',
    16:'a6',17:'a7',18:'s2',19:'s3',20:'s4',21:'s5',22:'s6',23:'s7',24:'s8',25:'s9',26:'s10',27:'s11',
    28:'t3',29:'t4',30:'t5',31:'t6'
  };
  return map[i] || '';
}

function getAsmClass(asm) {
  const mnemonic = (asm || '').trim().split(/[\s,]/)[0].toLowerCase();
  if (/^b(eq|ne|lt|ge|ltu|geu)$/.test(mnemonic)) return 'asm-branch';
  if (/^jal/.test(mnemonic)) return 'asm-jump';
  if (/^l(b|h|w|bu|hu)$/.test(mnemonic)) return 'asm-load';
  if (/^s(b|h|w)$/.test(mnemonic)) return 'asm-store';
  if (/^(ecall|ebreak|csr)/.test(mnemonic)) return 'asm-system';
  return '';
}

function setRunUI() {
  el.btnRun.textContent = running ? '⏸ Pause' : '▶ Run';
  el.btnRun.classList.toggle('primary', !running);
  if (el.runBadge && !el.runBadge.classList.contains('badge-halted')) {
    el.runBadge.textContent = running ? '● RUNNING' : '● PAUSED';
    el.runBadge.classList.remove('badge-running', 'badge-paused');
    el.runBadge.classList.add(running ? 'badge-running' : 'badge-paused');
  }
}

function setHaltBadge(isHalted) {
  if (!el.runBadge) return;
  el.runBadge.classList.remove('badge-running', 'badge-paused', 'badge-halted');
  if (isHalted) {
    el.runBadge.textContent = '● HALTED';
    el.runBadge.classList.add('badge-halted');
  } else {
    el.runBadge.textContent = running ? '● RUNNING' : '● PAUSED';
    el.runBadge.classList.add(running ? 'badge-running' : 'badge-paused');
  }
}

function stageSummary(st) {
  if (!st.valid) return '(bubble)';
  const id = st.instr_id;
  const pc = hex32(st.pc);
  const asm = st.asm || '';
  return `id ${id}  pc ${pc}  ${asm}`;
}

function renderPipeline(stages) {
  el.pipeline.innerHTML = '';
  for (let i = 0; i < StageOrder.length; i++) {
    const name = StageOrder[i];
    const st = stages[name];
    const box = document.createElement('div');
    box.className = 'stage' + (st.valid ? '' : ' invalid') + (name === selectedStage ? ' selected' : '');
    box.tabIndex = 0;
    box.setAttribute('role', 'button');
    box.setAttribute('aria-label', `Pipeline stage ${name}`);
    const activate = () => { selectedStage = name; saveUISettingsSoon(); refresh(); };
    box.onclick = activate;
    box.onkeydown = (e) => {
      const k = (e.key || '').toLowerCase();
      if (k === 'enter' || k === ' ') { e.preventDefault(); activate(); }
    };

    const hdr = document.createElement('div');
    hdr.className = 'stage-hdr';
    hdr.innerHTML = `<span class="stage-name">${name}</span><span class="stage-id mono">${st.valid ? ('id ' + st.instr_id) : ''}</span>`;

    const pc = document.createElement('div');
    pc.className = 'stage-pc mono';
    pc.textContent = st.valid ? hex32(st.pc) : '';

    const asm = document.createElement('div');
    asm.className = 'stage-asm mono';
    asm.textContent = st.valid ? (st.asm || '') : '(bubble)';

    const meta = [];
    if ((name === 'ID' || name === 'EX') && st.valid && st.is_branch) {
      meta.push(`pred ${st.pred_taken ? 'T' : 'NT'} → ${hex32(st.pred_target)}`);
    }
    if (name === 'MEM' && st.valid && (st.mem_r || st.mem_w)) {
      meta.push(`${st.mem_r ? '<span class="op-load">load</span>' : '<span class="op-store">store</span>'} @ ${hex32(st.mem_addr)}`);
    }
    if (name === 'WB' && st.valid && st.wb_w && st.wb_rd !== 0) {
      meta.push(`x${st.wb_rd} ← ${hex32(st.wb_data)}`);
    }

    const metaDiv = document.createElement('div');
    metaDiv.className = 'stage-meta mono';
    metaDiv.innerHTML = meta.join('  ');

    box.appendChild(hdr);
    box.appendChild(pc);
    box.appendChild(asm);
    box.appendChild(metaDiv);
    el.pipeline.appendChild(box);

    if (i < StageOrder.length - 1) {
      const conn = document.createElement('div');
      conn.className = 'stage-connector';
      conn.setAttribute('aria-hidden', 'true');
      conn.textContent = '›';
      el.pipeline.appendChild(conn);
    }
  }
}

function renderLens(st) {
  el.lensTitle.textContent = st.valid ? `${selectedStage}: id ${st.instr_id} @ ${hex32(st.pc)}` : `${selectedStage}: bubble`;

  const left = document.createElement('div');
  left.className = 'lens-box';
  left.innerHTML = `
    <div class="lens-box-title">Decode</div>
    <div class="mono">
      raw: ${st.valid ? hex32(st.raw) : '—'}<br/>
      asm: ${st.valid ? st.asm : '—'}<br/>
      rs1: x${st.rs1}  rs2: x${st.rs2}  rd: x${st.rd}<br/>
      imm: ${st.imm}<br/>
      uses_rs1: ${st.uses_rs1}  uses_rs2: ${st.uses_rs2}  writes_rd: ${st.writes_rd}<br/>
      mem: r=${st.mem_read} w=${st.mem_write} sz=${st.mem_size} sgn=${st.mem_signed} to_reg=${st.mem_to_reg}<br/>
      ctrl: branch=${st.is_branch} jump=${st.is_jump}<br/>
      csr: 0x${(st.csr_addr>>>0).toString(16).padStart(3,'0')} zimm=${st.csr_zimm}
    </div>
  `;

  const right = document.createElement('div');
  right.className = 'lens-box';
  const extras = [];
  if ((selectedStage === 'ID' || selectedStage === 'EX') && st.valid && st.is_branch) {
    extras.push(`Prediction: ${st.pred_taken ? 'TAKEN' : 'NOT TAKEN'} → ${hex32(st.pred_target)}`);
  }
  if (selectedStage === 'MEM' && st.valid && (st.mem_r || st.mem_w)) {
    extras.push(`${st.mem_r ? 'Load' : 'Store'} addr: ${hex32(st.mem_addr)}`);
    if (st.mem_w) extras.push(`Store data: ${hex32(st.store_data)}`);
  }
  if (selectedStage === 'WB' && st.valid && st.wb_w && st.wb_rd !== 0) {
    extras.push(`Writeback: x${st.wb_rd} ← ${hex32(st.wb_data)}`);
  }
  if (!extras.length) extras.push('—');

  right.innerHTML = `
    <div class="lens-box-title">Stage Details</div>
    <div class="mono">${extras.join('<br/>')}</div>
  `;

  el.lens.innerHTML = '';
  el.lens.appendChild(left);
  el.lens.appendChild(right);
}

function renderDisasm(lines, pc, prevPc, queryLower, selPc) {
  el.disasm.innerHTML = '';
  let pcRow = null;
  let prevRow = null;
  let selRow = null;

  for (const ln of lines) {
    const row = document.createElement('div');
    const isPc = ((ln.pc >>> 0) === (pc >>> 0));
    const isPrev = ((ln.pc >>> 0) === (prevPc >>> 0)) && !isPc;
    const isSel = ((ln.pc >>> 0) === (selPc >>> 0));

    const asmText = String(ln.asm || '');
    const isHit = !!queryLower && asmText.toLowerCase().includes(queryLower);

    row.className = 'line'
      + (isPc ? ' pc pc-flash' : '')
      + (isPrev ? ' prevPc' : '')
      + (isHit ? ' searchHit' : '')
      + (isSel ? ' searchSel' : '');
    row.dataset.pc = String(ln.pc >>> 0);

    row.tabIndex = 0;
    row.setAttribute('role', 'button');
    row.setAttribute('aria-label', `Set PC to ${hex32(ln.pc)}`);

    const asmClass = getAsmClass(asmText);
    row.innerHTML = `
      <div class="pcCol">${hex32(ln.pc)}</div>
      <div class="rawCol">${ln.mapped ? hex32(ln.raw) : '—'}</div>
      <div class="asmCol${asmClass ? ' ' + asmClass : ''}">${ln.asm}</div>
    `;
    const activate = () => { sim.set_pc(sim.handle, ln.pc >>> 0); refresh(); };
    row.onclick = activate;
    row.onkeydown = (e) => {
      const k = (e.key || '').toLowerCase();
      if (k === 'enter' || k === ' ') {
        e.preventDefault();
        activate();
      }
    };

    if (isPc) pcRow = row;
    if (isPrev) prevRow = row;
    if (isSel) selRow = row;

    el.disasm.appendChild(row);
  }

  // Follow along: keep current PC visible when enabled
  if (el.followPc.checked && pcRow) {
    // Make sure the row is visible inside the disasm scroller
    scrollIntoViewCenter(el.disasm, pcRow);
  }

  // Search selection should win over follow (explicit user action)
  if (selRow) {
    scrollIntoViewCenter(el.disasm, selRow);
  }

  return { pcRow, prevRow, selRow };
}

function renderRegs(regs, lastCommit, prevRegs) {
  const showAbi = !!el.showAbi?.checked;
  let focusRd = -1;
  let focusVal = 0;
  if (lastCommit && lastCommit.valid && lastCommit.reg_write && (lastCommit.rd >>> 0) !== 0) {
    focusRd = lastCommit.rd >>> 0;
    focusVal = lastCommit.wb_data >>> 0;
  }

  const changed = new Set();
  if (prevRegs && prevRegs.length === 32) {
    for (let i = 0; i < 32; ++i) {
      if (((prevRegs[i]>>>0) !== (regs[i]>>>0))) changed.add(i);
    }
  }

  const favIdx = [1,2,3,4,10,11,12,13,14,15,16,17];
  const rows = [];

  const focusText = (focusRd >= 0)
    ? `last write: x${focusRd} (${abiName(focusRd)}) = ${hex32(focusVal)}`
    : 'last write: —';
  rows.push(`<div class="focus-line mono">${focusText}</div>`);

  rows.push('<div class="reg-fav-grid">');
  for (const idx of favIdx) {
    const abi = abiName(idx);
    const name = showAbi && abi ? `x${idx} (${abi})` : `x${idx}`;
    const isFocus = (idx === focusRd);
    const isChg = changed.has(idx);
    rows.push(
      `<div class="reg-chip ${isFocus ? 'focus' : ''} ${isChg ? 'chg' : ''}" data-reg="${idx}">
        <div class="reg-chip-name">${name}</div>
        <div class="reg-chip-val">${hex32(regs[idx]>>>0)}</div>
      </div>`
    );
  }
  rows.push('</div>');
  rows.push('<div class="reg-divider"></div>');
  rows.push('<div class="reg-list">');
  for (let i = 0; i < 32; ++i) {
    const isFocus = (i === focusRd);
    const isChg = changed.has(i);
    const abi = abiName(i);
    const label = (showAbi && abi) ? `x${i} (${abi})` : `x${i}`;
    rows.push(
      `<div class="regRow ${isFocus ? 'focus' : ''} ${isChg ? 'chg' : ''}" data-reg="${i}">
        <div class="r">${label}</div><div>${hex32(regs[i]>>>0)}</div>
      </div>`
    );
  }
  rows.push('</div>');

  el.regs.innerHTML = rows.join('');

  if (focusRd >= 0) {
    const node = el.regs.querySelector(`.regRow[data-reg="${focusRd}"]`);
    if (node) scrollIntoViewCenter(el.regs, node);
  }
}

function renderMem(rows, base, focusAddr, lastWasStore) {
  const html = [];
  html.push('<table class="mem-table">');
  html.push('<thead><tr><th>Address</th><th>Words (u32 · little-endian)</th></tr></thead>');
  html.push('<tbody>');
  const f = (focusAddr >>> 0);
  const focusRowAddr = (f & ~0xF) >>> 0;
  const focusWordIndex = ((f - focusRowAddr) >>> 2) & 3;

  for (const r of rows) {
    const a = r.addr >>> 0;
    const isFocusRow = (a === focusRowAddr);
    html.push(`<tr class="${isFocusRow ? (lastWasStore ? 'focus-store' : 'focus-load') : ''}" data-addr="${a}">`);
    html.push(`<td class="addr">${hex32(a)}</td>`);
    html.push('<td class="words">');
    for (let wi = 0; wi < r.w.length; ++wi) {
      const word = r.w[wi] >>> 0;
      const isFocusWord = isFocusRow && (wi === focusWordIndex);
      html.push(`<span class="mem-word ${isFocusWord ? 'focus-word' : ''}">${hex32(word)}</span>`);
    }
    html.push('</td></tr>');
  }
  html.push('</tbody>');
  html.push('</table>');
  el.mem.innerHTML = html.join('');

  const end = (base + rows.length * 16) >>> 0;
  el.memRange.textContent = `${hex32(base)} … ${hex32(end)}`;

  if (el.autoMem.checked) {
    const row = el.mem.querySelector(`tr[data-addr="${focusRowAddr}"]`);
    if (row) scrollIntoViewCenter(el.mem, row);
  }

  if (memJumpOnceAddr !== null) {
    const row = el.mem.querySelector(`tr[data-addr="${focusRowAddr}"]`);
    if (row) scrollIntoViewCenter(el.mem, row);
    memJumpOnceAddr = null;
  }
}

function applyDensityUI() {
  const v = String(el.density?.value || 'comfortable');
  document.body.classList.toggle('density-compact', v === 'compact');
}

function applyShowRawUI() {
  const show = !!el.showRaw?.checked;
  document.body.classList.toggle('hideRaw', !show);
}

function updatePanelButtons(panelKey) {
  const p = panels[panelKey];
  if (!p) return;
  const btnC = p.querySelector('button[data-action="collapse"][data-panel]');
  if (btnC) btnC.textContent = p.classList.contains('collapsed') ? '▸' : '▾';
  const btnM = p.querySelector('button[data-action="maximize"][data-panel]');
  if (btnM) btnM.textContent = p.classList.contains('maximized') ? '⤡' : '⤢';
}

function setPanelCollapsed(panelKey, collapsed) {
  const p = panels[panelKey];
  if (!p) return;
  p.classList.toggle('collapsed', !!collapsed);
  updatePanelButtons(panelKey);
}

function setPanelMaximized(panelKey, maximized) {
  // only one maximized at a time
  for (const [k, p] of Object.entries(panels)) {
    if (!p || k === 'toolbar') continue;
    if (k !== panelKey) p.classList.remove('maximized');
  }
  const p = panels[panelKey];
  if (!p) return;
  p.classList.toggle('maximized', !!maximized);
  for (const k of Object.keys(panels)) updatePanelButtons(k);
}

function buildExampleMenu() {
  const menu = el.exampleMenu;
  if (!menu) return;
  menu.innerHTML = '';
  const title = document.createElement('div');
  title.className = 'example-menu-title';
  title.textContent = 'Choose an example';
  menu.appendChild(title);
  for (const ex of EXAMPLES) {
    const item = document.createElement('div');
    item.className = 'example-item';
    item.setAttribute('role', 'menuitem');
    item.tabIndex = 0;
    item.innerHTML = `<div class="example-item-name">${ex.name}</div><div class="example-item-desc">${ex.desc}</div>`;
    const pick = () => {
      closeExampleMenu();
      loadExample(ex.file, ex.name).catch(e => showToast(String(e), 'bad'));
    };
    item.onclick = pick;
    item.onkeydown = (e) => { if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); pick(); } };
    menu.appendChild(item);
  }
}

function openExampleMenu() {
  if (!el.exampleMenu) return;
  const r = el.btnLoadExample.getBoundingClientRect();
  el.exampleMenu.style.top  = (r.bottom + 6) + 'px';
  el.exampleMenu.style.right = (window.innerWidth - r.right) + 'px';
  el.exampleMenu.style.left = 'auto';
  el.exampleMenu.classList.remove('hidden');
}

function closeExampleMenu() {
  if (!el.exampleMenu) return;
  el.exampleMenu.classList.add('hidden');
}

async function loadExample(file = 'examples/test.hex', name = 'example') {
  const resp = await fetch(file);
  if (!resp.ok) throw new Error('Failed to fetch ' + name);
  const buf = await resp.arrayBuffer();
  const data = new Uint8Array(buf);
  const safeName = file.replace(/[^a-zA-Z0-9_.\-/]/g, '_').replace(/^examples\//, '');
  const path = '/tmp/' + safeName;
  Module.FS.writeFile(path, data);
  const entry = sim.load_program(sim.handle, path);
  if ((entry >>> 0) === 0xFFFFFFFF) throw new Error('load_program failed for ' + name);
  memBase = 0;
  disasmBase = 0;
  el.memBase.value = '0x00000000';
  disasmSearchSelPc = 0xFFFFFFFF;
  haltedToastShown = false;
  refresh();
  saveUISettingsSoon();
  showToast('Loaded: ' + name, 'good');
}

async function loadFile(file) {
  const data = new Uint8Array(await file.arrayBuffer());
  const safeName = file.name.replace(/[^a-zA-Z0-9_.-]/g, '_');
  const path = '/tmp/' + safeName;
  Module.FS.writeFile(path, data);
  const entry = sim.load_program(sim.handle, path);
  if ((entry >>> 0) === 0xFFFFFFFF) throw new Error('load_program failed');
  memBase = 0;
  disasmBase = 0;
  el.memBase.value = '0x00000000';
  disasmSearchSelPc = 0xFFFFFFFF;
  refresh();
  saveUISettingsSoon();
}

function refresh() {
  if (!Module) return;

  // Preserve manual scroll positions (unless follow/auto is enabled, or we intentionally navigated)
  const disasmScrollTop = el.disasm ? el.disasm.scrollTop : 0;
  const memScrollTop = el.mem ? el.mem.scrollTop : 0;

  const stateStr = Module.ccall('sim_get_state_json', 'string', ['number'], [sim.handle]);
  const state = JSON.parse(stateStr);

  const pc = state.pc >>> 0;
  const stages = state.stages;

  // top lines
  el.nowLine.textContent = `PC ${hex32(pc)}   |   IF: ${stages.IF.valid ? stages.IF.asm : '(bubble)'}   |   halted: ${state.halted}`;

  const s = state.stats;
  const cpi = (s.committed ? (s.cycles / s.committed) : 0).toFixed(3);
  el.statsLine.innerHTML = `cycles <span class="mono">${s.cycles}</span> &nbsp; committed <span class="mono">${s.committed}</span> &nbsp; CPI <span class="mono">${cpi}</span><br/>
    stalls <span class="mono">${s.stall_cycles}</span> &nbsp; br_misp <span class="mono">${s.branch_misp}</span>`;

  if ((s.branch_misp >>> 0) > (lastBranchMisp >>> 0) && panels.pipelinePanel) {
    panels.pipelinePanel.classList.remove('misp-flash');
    void panels.pipelinePanel.offsetWidth;
    panels.pipelinePanel.classList.add('misp-flash');
    window.setTimeout(() => panels.pipelinePanel?.classList.remove('misp-flash'), 380);
  }
  lastBranchMisp = s.branch_misp >>> 0;

  if (state.last_commit && state.last_commit.valid) {
    const lc = state.last_commit;
    const w = lc.reg_write && lc.rd !== 0 ? `x${lc.rd} <= ${hex32(lc.wb_data)}` : '';
    const m = lc.mem_is_store ? `store ${hex32(lc.mem_addr)} <= ${hex32(lc.mem_store_data)}` : (lc.mem_is_load ? `load ${hex32(lc.mem_addr)}` : '');
    el.commitLine.textContent = `Last commit: pc ${hex32(lc.pc)}  |  ${lc.asm}  |  ${w} ${m}`.trim();
  } else {
    el.commitLine.textContent = 'Last commit: —';
  }

  // Halt handling (auto-stop)
  if (state.halted) {
    if (running) {
      running = false;
      setRunUI();
    }
    if (!haltedToastShown) {
      showToast('CPU halted', 'bad');
      haltedToastShown = true;
    }
  } else {
    haltedToastShown = false;
  }

  setHaltBadge(!!state.halted);

  // follow PC behavior for disasm base
  const followPc = el.followPc.checked;
  if (followPc) {
    disasmBase = (pc - 24 * 4) >>> 0;
  }

  // auto memory follow
  const autoMem = el.autoMem.checked;
  if (autoMem && state.last_commit && state.last_commit.valid) {
    const lc = state.last_commit;
    if (lc.mem_is_load || lc.mem_is_store) {
      memBase = (lc.mem_addr & ~0xF) >>> 0;
      el.memBase.value = hex32(memBase);
    }
  } else {
    const parsed = safeParseHex(el.memBase.value, memBase);
    memBase = parsed >>> 0;
  }

  // pipeline
  renderPipeline(stages);

  // lens
  renderLens(stages[selectedStage]);

  // disassembly
  const disStr = Module.ccall('sim_get_disasm_json', 'string', ['number','number','number'], [sim.handle, disasmBase >>> 0, 120]);
  const lines = JSON.parse(disStr);
  lastDisasmLines = Array.isArray(lines) ? lines : [];
  el.disasmRange.textContent = `${hex32(disasmBase>>>0)} … ${hex32((disasmBase + lines.length*4)>>>0)}`;

  const q = (el.disasmQuery?.value || '').trim();
  if (!q) disasmSearchSelPc = 0xFFFFFFFF;
  const qAddr = parseMaybeHex(q);
  const qLower = (q && qAddr === null) ? q.toLowerCase() : '';
  renderDisasm(lines, pc, prevPc, qLower, disasmSearchSelPc >>> 0);

  // regs
  renderRegs(state.regs, state.last_commit, prevRegs);

  // memory table
// Determine memory focus (last commit mem addr if any, else MEM stage addr)
let focusAddr = memBase >>> 0;
let lastWasStore = false;
if (state.last_commit && state.last_commit.valid && (state.last_commit.mem_is_load || state.last_commit.mem_is_store)) {
  focusAddr = state.last_commit.mem_addr >>> 0;
  lastWasStore = !!state.last_commit.mem_is_store;
} else if (stages.MEM && stages.MEM.valid && (stages.MEM.mem_r || stages.MEM.mem_w)) {
  focusAddr = stages.MEM.mem_addr >>> 0;
  lastWasStore = !!stages.MEM.mem_w;
}

  const memStr = Module.ccall('sim_get_mem_json', 'string', ['number','number','number'], [sim.handle, memBase >>> 0, 24]);
  const memRows = JSON.parse(memStr);
  renderMem(memRows, memBase >>> 0, focusAddr, lastWasStore);

  // uart
  el.uart.textContent = state.uart || '';

  // bottom info
  el.followInfo.textContent = `selected: ${selectedStage}`;

  // Restore scroll positions when user is manually scrolling these panels.
  if (el.disasm && !el.followPc.checked && !disasmSuppressScrollRestore) {
    el.disasm.scrollTop = disasmScrollTop;
  }
  if (el.mem && !el.autoMem.checked && !memSuppressScrollRestore) {
    el.mem.scrollTop = memScrollTop;
  }
  disasmSuppressScrollRestore = false;
  memSuppressScrollRestore = false;

  // Update previous snapshot for follow-along highlighting
  prevPc = pc >>> 0;
  prevRegs = state.regs.slice(0);
  prevMemFocus = focusAddr >>> 0;
}

let lastUI = 0;
function tick(ts) {
  if (!Module) return requestAnimationFrame(tick);

  if (running) {
    const cpf = clamp(parseInt(el.cyclesPerFrame.value || '50', 10), 1, 5000);
    sim.step_cycles(sim.handle, cpf >>> 0);
  }

  // While running, throttle UI rendering for smoother feel + lower CPU.
  const minDelta = running ? 66 : 0; // ~15 fps when running
  if (!running || (ts - lastUI) >= minDelta) {
    refresh();
    lastUI = ts;
  }

  requestAnimationFrame(tick);
}

function bindTooltips() {
  document.querySelectorAll('.toggle-wrap[data-tip]').forEach(el => {
    el.addEventListener('mouseenter', () => {
      const r = el.getBoundingClientRect();
      const x = r.left + r.width / 2;
      const y = r.bottom + 8;
      el.style.setProperty('--tip-x', x + 'px');
      el.style.setProperty('--tip-y', y + 'px');
    });
  });
}

function bindUI() {
  el.btnRun.onclick = () => { running = !running; setRunUI(); };
  el.btnStep.onclick = () => { running = false; setRunUI(); sim.step_cycles(sim.handle, 1); refresh(); };
  el.btnCommit.onclick = () => { running = false; setRunUI(); sim.step_commit(sim.handle, 5000); refresh(); };
  el.btnReset.onclick = () => {
    running = false;
    setRunUI();
    sim.soft_reset(sim.handle);
    disasmSearchSelPc = 0xFFFFFFFF;
    refresh();
    saveUISettingsSoon();
  };

  // Persist settings from UI controls
  el.cyclesPerFrame.onchange = () => saveUISettingsSoon();
  el.followPc.onchange = () => { saveUISettingsSoon(); };
  el.autoMem.onchange = () => { saveUISettingsSoon(); };
  el.memBase.onchange = () => { memBase = safeParseHex(el.memBase.value, memBase); saveUISettingsSoon(); refresh(); };

  el.density.onchange = () => { applyDensityUI(); saveUISettingsSoon(); };
  el.showRaw.onchange = () => { applyShowRawUI(); saveUISettingsSoon(); refresh(); };
  el.showAbi.onchange = () => { saveUISettingsSoon(); refresh(); };

  buildExampleMenu();
  bindTooltips();

  el.btnLoadExample.onclick = (e) => {
    e.stopPropagation();
    if (el.exampleMenu.classList.contains('hidden')) {
      openExampleMenu();
    } else {
      closeExampleMenu();
    }
  };

  // Close example menu when clicking outside
  document.addEventListener('click', (e) => {
    if (el.exampleMenu && !el.exampleMenu.classList.contains('hidden')) {
      if (!el.exampleMenu.contains(e.target) && e.target !== el.btnLoadExample) {
        closeExampleMenu();
      }
    }
  });

  el.fileInput.onchange = async (evt) => {
    const f = evt.target.files && evt.target.files[0];
    if (!f) return;
    try { await loadFile(f); }
    catch (e) { console.error(e); showToast(String(e), 'bad'); }
    finally { el.fileInput.value = ''; }
  };

  // File label keyboard support
  el.fileLabel.onkeydown = (e) => {
    const k = (e.key || '').toLowerCase();
    if (k === 'enter' || k === ' ') {
      e.preventDefault();
      el.fileInput.click();
    }
  };

  // Disasm search / jump
  const findNext = () => {
    const q = (el.disasmQuery.value || '').trim();
    if (!q) return showToast('Enter a mnemonic or address to search.', 'bad');
    const addr = parseMaybeHex(q);
    if (addr !== null) {
      // manual navigation: disable Follow PC so it doesn't snap back
      if (el.followPc.checked) {
        el.followPc.checked = false;
        saveUISettingsSoon();
        showToast('Follow PC disabled for manual jump.', 'good');
      }
      disasmBase = (addr - 24 * 4) >>> 0;
      disasmSearchSelPc = addr >>> 0;
      disasmSuppressScrollRestore = true;
      refresh();
      return;
    }

    const qLower = q.toLowerCase();
    const hits = (lastDisasmLines || []).filter(ln => String(ln.asm || '').toLowerCase().includes(qLower));
    if (!hits.length) return showToast('No matches in current disassembly window.', 'bad');

    // pick next match after current selection
    const cur = disasmSearchSelPc >>> 0;
    let next = hits.find(h => (h.pc >>> 0) > cur);
    if (!next) next = hits[0];
    disasmSearchSelPc = next.pc >>> 0;
    disasmSuppressScrollRestore = true;
    refresh();
  };

  el.btnDisasmFind.onclick = () => findNext();
  el.btnDisasmJump.onclick = () => {
    const q = (el.disasmQuery.value || '').trim();
    const addr = parseMaybeHex(q);
    if (addr === null) return findNext();
    // Jump view (same as address search)
    if (el.followPc.checked) {
      el.followPc.checked = false;
      saveUISettingsSoon();
      showToast('Follow PC disabled for manual jump.', 'good');
    }
    disasmBase = (addr - 24 * 4) >>> 0;
    disasmSearchSelPc = addr >>> 0;
    disasmSuppressScrollRestore = true;
    refresh();
  };
  el.disasmQuery.onkeydown = (e) => {
    if (e.key === 'Enter') {
      e.preventDefault();
      findNext();
    }
  };
  el.disasmQuery.oninput = () => saveUISettingsSoon();

  // Memory jump
  el.btnMemJump.onclick = () => {
    const q = (el.memJump.value || '').trim();
    const addr = parseMaybeHex(q);
    if (addr === null) return showToast('Enter a hex address like 0x2000.', 'bad');
    if (el.autoMem.checked) {
      el.autoMem.checked = false;
      showToast('Auto mem disabled for manual jump.', 'good');
    }
    memBase = (addr & ~0xF) >>> 0;
    el.memBase.value = hex32(memBase);
    memJumpOnceAddr = addr >>> 0;
    memSuppressScrollRestore = true;
    saveUISettingsSoon();
    refresh();
  };
  el.memJump.onkeydown = (e) => {
    if (e.key === 'Enter') {
      e.preventDefault();
      el.btnMemJump.click();
    }
  };

  // Panel controls (collapse/maximize)
  document.querySelectorAll('button[data-action][data-panel]').forEach(btn => {
    btn.addEventListener('click', () => {
      const action = btn.getAttribute('data-action');
      const panelKey = btn.getAttribute('data-panel');
      if (!action || !panelKey) return;
      if (action === 'collapse') {
        const p = panels[panelKey];
        if (!p) return;
        setPanelCollapsed(panelKey, !p.classList.contains('collapsed'));
        saveUISettingsSoon();
        return;
      }
      if (action === 'maximize') {
        const p = panels[panelKey];
        if (!p) return;
        setPanelMaximized(panelKey, !p.classList.contains('maximized'));
        saveUISettingsSoon();
      }
    });
  });
}

// Keyboard shortcuts (Alt+key). Note: some browsers reserve certain combos.
document.addEventListener('keydown', (e) => {
  // Exit maximize with Esc
  if (e.key === 'Escape' && !isTypingInField()) {
    const anyMax = Object.entries(panels).find(([,p]) => p && p.classList.contains('maximized'));
    if (anyMax) {
      e.preventDefault();
      for (const [k, p] of Object.entries(panels)) {
        if (!p || k === 'toolbar') continue;
        p.classList.remove('maximized');
        updatePanelButtons(k);
      }
      saveUISettingsSoon({ maximized: '' });
      return;
    }
  }

  if (!e.altKey || e.ctrlKey || e.metaKey) return;
  if (isTypingInField()) return;

  const k = (e.key || '').toLowerCase();
  if (k === ' ') {
    e.preventDefault();
    running = !running; setRunUI();
    return;
  }
  if (k === 's') { // Step cycle
    e.preventDefault();
    running = false; setRunUI();
    sim.step_cycles(sim.handle, 1);
    refresh();
    return;
  }
  if (k === 'c') { // Step commit
    e.preventDefault();
    running = false; setRunUI();
    sim.step_commit(sim.handle, 5000);
    refresh();
    return;
  }
  if (k === 'r') { // Reset
    e.preventDefault();
    running = false; setRunUI();
    sim.soft_reset(sim.handle);
    refresh();
    return;
  }
  if (k === 'l') { // Load file dialog
    e.preventDefault();
    el.fileInput.click();
    return;
  }
  if (k === 'e') { // Toggle example menu
    e.preventDefault();
    if (el.exampleMenu.classList.contains('hidden')) openExampleMenu();
    else closeExampleMenu();
    return;
  }
});


(async function main() {
  el.boot.textContent = 'loading wasm…';

  // If the WASM bootstrap fails, the page otherwise looks "stuck".
// Surface failures directly in the header pill + console.
  try {
    // Preflight: if you later build with SINGLE_FILE this can 404 (OK).
    try { await fetch('./sim.wasm', { method: 'HEAD' }); } catch {}

    Module = await createSimModule({
      locateFile: (path, prefix) => {
        if (path.endsWith('.wasm')) return './sim.wasm';
        return prefix + path;
      },
      printErr: (...args) => console.error(...args),
    });
  } catch (e) {
    console.error('WASM init failed:', e);
    el.boot.textContent = 'FAIL';
    el.boot.classList.remove('chip-init', 'chip-ready');
    el.boot.classList.add('chip-error');
    showToast('WASM init failed: ' + String(e && e.message ? e.message : e), 'bad');
    return;
  }

  // Wire C API
  sim.create       = Module.cwrap('sim_create', 'number', []);
  sim.destroy      = Module.cwrap('sim_destroy', null, ['number']);
  sim.load_program = Module.cwrap('sim_load_program', 'number', ['number','string']);
  sim.soft_reset   = Module.cwrap('sim_soft_reset', null, ['number']);
  sim.set_pc       = Module.cwrap('sim_set_pc', null, ['number','number']);
  sim.step_cycles  = Module.cwrap('sim_step_cycles', 'number', ['number','number']);
  sim.step_commit  = Module.cwrap('sim_step_commit', 'number', ['number','number']);

  sim.handle = sim.create();

  // Ensure FS exists and temp dir
  try { Module.FS.mkdir('/tmp'); } catch {}

  bindUI();
  setRunUI();

  el.boot.textContent = 'READY';
  el.boot.classList.remove('chip-init');
  el.boot.classList.add('chip-ready');

  // Auto-load default example so the page isn't empty.
  try { await loadExample('examples/test.hex', 'Array Sum'); } catch (e) { console.warn('example load failed', e); }

  requestAnimationFrame(tick);
})();
