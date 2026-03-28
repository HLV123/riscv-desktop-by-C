'use strict';

const ABI = [
  'zero','ra','sp','gp','tp','t0','t1','t2',
  'fp','s1','a0','a1','a2','a3','a4','a5',
  'a6','a7','s2','s3','s4','s5','s6','s7',
  's8','s9','s10','s11','t3','t4','t5','t6'
];

let state = null;
let disasmRows = [];
let memData = { rom: null, ram: null };
let memViewRegion = 'rom';
let memViewBase = 0;
let breakpoints = new Set();
let gpioInputBits = [0, 0];
let regFmt = 'hex';
let isRunning = false;
let runLoopId = null;
let prevRegs = new Array(32).fill(0);
let regChangedSet = new Set();
let mipsCounter = 0;
let mipsLastTime = performance.now();
let programLoaded = false;
let uartContent = '';

function fmtReg(v) {
  const u = v >>> 0;
  if (regFmt === 'hex') return '0x' + u.toString(16).padStart(8, '0');
  if (regFmt === 'dec') return String(v | 0);
  if (regFmt === 'bin') return u.toString(2).padStart(32, '0').replace(/(.{8})/g, '$1 ').trim();
  return '';
}

async function cmd(c) {
  return await window.api.cmd(c);
}

async function loadProgram(filePath) {
  programLoaded = false;
  stopLoop();
  await window.api.restart();
  const res = await cmd('load ' + filePath);
  if (!res || !res.ok) {
    alert('Failed to load: ' + (res && res.error ? res.error : 'unknown error'));
    return;
  }
  programLoaded = true;
  uartContent = '';
  document.getElementById('uart-console').textContent = '';
  breakpoints.clear();
  memData = { rom: null, ram: null };
  document.getElementById('load-overlay').style.display = 'none';

  const stateRes = await cmd('state');
  if (stateRes && !stateRes.error) applyState(stateRes);

  const disRes = await cmd('disasm');
  if (Array.isArray(disRes)) {
    disasmRows = disRes;
    buildDisasmDOM();
  }

  await fetchMemory('rom');
  renderMemory();
  setButtonsEnabled(true);
}

function applyState(s) {
  state = s;
  renderRegs();
  renderPeriph();
  updateStatePill();
  if (s.uart && s.uart.length > uartContent.length) {
    uartContent = s.uart;
    const cons = document.getElementById('uart-console');
    cons.textContent = uartContent;
    cons.scrollTop = cons.scrollHeight;
  }
}

function updateStatePill() {
  if (!state) return;
  const pill = document.getElementById('state-pill');
  const s = state.state;
  pill.textContent = s;
  pill.className = 'status-pill ' + s.toLowerCase();
  document.getElementById('cycle-display').textContent = 'Cycle: ' + state.cycle;
  const exc = state.exception && state.exception !== 'None' ? '⚠ ' + state.exception : '';
  document.getElementById('exception-label').textContent = exc;
}

function renderRegs() {
  if (!state) return;
  const body = document.getElementById('regs-body');
  regChangedSet.clear();
  for (let i = 0; i < 32; i++) {
    if (state.regs[i] !== prevRegs[i]) regChangedSet.add(i);
  }

  let html = `<div class="pc-row">
    <span style="color:var(--subtext)">PC</span>&nbsp;&nbsp;&nbsp;
    <span style="color:var(--accent)">= 0x${(state.pc >>> 0).toString(16).padStart(8,'0')}</span>
    &nbsp;&nbsp;
    <span style="color:var(--subtext);font-size:10px">Cycle: ${state.cycle}</span>
  </div><div class="reg-grid">`;

  for (let i = 0; i < 32; i++) {
    const changed = regChangedSet.has(i);
    html += `<div class="reg-row${changed ? ' changed' : ''}">
      <span class="reg-name">x${i}</span>
      <span class="reg-alias">${ABI[i]}</span>
      <span class="reg-val">${fmtReg(state.regs[i] | 0)}</span>
    </div>`;
  }
  html += '</div>';
  body.innerHTML = html;
  prevRegs = [...state.regs];
}

