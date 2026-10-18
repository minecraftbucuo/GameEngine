'use strict';

// 面板前端：2Hz 轮询 /api/snapshot，渲染对象表。
// 无第三方依赖——局域网里可能没有外网，CDN 不可靠。

const POLL_MS = 500;
const TOKEN_KEY = 'ge-panel-token';

const el = (id) => document.getElementById(id);

const state = {
  token: sessionStorage.getItem(TOKEN_KEY) || '',
  timer: null,
  inFlight: false,
  paused: false,
  lastSnapshot: null,
  prevPositions: new Map(),
  filterText: '',
  onlySuspect: false,
  view: 'map',        // 'map' | 'table'
};

function escapeHtml(value) {
  return String(value).replace(/[&<>"']/g, (c) => ({
    '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;',
  }[c]));
}

function num(value, digits = 1) {
  if (value === null || value === undefined) return '—';
  if (typeof value !== 'number') return escapeHtml(value);
  if (!Number.isFinite(value)) return 'NaN';
  return value.toFixed(digits);
}

async function api(path) {
  const res = await fetch(path, { headers: { 'X-Panel-Token': state.token } });
  if (res.status === 401) {
    const err = new Error('unauthorized');
    err.code = 401;
    throw err;
  }
  if (!res.ok) {
    const err = new Error('HTTP ' + res.status);
    err.code = res.status;
    throw err;
  }
  return res.json();
}

// ── 视图切换 ────────────────────────────────────────────────────────

function showAuth(message) {
  el('auth').classList.remove('hidden');
  el('app').classList.add('hidden');
  el('auth-error').textContent = message || '';
  el('token-input').focus();
}

function showApp() {
  el('auth').classList.add('hidden');
  el('app').classList.remove('hidden');
  // 画布在 app 隐藏时尺寸为 0，显示出来必须重新适配一次
  if (mapState.canvas === null) mapInit();
  else mapOnShow();
}

// ── 视图切换 ────────────────────────────────────────────────────────

function switchView(view) {
  state.view = view;

  el('tab-map').classList.toggle('active', view === 'map');
  el('tab-table').classList.toggle('active', view === 'table');
  el('view-map').classList.toggle('hidden', view !== 'map');
  el('view-table').classList.toggle('hidden', view !== 'table');
  el('map-toolbar').classList.toggle('hidden', view !== 'map');
  el('table-toolbar').classList.toggle('hidden', view !== 'table');

  if (view === 'map') {
    mapOnShow();
  } else if (state.lastSnapshot) {
    renderTable(state.lastSnapshot);
  }
}

function setConn(text, cls) {
  const node = el('conn');
  node.textContent = text;
  node.className = 'conn ' + (cls || '');
}

// ── 轮询 ────────────────────────────────────────────────────────────

async function tick() {
  if (state.paused || state.inFlight) return;
  state.inFlight = true;
  try {
    const snap = await api('/api/snapshot');
    state.lastSnapshot = snap;
    setConn('已连接', 'ok');
    render(snap);
  } catch (err) {
    if (err.code === 401) {
      sessionStorage.removeItem(TOKEN_KEY);
      state.token = '';
      stopPolling();
      showAuth('token 无效，请重新输入');
      return;
    }
    if (err.code === 503) {
      setConn('快照未就绪', 'warn');
      return;
    }
    setConn('连接断开', 'bad');
  } finally {
    state.inFlight = false;
  }
}

function startPolling() {
  stopPolling();
  tick();
  state.timer = setInterval(tick, POLL_MS);
}

function stopPolling() {
  if (state.timer !== null) clearInterval(state.timer);
  state.timer = null;
}

// ── 可疑判定 ────────────────────────────────────────────────────────

// 「速度不为 0 但位置没变」——两次快照之间（500ms）理论上一定有位移。
// 这是排查"敌人卡住不动"最直接的信号。
function computeStuck(obj) {
  const prev = state.prevPositions.get(obj.id);
  state.prevPositions.set(obj.id, { x: obj.x, y: obj.y });
  if (!prev) return false;

  const moved = Math.abs(obj.x - prev.x) > 0.01 || Math.abs(obj.y - prev.y) > 0.01;
  const wantsMove = Math.abs(obj.vx) > 1 || Math.abs(obj.vy) > 1;
  return wantsMove && !moved;
}

