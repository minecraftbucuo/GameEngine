'use strict';

// 面板前端：轮询 /api/snapshot，渲染地图与表格。
// 无第三方依赖——局域网里可能没有外网，CDN 不可靠。
//
// 轮询间隔是自适应的：跟着服务端 meta.snapshotHz 走。
// 比它快只会拿到重复快照（靠 seq 去重），比它慢地图就会一卡一卡。

const POLL_MIN_MS = 80;
const POLL_MAX_MS = 500;
const TOKEN_KEY = 'ge-panel-token';

const el = (id) => document.getElementById(id);

const state = {
  token: sessionStorage.getItem(TOKEN_KEY) || '',
  timer: null,
  inFlight: false,
  paused: false,
  lastSnapshot: null,
  lastSeq: null,          // 用于丢弃重复快照，避免误判「卡住」
  prevPositions: new Map(),
  objectFilter: 'all',    // 对象快速筛选按钮：all / moving / stuck / abnormal / enemy / mario
  clientFilter: 'all',    // 客户端快速筛选按钮：all / connected / disconnected / unverified / stale
  view: 'map',          // 'map' | 'table'
  subView: 'objects',   // 表格子页：'objects' | 'clients'
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

  if (view === 'map') {
    mapOnShow();
  } else {
    mapStopLoop();   // 切走就别再占着一帧一次的 rAF
    if (state.lastSnapshot) {
      // 恢复到上次的子页，两个表都刷一遍成本低
      renderTable(state.lastSnapshot);
      renderClients(state.lastSnapshot);
    }
  }
}

function setConn(text, cls) {
  const node = el('conn');
  node.textContent = text;
  node.className = 'conn ' + (cls || '');
}

// ── 轮询 ────────────────────────────────────────────────────────────

// 跟着服务端实际采样频率走。服务端被自动降级过的话 meta.snapshotHz 会变小，
// 这里也跟着放慢，不会做无用请求。
function pollInterval() {
  const hz = state.lastSnapshot && state.lastSnapshot.meta
    ? state.lastSnapshot.meta.snapshotHz
    : 0;
  if (!hz || hz < 1) return POLL_MAX_MS;
  return Math.min(POLL_MAX_MS, Math.max(POLL_MIN_MS, Math.round(1000 / hz)));
}

