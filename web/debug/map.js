'use strict';

// 地图视图：把快照里的对象按世界坐标画到 canvas 上。
// 横版游戏，x 向右、y 向下，与快照坐标一一对应，不做翻转。
//
// 与 app.js 的分工：
//   app.js    —— 取数据、判定可疑、渲染表格
//   map.js    —— 只负责把一份快照画出来，不碰网络

const MAP_COLORS = {
  Ground: '#2d333b',
  Brick: '#8a5a2b',
  Box: '#b08800',
  Goomba: '#c4713a',
  Koopa: '#3fb950',
  Mario: '#f85149',
  Bowser: '#d29922',
  FireBall: '#ff7b3d',
  BowserFire: '#ff7b3d',
  Mushroom: '#e5534b',
};
const MAP_DEFAULT_COLOR = '#58a6ff';

// 静态物体先画，动态物体压在上面，否则砖块会盖住马里奥
const MAP_DRAW_ORDER = ['Ground', 'Brick', 'Box', 'Bowser', 'Goomba', 'Koopa', 'Mushroom', 'FireBall', 'BowserFire', 'Mario'];

const mapState = {
  canvas: null,
  ctx: null,
  // 世界坐标 → 屏幕：sx = (wx - offsetX) * scale
  scale: 1,
  offsetX: 0,
  offsetY: 0,
  fitPending: true,
  fitMode: 'cover',
  followId: null,
  hover: null,
  drag: null,
  dpr: 1,
  overlays: { velocity: true, boxes: true, grid: true, centers: false },

  // ── 平滑插值 ────────────────────────────────────────────────────
  // 服务端 10Hz 采样、前端按同样节奏轮询，直接画就是 10fps 的跳帧。
  // 保留最近两份快照，按时间在两份之间插值，再用 rAF 以 60fps 重绘。
  frames: [],          // [{ snap, at }]，最多两份，at = 到达时刻(performance.now)
  lastSeq: null,       // 去重：轮询快于采样时同一份快照会重复到达
  renderObjects: null, // 本帧实际画出来的对象（插值后的副本），命中测试用它
  rafId: null,
  frozenAt: null,      // 暂停时冻结的渲染时刻

  last: null,
  now: 0,
};

function mapEl(id) {
  return document.getElementById(id);
}

function mapColor(cls) {
  return MAP_COLORS[cls] || MAP_DEFAULT_COLOR;
}

function drawOrderIndex(cls) {
  const i = MAP_DRAW_ORDER.indexOf(cls);
  return i < 0 ? MAP_DRAW_ORDER.length : i;
}

// ── 坐标变换 ────────────────────────────────────────────────────────

function mapW2SX(wx) { return (wx - mapState.offsetX) * mapState.scale; }
function mapW2SY(wy) { return (wy - mapState.offsetY) * mapState.scale; }
function mapS2WX(sx) { return sx / mapState.scale + mapState.offsetX; }
function mapS2WY(sy) { return sy / mapState.scale + mapState.offsetY; }

// 所有对象的包围盒（含宽高）。空场景给一个兜底范围，避免除零。
function mapBounds(objects) {
  if (objects.length === 0) return { x: 0, y: 0, w: 1000, h: 600 };

  let minX = Infinity;
  let minY = Infinity;
  let maxX = -Infinity;
  let maxY = -Infinity;

  for (const o of objects) {
    if (!Number.isFinite(o.x) || !Number.isFinite(o.y)) continue;
    const w = Number.isFinite(o.w) ? o.w : 0;
    const h = Number.isFinite(o.h) ? o.h : 0;
    minX = Math.min(minX, o.x);
    minY = Math.min(minY, o.y);
    maxX = Math.max(maxX, o.x + w);
    maxY = Math.max(maxY, o.y + h);
  }

  // 全是 NaN 对象时同样兜底
  if (!Number.isFinite(minX) || !Number.isFinite(maxX)) return { x: 0, y: 0, w: 1000, h: 600 };
  if (maxX - minX < 1) maxX = minX + 1;
  if (maxY - minY < 1) maxY = minY + 1;

  return { x: minX, y: minY, w: maxX - minX, h: maxY - minY };
}