function isSuspect(obj) {
  return !obj.started || !obj.active || !obj.finite || obj.destroyed || obj._stuck === true;
}

// ── 渲染 ────────────────────────────────────────────────────────────

function render(snap) {
  // 位移状态要先算：统计卡片和表格都依赖 _stuck
  for (const obj of snap.objects) {
    obj._stuck = computeStuck(obj);
  }

  const meta = snap.meta;
  el('meta').textContent =
    `seq ${meta.seq} · 帧 ${meta.frame} · 运行 ${meta.uptimeSec.toFixed(0)}s · 构建 ${meta.buildCostUs}µs`;

  renderCards(snap);
  renderClients(snap);

  if (state.view === 'map') {
    mapRender(snap);
  } else {
    renderTable(snap);
  }
}

function renderCards(snap) {
  const objs = snap.objects;
  const counts = new Map();
  let suspect = 0;
  for (const obj of objs) {
    counts.set(obj.class, (counts.get(obj.class) || 0) + 1);
    if (isSuspect(obj)) suspect++;
  }

  const dist = [...counts.entries()]
    .sort((a, b) => b[1] - a[1])
    .map(([name, n]) => `${escapeHtml(name)} ${n}`)
    .join(' · ');

  // 物理统计：物理子系统未接入时 bodyCount 恒为 0，此时不要拿 0 去误导人
  const phys = snap.physics || { enabled: false };
  const physCard = phys.enabled
    ? {
      label: '物理',
      value: `${phys.bodyCount} body`,
      cls: phys.nonFiniteBodies > 0 ? 'bad' : '',
    }
    : { label: '物理', value: '未启用', cls: '' };

  const cards = [
    { label: '对象总数', value: objs.length, cls: '' },
    { label: '可疑', value: suspect, cls: suspect > 0 ? 'warn' : '' },
    { label: '非有限值', value: snap.meta.nonFiniteObjects, cls: snap.meta.nonFiniteObjects > 0 ? 'bad' : '' },
    { label: '平均帧耗时', value: snap.meta.avgFrameMs.toFixed(2) + ' ms', cls: '' },
    { label: '快照频率', value: snap.meta.snapshotHz + ' Hz', cls: '' },
    physCard,
    { label: '场景', value: snap.meta.scene, cls: '' },
  ];

  const html = cards.map((c) => `
    <div class="card ${c.cls}">
      <div class="card-label">${c.label}</div>
      <div class="card-value">${escapeHtml(c.value)}</div>
    </div>`).join('');

  // 物理启用后把明细补在类型分布旁边，未启用就只显示一行说明
  const physDetail = phys.enabled
    ? `静 ${phys.staticBodies} · 动 ${phys.dynamicBodies} · 运动学 ${phys.kinematicBodies}`
      + ` · 唤醒 ${phys.awakeBodies} · 睡眠 ${phys.sleepingBodies}`
      + ` · 接触 ${phys.touchingContacts}/${phys.contactCount}`
      + (phys.nonFiniteBodies > 0 ? ` · 坐标非有限 ${phys.nonFiniteBodies}` : '')
    : '场景 usePhysics=false，未创建 b2World（统计全 0 是真实状态）';

  el('cards').innerHTML =
    html + `<div class="card wide"><div class="card-label">类型分布</div>
            <div class="card-value small">${dist || '—'}</div></div>
            <div class="card wide"><div class="card-label">物理明细</div>
            <div class="card-value small">${escapeHtml(physDetail)}</div></div>`;
}

// ── 客户端列表 ──────────────────────────────────────────────────────