function renderPeriph() {
  if (!state) return;
  const g = state.gpio;
  const ledsA = document.getElementById('gpio-porta-leds');
  const ledsB = document.getElementById('gpio-portb-leds');
  ledsA.innerHTML = '';
  ledsB.innerHTML = '';

  for (let i = 0; i < 8; i++) {
    const bit = (g.porta_out >> i) & 1;
    const isOut = (g.porta_dir >> i) & 1;
    ledsA.appendChild(makeLed('PA' + i, isOut ? (bit ? 'out-on' : 'out-off') : '', false, null));
  }
  for (let i = 0; i < 8; i++) {
    const bit = (g.portb_out >> i) & 1;
    const isOut = (g.portb_dir >> i) & 1;
    ledsB.appendChild(makeLed('PB' + i, isOut ? (bit ? 'out-on' : 'out-off') : '', false, null));
  }

  const inpPins = document.getElementById('gpio-input-pins');
  inpPins.innerHTML = '';
  for (let i = 0; i < 8; i++) {
    const bit = (gpioInputBits[0] >> i) & 1;
    inpPins.appendChild(makeLed('↑PA' + i, bit ? 'inject-on' : '', true, async () => {
      gpioInputBits[0] ^= (1 << i);
      await cmd('gpio_input 0 ' + gpioInputBits[0]);
      renderPeriph();
    }));
  }

  const h2 = v => '0x' + (v >>> 0).toString(16).padStart(2, '0').toUpperCase();
  const b8 = v => (v >>> 0).toString(2).padStart(8, '0');
  const regRow = (name, addr, val) =>
    `<div class="gpio-reg-row">
      <span class="gpio-reg-name">${name}</span>
      <span class="gpio-reg-hex">${h2(val)}</span>
      <span class="gpio-reg-bin">${b8(val)}</span>
      <span class="gpio-reg-addr">@${addr}</span>
    </div>`;

  document.getElementById('gpio-regs-display').innerHTML =
    regRow('PORTA_DIR', '0x40000000', g.porta_dir) +
    regRow('PORTA_OUT', '0x40000004', g.porta_out) +
    regRow('PORTA_IN',  '0x40000008', g.porta_in)  +
    regRow('PORTA_IE',  '0x4000000C', g.porta_ie)  +
    '<div style="border-top:1px solid var(--surface1);margin:3px 0"></div>' +
    regRow('PORTB_DIR', '0x40000010', g.portb_dir) +
    regRow('PORTB_OUT', '0x40000014', g.portb_out) +
    regRow('PORTB_IN',  '0x40000018', g.portb_in)  +
    regRow('PORTB_IE',  '0x4000001C', g.portb_ie);

  const t = state.timer;
  const pct = t.load > 0 ? ((t.cnt / t.load) * 100).toFixed(1) : 0;
  document.getElementById('timer-bar').style.width = pct + '%';
  document.getElementById('timer-cnt-label').textContent = 'CNT: ' + t.cnt;
  document.getElementById('timer-load-label').textContent = 'LOAD: ' + t.load;
  document.getElementById('timer-state-label').textContent =
    (t.ctrl & 1) ? ((t.ctrl & 4) ? 'Periodic' : 'One-shot') : 'Stopped';
}

function makeLed(label, cls, clickable, onClick) {
  const wrap = document.createElement('div');
  wrap.className = 'gpio-pin';
  const led = document.createElement('div');
  led.className = 'led' + (cls ? ' ' + cls : '');
  led.title = label;
  if (clickable && onClick) led.addEventListener('click', onClick);
  const lbl = document.createElement('div');
  lbl.className = 'gpio-label';
  lbl.textContent = label;
  wrap.appendChild(led);
  wrap.appendChild(lbl);
  return wrap;
}

let disasmBuilt = false;
let lastRenderedPC = -1;

function buildDisasmDOM() {
  const body = document.getElementById('disasm-body');
  body.innerHTML = '';
  disasmBuilt = false;
  if (!disasmRows || disasmRows.length === 0) {
    body.innerHTML = '<div class="placeholder">Load a program to see disassembly.</div>';
    return;
  }
  const frag = document.createDocumentFragment();
  for (const row of disasmRows) {
    const div = document.createElement('div');
    div.className = 'disasm-row';
    div.dataset.addr = String(row.addr);
    const parts = (row.text || '').match(/^(\S+)\s*(.*)$/) || ['', '???', ''];
    const mnemonic = parts[1] || '';
    const operands  = parts[2] || '';
    div.innerHTML =
      '<span class="pc-arrow"> </span>' +
      '<span class="bp-dot off"></span>' +
      '<span class="disasm-addr">0x' + (row.addr >>> 0).toString(16).padStart(8,'0') + '</span>' +
      '<span class="disasm-hex">' + (row.raw >>> 0).toString(16).padStart(8,'0') + '</span>' +
      '<span class="disasm-inst"><span class="mnemonic">' + mnemonic + '</span> ' + operands + '</span>';
    div.addEventListener('click', (e) => {
      e.stopPropagation();
      const addr = parseInt(div.dataset.addr);
      if (breakpoints.has(addr)) {
        breakpoints.delete(addr);
        cmd('bp_del ' + addr);
      } else {
        breakpoints.add(addr);
        cmd('bp_add ' + addr);
      }
      updateDisasmHighlights();
    });
    frag.appendChild(div);
  }
  body.appendChild(frag);
  disasmBuilt = true;
  lastRenderedPC = -1;
  updateDisasmHighlights(true);
}