// 视野适配。两种模式，用途不同：
//   cover   —— 铺满视口（宽关卡按高度算），对象看得清，横向靠平移
//   contain —— 整个关卡装进视口，能看全局但对象会小到看不清
// 横版关卡通常是十几比一的长条（本关卡 15822×960），默认用 cover 才有可读性。
function mapFit(mode, objects) {
  if (!mapState.canvas || !objects) return;
  const cw = mapState.canvas.clientWidth;
  const ch = mapState.canvas.clientHeight;
  if (cw < 10 || ch < 10) return;

  const b = mapBounds(objects);
  const pad = 24;
  const sx = (cw - pad * 2) / b.w;
  const sy = (ch - pad * 2) / b.h;
  const scale = Math.max(0.001, mode === 'contain' ? Math.min(sx, sy) : Math.max(sx, sy));

  mapState.scale = scale;

  if (mode === 'contain') {
    // 装下模式：把包围盒居中
    mapState.offsetX = b.x - (cw / scale - b.w) / 2;
    mapState.offsetY = b.y - (ch / scale - b.h) / 2;
  } else {
    // 铺满模式：有马里奥就以它为中心，否则从关卡左端开始（横向居中会落到中间段）
    const mario = objects.find((o) => o.class === 'Mario');
    const centerX = mario
      ? mario.x + mario.w / 2
      : b.x + Math.min(b.w, cw / scale) / 2;
    mapState.offsetX = centerX - cw / (2 * scale);
    mapState.offsetY = b.y + b.h / 2 - ch / (2 * scale);
  }

  mapState.fitPending = false;
}

function mapCenterOn(obj) {
  if (!mapState.canvas) return;
  const cw = mapState.canvas.clientWidth;
  const ch = mapState.canvas.clientHeight;
  mapState.offsetX = obj.x + obj.w / 2 - cw / (2 * mapState.scale);
  mapState.offsetY = obj.y + obj.h / 2 - ch / (2 * mapState.scale);
}

// ── 尺寸自适应 ──────────────────────────────────────────────────────

function mapResize() {
  const c = mapState.canvas;
  if (!c) return;

  const dpr = window.devicePixelRatio || 1;
  const w = c.clientWidth;
  const h = c.clientHeight;
  if (w < 10 || h < 10) return;

  const nextW = Math.round(w * dpr);
  const nextH = Math.round(h * dpr);
  const sizeChanged = c.width !== nextW || c.height !== nextH;

  // 旧尺寸按 CSS 像素算，用于换算「原来的视野中心」
  const prevCssW = mapState.dpr > 0 ? c.width / mapState.dpr : w;
  const prevCssH = mapState.dpr > 0 ? c.height / mapState.dpr : h;

  if (sizeChanged) {
    // 用设备像素设置后备缓冲，否则高分屏上线条发虚。
    // 只在真的变了才赋值——给 canvas.width 赋值会重置整个 context 状态。
    c.width = nextW;
    c.height = nextH;
    mapState.dpr = dpr;
  }
  mapState.ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

  if (!sizeChanged) return;

  if (mapState.last === null || mapState.fitPending) {
    mapState.fitPending = true;
    return;
  }

  // 保持视野中心不变：「客户端」区块展开/收起会改画布高度，
  // 如果这时重新 fit，用户正在看的位置会突然跳走。
  const centerWX = mapS2WX(prevCssW / 2);
  const centerWY = mapS2WY(prevCssH / 2);
  mapState.offsetX = centerWX - w / (2 * mapState.scale);
  mapState.offsetY = centerWY - h / (2 * mapState.scale);
}

// ── 绘制 ────────────────────────────────────────────────────────────

// 网格步长自适应：让屏幕上的格子在 50~140px 之间，读数才不会挤成一团
function mapGridStep() {
  const candidates = [10, 20, 50, 100, 200, 500, 1000, 2000, 5000];
  for (const step of candidates) {
    if (step * mapState.scale >= 50) return step;
  }
  return 10000;
}