function clientRowHtml(c) {
  // 超过 2s 没收到包就标黄：TCP 还连着但对方可能已经卡死
  const stale = c.lastRecvAgeMs !== null && c.lastRecvAgeMs > 2000;
  const rowCls = !c.connected ? 'row-inactive' : (stale ? 'row-stuck' : '');

  const status = [];
  status.push(c.connected ? '已连接' : '已断开');
  status.push(c.verified ? '已验证' : '未验证');

  // 刚收过包时显示毫秒，避免「0.0s 前」这种看不出差别的写法
  const age = c.lastRecvAgeMs === null
    ? '从未'
    : (c.lastRecvAgeMs < 1000 ? `${c.lastRecvAgeMs}ms 前` : `${(c.lastRecvAgeMs / 1000).toFixed(1)}s 前`);

  return `<tr class="${rowCls}">
    <td class="mono">${escapeHtml(c.address)}</td>
    <td class="state">${status.join(' · ')}</td>
    <td class="num">${c.playerId === null ? '—' : c.playerId}</td>
    <td class="mono cls">${escapeHtml(c.playerClass || '—')}</td>
    <td class="num">${age}</td>
    <td class="num">${c.packetsRecv}</td>
    <td class="num">${c.bytesRecv}</td>
  </tr>`;
}

function renderClients(snap) {
  const clients = snap.clients || { count: 0, unverified: 0, list: [] };
  const countNode = el('clients-count');
  const table = el('clients-table');

  if (clients.count === 0) {
    countNode.textContent = '无连接';
    table.classList.add('hidden');
    return;
  }

  const unverified = clients.unverified > 0 ? `（${clients.unverified} 个未通过验证）` : '';
  countNode.textContent = `${clients.count} 个${unverified}`;
  table.classList.remove('hidden');
  el('clients-tbody').innerHTML = clients.list.map(clientRowHtml).join('');
}

function compsHtml(c) {
  const parts = [];
  if (c.Move) parts.push('<span class="chip">Move</span>');
  if (c.Gravity) parts.push('<span class="chip">Gravity</span>');
  if (c.HealthBar) parts.push('<span class="chip">HP</span>');
  if (c.Collision) {
    const label = c.Collision === 'BoxCollision' ? 'Box'
      : c.Collision === 'CircleCollision' ? 'Circle'
        : c.Collision;
    parts.push(`<span class="chip col">${escapeHtml(label)}</span>`);
  }
  return parts.length > 0 ? parts.join('') : '<span class="muted">无</span>';
}

// 类型私有状态。这些字段在 API 里是按类型裁剪过的，不能假定同时存在。
function typeHtml(t) {
  if (!t) return '<span class="muted">—</span>';

  const parts = [];
  if (t.kind === 'Mario') {
    const hpCls = t.health <= 1 ? 'chip chip-bad' : 'chip';
    parts.push(`<span class="${hpCls}">HP ${t.health}</span>`);
    parts.push(`<span class="chip">${t.isBig ? '大' : '小'}</span>`);
    if (!t.isPlayer) parts.push('<span class="chip">NPC</span>');
    if (t.transforming) parts.push('<span class="chip chip-warn">变身中</span>');
  } else if (t.kind === 'Goomba') {
    if (t.squashed) parts.push('<span class="chip chip-warn">被踩扁</span>');
    if (t.killedByFireball) parts.push('<span class="chip chip-bad">炮弹击毙</span>');
    if (!t.squashed && !t.killedByFireball) parts.push('<span class="chip">活着</span>');
  } else if (t.kind === 'Koopa') {
    const label = { Walking: '行走', ShellIdle: '静止壳', ShellMoving: '滑行壳' }[t.state] || t.state;
    parts.push(`<span class="chip ${t.shellMoving ? 'chip-warn' : ''}">${escapeHtml(label)}</span>`);
    if (t.killed) parts.push('<span class="chip chip-bad">击毙</span>');
  } else {
    parts.push(`<span class="chip">${escapeHtml(t.kind)}</span>`);
  }
  return parts.join('');
}