function updateDisasmHighlights(forceScroll) {
  if (!disasmBuilt || !state) return;
  const body = document.getElementById('disasm-body');
  const pc = state.pc >>> 0;
  let curEl = null;
  body.querySelectorAll('.disasm-row').forEach(div => {
    const addr = parseInt(div.dataset.addr);
    const isCur = addr === pc;
    const isBP  = breakpoints.has(addr);
    div.className = 'disasm-row' + (isCur ? ' current' : '') + (isBP ? ' breakpoint' : '');
    const arrow = div.querySelector('.pc-arrow');
    if (arrow) arrow.textContent = isCur ? '\u25BA' : ' ';
    const dot = div.querySelector('.bp-dot');
    if (dot) dot.className = isBP ? 'bp-dot on' : 'bp-dot off';
    if (isCur) curEl = div;
  });
  if (curEl && (forceScroll || lastRenderedPC !== pc)) {
    curEl.scrollIntoView({ block: 'center', behavior: 'smooth' });
  }
  lastRenderedPC = pc;
}

async function fetchMemory(region) {
  const res = await cmd('mem ' + region);
  if (res && res.data) {
    memData[region] = res;
  }
}

function renderMemory() {
  const body = document.getElementById('mem-body');
  const mem = memData[memViewRegion];
  if (!mem) { body.innerHTML = '<div class="placeholder">No data.</div>'; return; }

  const base = mem.base;
  const data = mem.data;
  const startOff = Math.max(0, Math.floor((memViewBase - base) / 16) * 16);
  const rows = 32;
  let html = '';
  for (let r = 0; r < rows; r++) {
    const off = startOff + r * 16;
    if (off >= data.length) break;
    const addr = base + off;
    let bytesHtml = '';
    let asciiHtml = '';
    for (let b = 0; b < 16; b++) {
      if (off + b >= data.length) { bytesHtml += '<span class="mem-byte">  </span>'; continue; }
      const byte = data[off + b];
      bytesHtml += `<span class="mem-byte">${byte.toString(16).padStart(2,'0')}</span>`;
      asciiHtml += `<span class="mem-ascii-ch">${(byte >= 32 && byte < 127) ? String.fromCharCode(byte) : '.'}</span>`;
      if (b === 7) bytesHtml += '<span class="mem-sep">·</span>';
    }
    html += `<div class="mem-row">
      <span class="mem-addr">0x${(addr >>> 0).toString(16).padStart(8,'0')}</span>
      <span class="mem-bytes">${bytesHtml}</span>
      <span class="mem-ascii">${asciiHtml}</span>
    </div>`;
  }
  body.innerHTML = html || '<div class="placeholder">Empty region.</div>';
}

function setButtonsEnabled(on) {
  ['btn-run','btn-pause','btn-step','btn-reset'].forEach(id => {
    document.getElementById(id).disabled = !on;
  });
}

async function startRun() {
  if (!programLoaded || isRunning) return;
  if (state && (state.state === 'HALTED' || state.state === 'ERROR')) return;
  isRunning = true;
  mipsCounter = 0;
  mipsLastTime = performance.now();
  runLoop();
}

async function runLoop() {
  if (!isRunning) return;
  const speed = parseInt(document.getElementById('speed-select').value);
  const res = await cmd('run ' + speed);
  if (!res || res.error) { stopLoop(); return; }
  mipsCounter += speed;
  applyState(res);
  updateDisasmHighlights();

  const now = performance.now();
  if (now - mipsLastTime >= 500) {
    const mips = (mipsCounter / 1e6) / ((now - mipsLastTime) / 1000);
    document.getElementById('mips-display').textContent = mips.toFixed(2) + ' MIPS';
    mipsCounter = 0;
    mipsLastTime = now;
  }

  if (res.state === 'RUNNING') {
    runLoopId = setTimeout(runLoop, 0);
  } else {
    stopLoop();
    await fetchMemory(memViewRegion);
    renderMemory();
  }
}

function stopLoop() {
  isRunning = false;
  if (runLoopId) { clearTimeout(runLoopId); runLoopId = null; }
}

document.getElementById('btn-run').onclick = () => startRun();

document.getElementById('btn-pause').onclick = async () => {
  stopLoop();
  const res = await cmd('pause');
  if (res && !res.error) applyState(res);
  updateDisasmHighlights(true);
  await fetchMemory(memViewRegion);
  renderMemory();
};