function mapDrawGrid(ctx, cw, ch) {
  const step = mapGridStep();
  const w0 = mapS2WX(0);
  const w1 = mapS2WX(cw);
  const h0 = mapS2WY(0);
  const h1 = mapS2WY(ch);

  ctx.lineWidth = 1;
  ctx.strokeStyle = 'rgba(139,148,158,0.12)';
  ctx.beginPath();
  for (let x = Math.floor(w0 / step) * step; x <= w1; x += step) {
    const sx = Math.round(mapW2SX(x)) + 0.5;
    ctx.moveTo(sx, 0);
    ctx.lineTo(sx, ch);
  }
  for (let y = Math.floor(h0 / step) * step; y <= h1; y += step) {
    const sy = Math.round(mapW2SY(y)) + 0.5;
    ctx.moveTo(0, sy);
    ctx.lineTo(cw, sy);
  }
  ctx.stroke();

  // 坐标刻度：贴着上边和左边，方便对照关卡 JSON 里的数值
  ctx.fillStyle = 'rgba(139,148,158,0.65)';
  ctx.font = '10px ui-monospace, Consolas, monospace';
  ctx.textBaseline = 'top';
  ctx.textAlign = 'left';
  for (let x = Math.floor(w0 / step) * step; x <= w1; x += step) {
    const sx = mapW2SX(x);
    if (sx < 2 || sx > cw - 30) continue;
    ctx.fillText(String(Math.round(x)), sx + 3, 3);
  }
  for (let y = Math.floor(h0 / step) * step; y <= h1; y += step) {
    const sy = mapW2SY(y);
    if (sy < 14 || sy > ch - 12) continue;
    ctx.fillText(String(Math.round(y)), 3, sy + 2);
  }
}

// 异常状态的描边：和表格里的行高亮用同一套语义
function mapStrokeFor(obj, time) {
  if (!obj.finite) return { color: '#db61ff', width: 2, dash: [] };
  if (obj._stuck) {
    // 卡住的对象闪一下，静止画面里也能注意到
    const pulse = 0.55 + 0.45 * Math.abs(Math.sin(time / 320));
    return { color: `rgba(210,153,34,${pulse.toFixed(3)})`, width: 2, dash: [] };
  }
  if (!obj.started) return { color: '#d29922', width: 1.5, dash: [3, 3] };
  if (obj.destroyed) return { color: '#8b949e', width: 1, dash: [2, 4] };
  return null;
}

function mapDrawObject(ctx, obj, time) {
  if (!Number.isFinite(obj.x) || !Number.isFinite(obj.y)) return;

  const sx = mapW2SX(obj.x);
  const sy = mapW2SY(obj.y);
  const sw = Math.max(1.5, obj.w * mapState.scale);
  const sh = Math.max(1.5, obj.h * mapState.scale);

  // 视口裁剪：地图很大时别为看不见的对象浪费绘制
  if (sx + sw < -20 || sy + sh < -20 || sx > mapState.canvas.clientWidth + 20
      || sy > mapState.canvas.clientHeight + 20) return;

  let alpha = 1;
  if (!obj.active) alpha = 0.35;
  else if (obj.destroyed) alpha = 0.45;

  ctx.globalAlpha = alpha;
  ctx.fillStyle = mapColor(obj.class);
  ctx.fillRect(sx, sy, sw, sh);

  const stroke = mapStrokeFor(obj, time);
  if (stroke) {
    ctx.globalAlpha = 1;
    ctx.strokeStyle = stroke.color;
    ctx.lineWidth = stroke.width;
    ctx.setLineDash(stroke.dash);
    ctx.strokeRect(sx + 0.5, sy + 0.5, sw - 1, sh - 1);
    ctx.setLineDash([]);
  }

  // 高亮：悬停或跟随
  if (obj.id === mapState.hover || obj.id === mapState.followId) {
    ctx.globalAlpha = 1;
    ctx.strokeStyle = obj.id === mapState.followId ? '#58a6ff' : '#e6edf3';
    ctx.lineWidth = 2;
    ctx.strokeRect(sx - 1.5, sy - 1.5, sw + 3, sh + 3);
  }

  ctx.globalAlpha = 1;

  if (mapState.overlays.boxes && obj.collisionBox) {
    const g = obj.collisionBox;
    const bx = mapW2SX(g.x);
    const by = mapW2SY(g.y);
    const bw = Math.max(1, g.w * mapState.scale);
    const bh = Math.max(1, g.h * mapState.scale);

    ctx.strokeStyle = 'rgba(88,166,255,0.75)';
    ctx.lineWidth = 1;
    ctx.setLineDash([2, 2]);
    if (g.shape === 'circle') {
      // 圆按外接矩形中心 + 半径画，视觉上才是真的圆
      ctx.beginPath();
      ctx.arc(bx + bw / 2, by + bh / 2, Math.max(1, g.r * mapState.scale), 0, Math.PI * 2);
      ctx.stroke();
    } else {
      ctx.strokeRect(bx + 0.5, by + 0.5, bw, bh);
    }
    ctx.setLineDash([]);
  }

  if (mapState.overlays.centers) {
    const cx = mapW2SX(obj.cx);
    const cy = mapW2SY(obj.cy);
    ctx.fillStyle = '#e6edf3';
    ctx.fillRect(cx - 1.5, cy - 1.5, 3, 3);
  }

  if (mapState.overlays.velocity && (Math.abs(obj.vx) > 1 || Math.abs(obj.vy) > 1)) {
    const cx = mapW2SX(obj.cx);
    const cy = mapW2SY(obj.cy);
    const speed = Math.hypot(obj.vx, obj.vy);
    const ang = Math.atan2(obj.vy, obj.vx);
    // 太短会看成一道杂线，太长会横穿屏幕，两头都夹一下
    const len = Math.max(20, Math.min(110, speed * 0.3));
    const ex = cx + Math.cos(ang) * len;
    const ey = cy + Math.sin(ang) * len;
    const head = 7;

    // 先画一遍深色描边，白箭头压在浅色对象上也能看清
    for (const pass of [{ color: 'rgba(13,17,23,0.85)', width: 4.5 },
                        { color: 'rgba(240,246,252,0.95)', width: 1.8 }]) {
      ctx.strokeStyle = pass.color;
      ctx.fillStyle = pass.color;
      ctx.lineWidth = pass.width;

      ctx.beginPath();
      ctx.moveTo(cx, cy);
      ctx.lineTo(ex, ey);
      ctx.stroke();

      ctx.beginPath();
      ctx.moveTo(ex, ey);
      ctx.lineTo(ex - Math.cos(ang - 0.45) * head, ey - Math.sin(ang - 0.45) * head);
      ctx.lineTo(ex - Math.cos(ang + 0.45) * head, ey - Math.sin(ang + 0.45) * head);
      ctx.closePath();
      ctx.fill();
    }
  }
}