function rowHtml(obj) {
  const rowCls = [];
  if (!obj.active) rowCls.push('row-inactive');
  if (!obj.started) rowCls.push('row-nostart');
  if (!obj.finite) rowCls.push('row-nonfinite');
  if (obj.destroyed) rowCls.push('row-destroyed');
  if (obj._stuck) rowCls.push('row-stuck');

  const flags = [];
  if (obj.destroyed) flags.push('已销毁');
  if (!obj.started) flags.push('未start');
  if (!obj.active) flags.push('inactive');
  if (obj._stuck) flags.push('卡住');
  if (!obj.finite) flags.push('NaN');
  const stateText = flags.length > 0 ? flags.join(' ') : 'ok';

  return `<tr class="${rowCls.join(' ')}">
    <td class="mono">${obj.id}</td>
    <td class="mono cls">${escapeHtml(obj.class)}</td>
    <td class="tag">${escapeHtml(obj.tag)}</td>
    <td class="state">${stateText}</td>
    <td class="type">${typeHtml(obj.typeData)}</td>
    <td class="num">${num(obj.x)}</td>
    <td class="num">${num(obj.y)}</td>
    <td class="num">${num(obj.vx)}</td>
    <td class="num">${num(obj.vy)}</td>
    <td class="comps">${compsHtml(obj.components)}</td>
    <td class="num">${obj.networkId === null ? '—' : obj.networkId}</td>
  </tr>`;
}

function renderTable(snap) {
  const q = state.filterText.trim().toLowerCase();
  const rows = [];
  let visible = 0;

  for (const obj of snap.objects) {
    if (state.onlySuspect && !isSuspect(obj)) continue;
    if (q !== '') {
      const hay = `${obj.id} ${obj.class} ${obj.tag}`.toLowerCase();
      if (!hay.includes(q)) continue;
    }
    visible++;
    rows.push(rowHtml(obj));
  }

  el('visible-count').textContent = `${visible} / ${snap.objects.length}`;
  el('tbody').innerHTML = rows.join('');
  el('empty').classList.toggle('hidden', visible > 0);
}

// ── 事件 ────────────────────────────────────────────────────────────

el('auth-form').addEventListener('submit', (event) => {
  event.preventDefault();
  const token = el('token-input').value.trim();
  if (token === '') return;

  state.token = token;
  sessionStorage.setItem(TOKEN_KEY, token);
  showApp();
  startPolling();
});

el('btn-pause').addEventListener('click', () => {
  state.paused = !state.paused;
  el('btn-pause').textContent = state.paused ? '继续' : '暂停';
  el('btn-pause').classList.toggle('active', state.paused);
});

el('btn-logout').addEventListener('click', () => {
  sessionStorage.removeItem(TOKEN_KEY);
  state.token = '';
  stopPolling();
  showAuth('');
});

el('filter-text').addEventListener('input', (event) => {
  state.filterText = event.target.value;
  if (state.lastSnapshot) renderTable(state.lastSnapshot);
});

el('filter-suspect').addEventListener('change', (event) => {
  state.onlySuspect = event.target.checked;
  if (state.lastSnapshot) renderTable(state.lastSnapshot);
});

// ── 标签页与地图控件 ────────────────────────────────────────────────

el('tab-map').addEventListener('click', () => switchView('map'));
el('tab-table').addEventListener('click', () => switchView('table'));

el('ov-velocity').addEventListener('change', (e) => mapSetOverlay('velocity', e.target.checked));
el('ov-boxes').addEventListener('change', (e) => mapSetOverlay('boxes', e.target.checked));
el('ov-grid').addEventListener('change', (e) => mapSetOverlay('grid', e.target.checked));
el('ov-centers').addEventListener('change', (e) => mapSetOverlay('centers', e.target.checked));

el('btn-fit').addEventListener('click', () => mapRefit());
el('btn-fit-all').addEventListener('click', () => mapFitAll());

// 切到后台时暂停轮询，避免多标签页把线程池占满
document.addEventListener('visibilitychange', () => {
  if (document.hidden) {
    stopPolling();
  } else if (state.token !== '' && !state.paused) {
    startPolling();
  }
});

// ── 启动 ────────────────────────────────────────────────────────────

if (state.token !== '') {
  showApp();
  startPolling();
} else {
  showAuth('');
}