async function tick() {
  if (state.paused) return;

  state.inFlight = true;
  try {
    const snap = await api('/api/snapshot');

    // 轮询可能比服务端采样快，同一份快照会重复到达。
    // 重复帧既没必要重绘，更关键的是会让「卡住」判定误报
    // （两次相同位置 + 速度非零 = 看起来没动）。
    if (state.lastSeq === snap.meta.seq) {
      setConn('已连接', 'ok');
      return;
    }

    state.lastSeq = snap.meta.seq;
    state.lastSnapshot = snap;
    setConn('已连接', 'ok');
    render(snap);
  } catch (err) {
    if (err.code === 401) {
      sessionStorage.removeItem(TOKEN_KEY);
      state.token = '';
      state.lastSeq = null;
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
    scheduleNext();
  }
}

function scheduleNext() {
  if (state.timer !== null) clearTimeout(state.timer);
  state.timer = state.paused ? null : setTimeout(tick, pollInterval());
}

function startPolling() {
  stopPolling();
  tick();
}

function stopPolling() {
  if (state.timer !== null) clearTimeout(state.timer);
  state.timer = null;
}

// ── 可疑判定 ────────────────────────────────────────────────────────

// 「速度不为 0 但位置没变」——两次不同快照之间理论上一定有位移。
// 这是排查"敌人卡住不动"最直接的信号。
// 注意：只在 seq 变化时调用，重复快照会误报（见 tick 里的去重）。
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
  // 数字全部补零到固定位数：位数变化会让字符串忽宽忽窄，
  // 右上角这一行就会左右跳。配合 tabular-nums 字符数恒定 ⇒ 宽度恒定。
  const pad = (n, w) => String(n).padStart(w, '0');
  el('meta').textContent =
    `seq ${pad(meta.seq, 6)} · 帧 ${pad(meta.frame, 8)} · 运行 ${pad(meta.uptimeSec.toFixed(0), 4)}s · 构建 ${pad(meta.buildCostUs, 5)}µs`;

  renderCards(snap);

  if (state.view === 'map') {
    mapRender(snap);
  } else if (state.subView === 'clients') {
    renderClients(snap);
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

  const cards = [
    { label: '对象总数', value: objs.length, cls: '' },
    { label: '可疑', value: suspect, cls: suspect > 0 ? 'warn' : '' },
    { label: '非有限值', value: snap.meta.nonFiniteObjects, cls: snap.meta.nonFiniteObjects > 0 ? 'bad' : '' },
    { label: '平均帧耗时', value: snap.meta.avgFrameMs.toFixed(2) + ' ms', cls: '' },
    { label: '快照频率', value: snap.meta.snapshotHz + ' Hz', cls: '' },
  ];

  const html = cards.map((c) => `
    <div class="card ${c.cls}">
      <div class="card-label">${c.label}</div>
      <div class="card-value">${escapeHtml(c.value)}</div>
    </div>`).join('');

  el('cards').innerHTML =
    html + `<div class="card wide"><div class="card-label">类型分布</div>
            <div class="card-value small">${dist || '—'}</div></div>`;
}

// ── 客户端列表（同样走行 DOM 复用，原因同对象表） ───────────────────

const CLI_CELL_CLS = ['mono', 'state', 'num', 'mono cls', 'num', 'num', 'num'];
// 行 key 用 playerId：已验证客户端的 playerId 就是角色 networkId，天然唯一。
// 未验证客户端没有 id，退回用列表下标——那只是验证前的短暂状态。
const cliRowCache = new Map();

function cliRowKey(c, i) {
  return c.playerId != null ? 'p' + c.playerId : 'u' + i;
}

function ensureCliRow(key, c) {
  let entry = cliRowCache.get(key);
  if (entry) return entry;

  const tr = document.createElement('tr');
  const cells = CLI_CELL_CLS.map((cls) => {
    const td = document.createElement('td');
    td.className = cls;
    tr.appendChild(td);
    return td;
  });
  entry = { tr, cells };
  cliRowCache.set(key, entry);
  return entry;
}

function updateCliRow(entry, c) {
  const cc = entry.cells;
  const stale = c.lastRecvAgeMs !== null && c.lastRecvAgeMs > 2000;
  const status = [
    c.connected ? '已连接' : '已断开',
    c.verified ? '已验证' : '未验证',
  ].join(' · ');
  // 刚收过包时显示毫秒，避免「0.0s 前」这种看不出差别的写法
  const age = c.lastRecvAgeMs === null
    ? '从未'
    : (c.lastRecvAgeMs < 1000 ? `${c.lastRecvAgeMs}ms 前` : `${(c.lastRecvAgeMs / 1000).toFixed(1)}s 前`);

  setCellHtml(cc[0], escapeHtml(c.address));
  setCellText(cc[1], status);
  setCellText(cc[2], c.playerId === null ? '—' : String(c.playerId));
  setCellHtml(cc[3], escapeHtml(c.playerClass || '—'));
  setCellText(cc[4], age);
  setCellText(cc[5], String(c.packetsRecv));
  setCellText(cc[6], String(c.bytesRecv));

  const rowCls = !c.connected ? 'row-inactive' : (stale ? 'row-stuck' : '');
  if (entry.tr.className !== rowCls) entry.tr.className = rowCls;
}

const CLIENT_FILTERS = {
  all:          () => true,
  connected:    (c) => c.connected === true,
  disconnected: (c) => c.connected !== true,
  unverified:   (c) => c.verified !== true,
  stale:        (c) => c.connected === true && c.lastRecvAgeMs !== null && c.lastRecvAgeMs > 2000,
};

function renderClients(snap) {
  const clients = snap.clients || { count: 0, unverified: 0, list: [] };
  const countNode = el('clients-count');
  const table = el('clients-table');
  const empty = el('clients-empty');
  const tbody = el('clients-tbody');

  if (clients.count === 0) {
    countNode.textContent = '无连接';
    table.classList.add('hidden');
    empty.textContent = '暂无客户端连接';
    empty.classList.remove('hidden');
    for (const entry of cliRowCache.values()) entry.tr.remove();
    cliRowCache.clear();
    return;
  }

  const cPred = CLIENT_FILTERS[state.clientFilter] || CLIENT_FILTERS.all;
  const list = clients.list.filter(cPred);

  const unverified = clients.unverified > 0 ? `（${clients.unverified} 个未通过验证）` : '';
  countNode.textContent = `${clients.count} 个${unverified}`;

  if (list.length === 0) {
    table.classList.add('hidden');
    empty.textContent = '没有匹配的客户端';
    empty.classList.remove('hidden');
    return;
  }

  table.classList.remove('hidden');
  empty.classList.add('hidden');
  const seen = new Set();
  for (let i = 0; i < list.length; i++) {
    const c = list[i];
    const key = cliRowKey(c, i);
    seen.add(key);
    const entry = ensureCliRow(key, c);
    updateCliRow(entry, c);
    tbody.appendChild(entry.tr);
  }
  for (const [key, entry] of cliRowCache) {
    if (!seen.has(key)) {
      entry.tr.remove();
      cliRowCache.delete(key);
    }
  }
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

// ── 表格渲染：行 DOM 复用 ───────────────────────────────────────────
// 之前每 100ms 用 innerHTML 全量重建 tbody，两个恶果：
//   1. 重建后 :hover 状态丢失 → 鼠标放上去闪来闪去
//   2. 两次点击之间元素被替换，click/dblclick 派发不出来 → 双击没反应
// 现在行节点常驻（按 id 缓存），只更新单元格内容，DOM 稳定后
// 原生 dblclick 和 hover 都正常。

function setCellText(td, text) {
  if (td._text !== text) { td.textContent = text; td._text = text; }
}

function setCellHtml(td, html) {
  if (td._html !== html) { td.innerHTML = html; td._html = html; }
}

const OBJ_CELL_CLS = ['mono', 'mono cls', 'tag', 'state', 'type', 'num', 'num', 'num', 'num', 'comps', 'num'];
const objRowCache = new Map();   // id -> { tr, cells }

function ensureObjRow(obj) {
  let entry = objRowCache.get(obj.id);
  if (entry) return entry;

  const tr = document.createElement('tr');
  const cells = OBJ_CELL_CLS.map((cls) => {
    const td = document.createElement('td');
    td.className = cls;
    tr.appendChild(td);
    return td;
  });
  entry = { tr, cells };
  objRowCache.set(obj.id, entry);
  return entry;
}

function updateObjRow(entry, obj) {
  const c = entry.cells;

  const flags = [];
  if (obj.destroyed) flags.push('已销毁');
  if (!obj.started) flags.push('未start');
  if (!obj.active) flags.push('inactive');
  if (obj._stuck) flags.push('卡住');
  if (!obj.finite) flags.push('NaN');
  const stateText = flags.length > 0 ? flags.join(' ') : 'ok';

  setCellText(c[0], String(obj.id));
  setCellHtml(c[1], escapeHtml(obj.class));
  setCellHtml(c[2], escapeHtml(obj.tag));
  setCellText(c[3], stateText);
  setCellHtml(c[4], typeHtml(obj.typeData));
  setCellText(c[5], num(obj.x));
  setCellText(c[6], num(obj.y));
  setCellText(c[7], num(obj.vx));
  setCellText(c[8], num(obj.vy));
  setCellHtml(c[9], compsHtml(obj.components));
  setCellText(c[10], obj.networkId === null ? '—' : String(obj.networkId));

  const rowCls = [];
  if (!obj.active) rowCls.push('row-inactive');
  if (!obj.started) rowCls.push('row-nostart');
  if (!obj.finite) rowCls.push('row-nonfinite');
  if (obj.destroyed) rowCls.push('row-destroyed');
  if (obj._stuck) rowCls.push('row-stuck');
  const cls = rowCls.join(' ');
  if (entry.tr.className !== cls) entry.tr.className = cls;
}

// 快速筛选按钮的谓词。isMoving 看「想要动」（速度非零），比看位移更准——
// 刚重生还没走第一步的对象也算移动中。
const OBJECT_FILTERS = {
  all:      () => true,
  moving:   (o) => Math.abs(o.vx) > 1 || Math.abs(o.vy) > 1,
  stuck:    (o) => o._stuck === true,
  abnormal: (o) => !o.started || !o.active || !o.finite || o.destroyed,
  enemy:    (o) => o.class === 'Goomba' || o.class === 'Koopa' || o.class === 'Bowser',
  mario:    (o) => o.class === 'Mario',
};

function renderTable(snap) {
  const pred = OBJECT_FILTERS[state.objectFilter] || OBJECT_FILTERS.all;
  const tbody = el('tbody');
  let visible = 0;
  const seen = new Set();

  for (const obj of snap.objects) {
    if (!pred(obj)) continue;
    visible++;
    seen.add(obj.id);
    const entry = ensureObjRow(obj);
    updateObjRow(entry, obj);
    tbody.appendChild(entry.tr);   // 已在 tbody 里就只是挪到正确顺序，成本低
  }

  // 被筛掉/已销毁的行摘掉
  for (const [id, entry] of objRowCache) {
    if (!seen.has(id)) {
      entry.tr.remove();
      objRowCache.delete(id);
    }
  }

  el('visible-count').textContent = `${visible} / ${snap.objects.length}`;
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

  if (state.paused) {
    // 冻结在当前插值时刻，否则暂停后画面还会自己往前滑一小段
    stopPolling();
    mapFreeze();
  } else {
    mapUnfreeze();
    startPolling();
  }
});

el('btn-logout').addEventListener('click', () => {
  sessionStorage.removeItem(TOKEN_KEY);
  state.token = '';
  stopPolling();
  showAuth('');
});

// 对象快速筛选按钮组（单选）
document.querySelectorAll('#sub-objects .filter-btn').forEach((btn) => {
  btn.addEventListener('click', () => {
    state.objectFilter = btn.dataset.filter;
    document.querySelectorAll('#sub-objects .filter-btn').forEach((b) => b.classList.toggle('active', b === btn));
    if (state.lastSnapshot) renderTable(state.lastSnapshot);
  });
});

// 客户端快速筛选按钮组（单选）
document.querySelectorAll('#sub-clients .filter-btn').forEach((btn) => {
  btn.addEventListener('click', () => {
    state.clientFilter = btn.dataset.cfilter;
    document.querySelectorAll('#sub-clients .filter-btn').forEach((b) => b.classList.toggle('active', b === btn));
    if (state.lastSnapshot) renderClients(state.lastSnapshot);
  });
});

// 表格子页切换：对象 / 客户端
function switchSubView(sub) {
  state.subView = sub;
  el('subtab-objects').classList.toggle('active', sub === 'objects');
  el('subtab-clients').classList.toggle('active', sub === 'clients');
  el('sub-objects').classList.toggle('hidden', sub !== 'objects');
  el('sub-clients').classList.toggle('hidden', sub !== 'clients');
  if (state.lastSnapshot) {
    if (sub === 'objects') renderTable(state.lastSnapshot);
    else renderClients(state.lastSnapshot);
  }
}
el('subtab-objects').addEventListener('click', () => switchSubView('objects'));
el('subtab-clients').addEventListener('click', () => switchSubView('clients'));

// 表格行双击 → 跳到地图并跟随该对象（再点地图空白处解除）。
// 行 DOM 已常驻，原生 dblclick 可以直接用。
el('tbody').addEventListener('dblclick', (event) => {
  const tr = event.target.closest('tr');
  if (!tr) return;
  const id = Number(tr.querySelector('td')?.textContent);
  if (!Number.isFinite(id)) return;
  switchView('map');
  mapFollowById(id);
});

// 客户端表：双击跟随该客户端的角色。playerId 就是角色对象的 networkId，
// 用它反查地图对象；角色不在场景里（未进入/已断开）就不跳转。
el('clients-tbody').addEventListener('dblclick', (event) => {
  const tr = event.target.closest('tr');
  if (!tr || !state.lastSnapshot) return;
  const pid = Number(tr.querySelectorAll('td')[2]?.textContent);
  if (!Number.isFinite(pid)) return;
  const obj = state.lastSnapshot.objects.find((o) => o.networkId === pid);
  if (!obj) return;
  switchView('map');
  mapFollowById(obj.id);
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