// 按时间把最近两份快照插值成当前该画的一帧。
//
// 关键点：渲染时刻固定落后一个采样周期。因为「最新那份」永远是 t=1，
// 只有往回退一个周期，t 才会从 0 平滑走到 1，并在下一份快照到达时无缝接上。
// 代价是画面滞后一个周期（10Hz 下 100ms，肉眼基本无感）。
//
// 不用「按速度外推」：500ms 周期下，100px/s 的敌人会外推 50px，
// 一旦它中途掉头就会明显回弹，比卡顿更难受。位置插值不会过冲。
function mapBuildRenderList() {
  const frames = mapState.frames;
  if (frames.length === 0) return null;

  const newest = frames[frames.length - 1];

  // 只有一份快照：直接显示，无从插值
  if (frames.length === 1) return newest.snap.objects;

  const older = frames[frames.length - 2];
  const span = newest.at - older.at;
  if (span <= 0) return newest.snap.objects;

  const now = mapState.frozenAt !== null ? mapState.frozenAt : performance.now();
  const renderAt = now - span;
  const t = Math.min(1, Math.max(0, (renderAt - older.at) / span));

  const prevById = new Map();
  for (const o of older.snap.objects) prevById.set(o.id, o);

  const out = [];
  for (const cur of newest.snap.objects) {
    const prev = prevById.get(cur.id);

    // 新生成的对象没有上一帧位置，或坐标本身不可用：原样画
    if (!prev || !Number.isFinite(prev.x) || !Number.isFinite(cur.x)
        || !Number.isFinite(prev.y) || !Number.isFinite(cur.y)) {
      out.push(cur);
      continue;
    }

    const dx = cur.x - prev.x;
    const dy = cur.y - prev.y;

    // 位移过大多半是重生/传送/掉落穿出场景，插值会拖出一条横穿地图的线
    if (Math.abs(dx) > 400 || Math.abs(dy) > 400) {
      out.push(cur);
      continue;
    }

    const nx = prev.x + dx * t;
    const ny = prev.y + dy * t;

    // 碰撞箱必须跟着插值后的位置走。直接 ...cur 会把最新快照的碰撞箱原样带过来，
    // 而 x/y 是插值后的 —— 移动中的对象两者会错开最多一个采样周期的位移。
    // 这里只让锚点跟随，相对偏移保留（Bowser 的 128×128 精灵对 80×100 碰撞体，
    // offset 不是 0，硬对齐反而会把它压到实体左上角）。
    let box = cur.collisionBox;
    if (box && Number.isFinite(box.x) && Number.isFinite(box.y)) {
      box = { ...box, x: nx + (box.x - cur.x), y: ny + (box.y - cur.y) };
    }

    out.push({
      ...cur,
      x: nx,
      y: ny,
      cx: prev.cx + (cur.cx - prev.cx) * t,
      cy: prev.cy + (cur.cy - prev.cy) * t,
      collisionBox: box,
    });
  }

  return out;
}