document.getElementById('btn-step').onclick = async () => {
  if (!programLoaded || isRunning) return;
  const res = await cmd('step');
  if (res && !res.error) applyState(res);
  updateDisasmHighlights(true);
};

document.getElementById('btn-reset').onclick = async () => {
  stopLoop();
  await window.api.restart();
  programLoaded = false;
  state = null;
  disasmRows = [];
  memData = { rom: null, ram: null };
  uartContent = '';
  breakpoints.clear();
  gpioInputBits = [0, 0];
  prevRegs = new Array(32).fill(0);
  document.getElementById('disasm-body').innerHTML = '<div class="placeholder">Load a program to see disassembly.</div>';
  document.getElementById('regs-body').innerHTML = '<div class="placeholder">No program loaded.</div>';
  document.getElementById('mem-body').innerHTML = '<div class="placeholder">No program loaded.</div>';
  document.getElementById('uart-console').textContent = '';
  document.getElementById('gpio-porta-leds').innerHTML = '';
  document.getElementById('gpio-portb-leds').innerHTML = '';
  document.getElementById('gpio-input-pins').innerHTML = '';
  document.getElementById('gpio-regs-display').innerHTML = '';
  document.getElementById('timer-bar').style.width = '0%';
  document.getElementById('timer-cnt-label').textContent = 'CNT: 0';
  document.getElementById('timer-load-label').textContent = 'LOAD: 0';
  document.getElementById('state-pill').textContent = 'IDLE';
  document.getElementById('state-pill').className = 'status-pill idle';
  document.getElementById('cycle-display').textContent = 'Cycle: 0';
  document.getElementById('mips-display').textContent = '0.00 MIPS';
  setButtonsEnabled(false);
};

document.getElementById('btn-load').onclick = () => {
  document.getElementById('load-overlay').style.display = 'flex';
};

document.getElementById('btn-close-overlay').onclick = () => {
  document.getElementById('load-overlay').style.display = 'none';
};

document.getElementById('btn-file-pick').onclick = async () => {
  const filePath = await window.api.openBinFile();
  if (filePath) await loadProgram(filePath);
};

document.getElementById('btn-clear-uart').onclick = () => {
  uartContent = '';
  document.getElementById('uart-console').textContent = '';
};

document.getElementById('reg-fmt').onchange = (e) => {
  regFmt = e.target.value;
  renderRegs();
};

document.getElementById('mem-region-sel').onchange = async (e) => {
  memViewRegion = e.target.value;
  memViewBase = memViewRegion === 'ram' ? 0x20000000 : 0;
  if (!memData[memViewRegion] && programLoaded) await fetchMemory(memViewRegion);
  renderMemory();
};

document.getElementById('btn-mem-goto').onclick = () => {
  const v = parseInt(document.getElementById('mem-goto').value, 16);
  if (!isNaN(v)) { memViewBase = v; renderMemory(); }
};

document.getElementById('mem-goto').onkeydown = (e) => {
  if (e.key === 'Enter') document.getElementById('btn-mem-goto').click();
};

window.addEventListener('keydown', e => {
  if (e.target.tagName === 'INPUT') return;
  if (e.key === 'F5')  { e.preventDefault(); document.getElementById('btn-run').click(); }
  if (e.key === 'F10') { e.preventDefault(); document.getElementById('btn-step').click(); }
  if (e.key === ' ')   { e.preventDefault(); isRunning ? document.getElementById('btn-pause').click() : document.getElementById('btn-run').click(); }
  if (e.ctrlKey && e.key === 'r') { e.preventDefault(); document.getElementById('btn-reset').click(); }
  if (e.ctrlKey && e.key === 'o') { e.preventDefault(); document.getElementById('btn-load').click(); }
});

async function init() {
  const res = await window.api.init();
  if (!res.cliExists) {
    let banner = document.getElementById('no-cli-banner');
    if (!banner) {
      banner = document.createElement('div');
      banner.id = 'no-cli-banner';
      document.body.insertBefore(banner, document.getElementById('toolbar').nextSibling);
    }
    banner.style.display = 'block';
    banner.textContent = '⚠ CLI binary not found. Build it first: cmake -S . -B build && cmake --build build';
  }

  const programs = await window.api.listPrograms();
  const list = document.getElementById('demo-list');
  list.innerHTML = '';
  if (programs.length === 0) {
    list.innerHTML = '<li style="color:var(--subtext);cursor:default">No .bin files found in programs/</li>';
  } else {
    for (const prog of programs) {
      const li = document.createElement('li');
      li.innerHTML = `<b>${prog.name}</b>`;
      li.onclick = () => loadProgram(prog.path);
      list.appendChild(li);
    }
  }

  document.getElementById('load-overlay').style.display = 'flex';
}

init();