function mapDraw() {
  if (!mapState.ctx) return;

  const objects = mapBuildRenderList();
  if (!objects) return;

  // 每次重绘前都对齐后备缓冲与 CSS 尺寸。
  // 「客户端」区块展开/收起、卡片文字换行都会改变画布高度，但这些布局变化
  // **不会触发 window.resize**。不同步的话后备缓冲比 CSS 盒子大，浏览器会把
  // 整张图缩放着显示，而 clearRect 只覆盖了 CSS 盒子那么大一块 ——
  // 底部那条没被清掉，就表现为「画布下面不刷新、移动留残影」。
  mapResize();

  const ctx = mapState.ctx;
  const cw = mapState.canvas.clientWidth;
  const ch = mapState.canvas.clientHeight;
  if (cw < 10 || ch < 10) return;

  if (mapState.fitPending) mapFit(mapState.fitMode, objects);

  if (mapState.followId !== null) {
    const target = objects.find((o) => o.id === mapState.followId);
    if (target) mapCenterOn(target);
    else mapState.followId = null;   // 跟随对象没了就自动解除
  }

  mapState.renderObjects = objects;
  mapState.now = performance.now();

  // 用恒等变换清满整个后备缓冲。按 CSS 尺寸清的话，dpr 非整数时
  // Math.round 会多出不到 1px，那一条永远清不到，攒久了就是边缘残影。
  ctx.save();
  ctx.setTransform(1, 0, 0, 1, 0, 0);
  ctx.clearRect(0, 0, mapState.canvas.width, mapState.canvas.height);
  ctx.restore();

  ctx.fillStyle = '#0b0f14';
  ctx.fillRect(0, 0, cw, ch);

  if (mapState.overlays.grid) mapDrawGrid(ctx, cw, ch);

  const sorted = objects.slice().sort((a, b) => drawOrderIndex(a.class) - drawOrderIndex(b.class));
  for (const obj of sorted) mapDrawObject(ctx, obj, mapState.now);
}

// ── 命中测试与交互 ──────────────────────────────────────────────────

function mapHitTest(sx, sy) {
  // 用「本帧实际画出来的位置」判定，否则插值期间点击会对不上眼睛看到的位置
  const objects = mapState.renderObjects;
  if (!objects) return null;

  // 反向遍历：后画的在上层，先命中
  for (let i = objects.length - 1; i >= 0; i--) {
    const o = objects[i];
    if (!Number.isFinite(o.x) || !Number.isFinite(o.y)) continue;
    const x = mapW2SX(o.x);
    const y = mapW2SY(o.y);
    const w = Math.max(2, o.w * mapState.scale);
    const h = Math.max(2, o.h * mapState.scale);
    if (sx >= x && sx <= x + w && sy >= y && sy <= y + h) return o;
  }
  return null;
}

function mapShowTooltip(hit, sx, sy) {
  const tip = mapEl('map-tooltip');
  if (!hit) {
    tip.classList.add('hidden');
    return;
  }

  // 位置是按插值画的，但数值要显示最新快照里的真实值——
  // 调试时看到一堆插出来的小数尾数会分不清哪个是真的。
  const obj = (mapState.last && mapState.last.objects.find((o) => o.id === hit.id)) || hit;

  const lines = [
    `<b>${escapeHtml(obj.class)}</b> #${obj.id}`,
    `x ${num(obj.x)}  y ${num(obj.y)}`,
    `w ${num(obj.w, 0)}  h ${num(obj.h, 0)}`,
    `vx ${num(obj.vx)}  vy ${num(obj.vy)}`,
  ];

  if (obj.collisionBox) {
    const g = obj.collisionBox;
    lines.push(`碰撞 ${g.shape === 'circle' ? '圆 r=' + num(g.r, 0) : '框'} ${num(g.w, 0)}×${num(g.h, 0)}`);
  }

  const t = obj.typeData;
  if (t) {
    if (t.kind === 'Mario') {
      lines.push(`HP ${t.health} · ${t.isBig ? '大' : '小'}${t.transforming ? ' · 变身中' : ''}`);
    } else if (t.kind === 'Goomba') {
      lines.push(t.squashed ? '被踩扁' : (t.killedByFireball ? '炮弹击毙' : '活着'));
    } else if (t.kind === 'Koopa') {
      const label = { Walking: '行走', ShellIdle: '静止壳', ShellMoving: '滑行壳' }[t.state] || t.state;
      lines.push(label + (t.killed ? ' · 击毙' : ''));
    }
  }

  if (!obj.finite) lines.push('<span class="tip-bad">坐标含 NaN/Inf</span>');
  else if (obj._stuck) lines.push('<span class="tip-warn">速度非零但没动</span>');
  else if (!obj.started) lines.push('<span class="tip-warn">未 start</span>');

  tip.innerHTML = lines.join('<br>');
  tip.classList.remove('hidden');

  // tooltip 相对 #view-map 定位，而 sx/sy 是相对画布的，
  // 所以要把画布在容器里的偏移补上，否则会偏一个 padding 的距离。
  const rect = mapState.canvas.getBoundingClientRect();
  const view = mapEl('view-map').getBoundingClientRect();
  const baseX = rect.left - view.left;
  const baseY = rect.top - view.top;

  const tw = tip.offsetWidth;
  const th = tip.offsetHeight;
  let px = sx + 14;
  let py = sy + 14;
  if (px + tw > rect.width - 8) px = sx - tw - 14;
  if (py + th > rect.height - 8) py = sy - th - 14;
  tip.style.left = Math.max(4, baseX + px) + 'px';
  tip.style.top = Math.max(4, baseY + py) + 'px';
}

function mapEventPos(event) {
  const rect = mapState.canvas.getBoundingClientRect();
  return { x: event.clientX - rect.left, y: event.clientY - rect.top };
}

function mapBindEvents() {
  const c = mapState.canvas;

  c.addEventListener('wheel', (event) => {
    event.preventDefault();
    const p = mapEventPos(event);
    const wx = mapS2WX(p.x);
    const wy = mapS2WY(p.y);

    // 以光标位置为锚点缩放，缩放时手指下的世界坐标不动
    const factor = event.deltaY < 0 ? 1.15 : 1 / 1.15;
    const next = Math.min(20, Math.max(0.01, mapState.scale * factor));
    if (next === mapState.scale) return;

    mapState.scale = next;
    mapState.offsetX = wx - p.x / next;
    mapState.offsetY = wy - p.y / next;
    mapDraw();
  }, { passive: false });

  c.addEventListener('mousedown', (event) => {
    mapState.drag = { x: event.clientX, y: event.clientY, ox: mapState.offsetX, oy: mapState.offsetY, moved: false };
  });

  window.addEventListener('mousemove', (event) => {
    if (!mapState.canvas) return;

    if (mapState.drag) {
      const dx = event.clientX - mapState.drag.x;
      const dy = event.clientY - mapState.drag.y;
      if (Math.abs(dx) > 3 || Math.abs(dy) > 3) mapState.drag.moved = true;
      mapState.offsetX = mapState.drag.ox - dx / mapState.scale;
      mapState.offsetY = mapState.drag.oy - dy / mapState.scale;
      mapDraw();
      return;
    }

    const rect = mapState.canvas.getBoundingClientRect();
    if (event.clientX < rect.left || event.clientX > rect.right
        || event.clientY < rect.top || event.clientY > rect.bottom) return;

    const p = { x: event.clientX - rect.left, y: event.clientY - rect.top };
    const hit = mapHitTest(p.x, p.y);
    const id = hit ? hit.id : null;
    if (id !== mapState.hover) {
      mapState.hover = id;
      mapDraw();
    }
    mapShowTooltip(hit, p.x, p.y);
  });

  window.addEventListener('mouseup', (event) => {
    if (!mapState.drag) return;
    const wasDrag = mapState.drag.moved;
    mapState.drag = null;

    // 拖动结束不该当成点击，否则每次平移都会切换跟随
    if (wasDrag) return;

    const rect = mapState.canvas.getBoundingClientRect();
    const p = { x: event.clientX - rect.left, y: event.clientY - rect.top };
    const hit = mapHitTest(p.x, p.y);
    mapState.followId = hit ? (mapState.followId === hit.id ? null : hit.id) : null;
    mapUpdateFollowHint();
    mapDraw();
  });

  c.addEventListener('mouseleave', () => {
    mapState.hover = null;
    mapEl('map-tooltip').classList.add('hidden');
    mapDraw();
  });

  window.addEventListener('resize', () => {
    mapResize();
    mapDraw();
  });
}

function mapUpdateFollowHint() {
  const hint = mapEl('follow-hint');
  if (mapState.followId === null) {
    hint.textContent = '';
    return;
  }
  const obj = mapState.last && mapState.last.objects.find((o) => o.id === mapState.followId);
  hint.textContent = obj ? `跟随 #${obj.id} ${obj.class}` : '';
}

// ── 对外入口 ────────────────────────────────────────────────────────

function mapInit() {
  mapState.canvas = mapEl('map-canvas');
  if (!mapState.canvas) return;
  mapState.ctx = mapState.canvas.getContext('2d');
  mapBindEvents();
  mapResize();

  // 兜底：暂停轮询时不会重绘，靠这个保证布局变化后画布仍然同步
  if (typeof ResizeObserver !== 'undefined') {
    mapState.observer = new ResizeObserver(() => {
      mapResize();
      mapDraw();
    });
    mapState.observer.observe(mapState.canvas);
  }

  // 供浏览器控制台/自动化测试检查视图状态（面板自己的状态，暴露出去便于排查）
  window.__map = mapState;
}

// app.js 每拿到一份「新」快照就调一次。obj._stuck 已在那里算好。
// 这里只负责入帧缓冲，真正的绘制交给 rAF 循环，这样 10Hz 的数据也能画成 60fps。
function mapRender(snap) {
  mapState.last = snap;

  if (mapState.lastSeq !== snap.meta.seq) {
    mapState.lastSeq = snap.meta.seq;
    mapState.frames.push({ snap, at: performance.now() });
    // 只留两份：插值只需要前后两帧，多了反而会把 t 拉得不准
    while (mapState.frames.length > 2) mapState.frames.shift();
  }

  if (!mapState.canvas || mapState.canvas.clientWidth < 10) return;   // 地图标签页还没显示
  mapUpdateFollowHint();
  mapStartLoop();
}

// ── 动画循环 ────────────────────────────────────────────────────────

function mapStartLoop() {
  if (mapState.rafId !== null) return;
  mapState.rafId = requestAnimationFrame(mapLoopStep);
}

function mapStopLoop() {
  if (mapState.rafId !== null) {
    cancelAnimationFrame(mapState.rafId);
    mapState.rafId = null;
  }
}

function mapLoopStep() {
  mapState.rafId = null;

  // 标签页切走 / 浏览器切到后台就不再续帧，回来时由 mapOnShow / mapRender 重启
  const c = mapState.canvas;
  if (!c || c.clientWidth < 10 || document.hidden) return;

  mapDraw();
  mapState.rafId = requestAnimationFrame(mapLoopStep);
}

// 暂停：冻结插值时刻，否则画面还会自己往前滑一小段才停
function mapFreeze() {
  mapState.frozenAt = performance.now();
  mapStopLoop();
  mapDraw();
}

function mapUnfreeze() {
  mapState.frozenAt = null;
  mapStartLoop();
}

function mapSetOverlay(name, on) {
  mapState.overlays[name] = on;
  mapDraw();
}

function mapRefit() {
  mapState.fitPending = true;
  mapState.fitMode = 'cover';
  mapState.followId = null;
  mapUpdateFollowHint();
  mapDraw();
}

function mapFitAll() {
  mapState.fitPending = true;
  mapState.fitMode = 'contain';
  mapState.followId = null;
  mapUpdateFollowHint();
  mapDraw();
}

// 切换到地图标签页时画布尺寸才从 0 变成实际值，必须重新适配
function mapOnShow() {
  mapResize();
  mapStartLoop();
  mapDraw();
}
