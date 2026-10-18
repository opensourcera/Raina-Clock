#ifndef TODO_HTML_H
#define TODO_HTML_H

// The phone page for the to-do list (opened from the QR code on the clock): one day at a time, like a bullet journal.
// The whole list travels as JSON through /api/todo; this page keeps its own copy and sends it back after each change.

const char todo_html[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="theme-color" content="#11111b">
<title>Raina To-do</title>
<style>
:root { --bg:#11111b; --card:#1e1e2e; --line:#313244; --text:#cdd6f4; --dim:#7f849c; --lilac:#cba6f7; --red:#f38ba8; --green:#a6e3a1; --peach:#fab387; }
* { box-sizing: border-box; -webkit-tap-highlight-color: transparent; }
body { margin: 0; background: var(--bg); color: var(--text); font: 17px/1.4 system-ui, -apple-system, 'Segoe UI', sans-serif; padding: env(safe-area-inset-top) 0 90px; }
header { position: sticky; top: 0; background: var(--bg); padding: 14px 16px 10px; z-index: 2; border-bottom: 1px solid var(--line); }
.brand { font-size: 13px; color: var(--dim); letter-spacing: .08em; text-transform: uppercase; }
.daybar { display: flex; align-items: center; justify-content: space-between; margin-top: 6px; }
.daybar button { width: 44px; height: 44px; border-radius: 22px; border: 0; background: var(--card); color: var(--text); font-size: 22px; }
.day { text-align: center; }
.day b { display: block; font-size: 24px; font-weight: 700; }
.day span { font-size: 13px; color: var(--dim); }
.day .today { color: var(--peach); }
main { padding: 8px 12px; }
ul { list-style: none; margin: 0; padding: 0; }
li { display: flex; align-items: center; gap: 12px; padding: 12px 6px; border-bottom: 1px solid var(--line); animation: in .2s ease; }
@keyframes in { from { opacity: 0; transform: translateY(-4px); } to { opacity: 1; transform: none; } }
.bullet { flex: none; width: 30px; height: 30px; border-radius: 15px; border: 2px solid var(--dim); display: grid; place-items: center; color: var(--bg); font-weight: 900; font-size: 18px; transition: all .15s; }
li.urgent .bullet { border-color: var(--red); }
li.done .bullet { background: var(--green); border-color: var(--green); }
.txt { flex: 1; word-break: break-word; }
li.done .txt { color: var(--dim); text-decoration: line-through; }
.from { display: block; font-size: 12px; color: var(--peach); }
.flag, .del { flex: none; border: 0; background: none; color: var(--dim); font-size: 20px; width: 36px; height: 36px; border-radius: 18px; }
li.urgent .flag { color: var(--red); background: rgba(243,139,168,.15); font-weight: 900; }
.empty { text-align: center; color: var(--dim); padding: 40px 0; }
.tools { display: flex; justify-content: space-between; align-items: center; padding: 14px 6px; color: var(--dim); font-size: 15px; }
.tools button { border: 1px solid var(--line); background: none; color: var(--dim); border-radius: 18px; padding: 8px 14px; font-size: 14px; }
.switch { display: flex; align-items: center; gap: 8px; }
.switch input { width: 22px; height: 22px; accent-color: var(--lilac); }
.add { position: fixed; left: 0; right: 0; bottom: 0; display: flex; gap: 8px; padding: 10px 12px calc(10px + env(safe-area-inset-bottom)); background: var(--card); border-top: 1px solid var(--line); }
.add input { flex: 1; min-width: 0; border: 1px solid var(--line); background: var(--bg); color: var(--text); border-radius: 22px; padding: 0 16px; height: 44px; font-size: 17px; }
.add input:focus { outline: none; border-color: var(--lilac); }
.add .u { width: 44px; height: 44px; border-radius: 22px; border: 1px solid var(--line); background: var(--bg); color: var(--dim); font-size: 20px; font-weight: 900; }
.add .u.on { background: var(--red); border-color: var(--red); color: var(--bg); }
.add .go { width: 44px; height: 44px; border-radius: 22px; border: 0; background: var(--lilac); color: var(--bg); font-size: 26px; font-weight: 700; }
.toast { position: fixed; left: 50%; bottom: 80px; transform: translateX(-50%); background: var(--red); color: var(--bg); padding: 8px 14px; border-radius: 16px; font-size: 14px; display: none; }
</style>
</head>
<body>
<header>
  <div class="brand">Raina Clock &middot; To-do</div>
  <div class="daybar">
    <button onclick="shift(-1)" aria-label="Previous day">&lsaquo;</button>
    <div class="day" onclick="goToday()"><b id="dname"></b><span id="ddate"></span></div>
    <button onclick="shift(1)" aria-label="Next day">&rsaquo;</button>
  </div>
</header>
<main>
  <ul id="list"></ul>
  <div class="tools">
    <label class="switch"><input type="checkbox" id="hide" onchange="setHide(this.checked)">Hide done</label>
    <button onclick="clearDone()">Clear done</button>
  </div>
</main>
<form class="add" onsubmit="add(event)">
  <button type="button" class="u" id="urg" onclick="toggleUrg()" aria-label="Urgent">!</button>
  <input id="new" placeholder="Add a task" maxlength="64" autocomplete="off" enterkeyhint="done">
  <button class="go" aria-label="Add">+</button>
</form>
<div class="toast" id="toast"></div>
<script>
let data = { hide_done: false, items: [] };
let urgent = false;
const pad = n => String(n).padStart(2, '0');
const key = d => d.getFullYear() + '-' + pad(d.getMonth() + 1) + '-' + pad(d.getDate());
const todayKey = () => key(new Date());
let view = todayKey();
const $ = id => document.getElementById(id);
function dateOf(k) { const [y, m, d] = k.split('-').map(Number); return new Date(y, m - 1, d); }
function toast(t) { const el = $('toast'); el.textContent = t; el.style.display = 'block'; setTimeout(() => el.style.display = 'none', 2500); }
async function load() {
  try { data = await (await fetch('/api/todo')).json(); } catch (e) { toast('The clock did not answer'); }
  data.items = data.items || [];
  render();
}
async function save() {
  render();
  try {
    const r = await fetch('/api/todo', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(data) });
    if (!r.ok) toast('Not saved: ' + await r.text());
  } catch (e) { toast('Not saved: the clock did not answer'); }
}
function shown() {
  const t = todayKey();
  const out = [];
  data.items.forEach((it, i) => {
    const d = it.day || '';
    const carried = view === t && !it.done && d && d < t;
    if (d === view || carried || (!d && view === t)) {
      if (it.done && data.hide_done) return;
      out.push({ it, i, carried });
    }
  });
  const rank = o => o.it.done ? 2 : (o.it.urgent ? 0 : 1);
  return out.sort((a, b) => rank(a) - rank(b));
}
function render() {
  const d = dateOf(view), t = view === todayKey();
  $('dname').textContent = t ? 'Today' : d.toLocaleDateString(undefined, { weekday: 'long' });
  $('dname').className = t ? 'today' : '';
  $('ddate').textContent = d.toLocaleDateString(undefined, { month: 'long', day: 'numeric', year: 'numeric' });
  $('hide').checked = !!data.hide_done;
  const ul = $('list'); ul.innerHTML = '';
  const rows = shown();
  if (!rows.length) { ul.innerHTML = '<li class="empty" style="display:block;border:0">Nothing planned. Add a task below.</li>'; return; }
  for (const { it, i, carried } of rows) {
    const li = document.createElement('li');
    li.className = (it.done ? 'done ' : '') + (it.urgent ? 'urgent' : '');
    const b = document.createElement('div'); b.className = 'bullet'; b.textContent = it.done ? '✓' : '';
    b.onclick = () => { it.done = !it.done; save(); };
    const tx = document.createElement('div'); tx.className = 'txt'; tx.textContent = it.text;
    tx.onclick = b.onclick;
    if (carried) { const f = document.createElement('span'); f.className = 'from'; f.textContent = '› from ' + dateOf(it.day).toLocaleDateString(undefined, { weekday: 'short', month: 'short', day: 'numeric' }); tx.appendChild(f); }
    const fl = document.createElement('button'); fl.className = 'flag'; fl.textContent = '!'; fl.setAttribute('aria-label', 'Urgent');
    fl.onclick = () => { it.urgent = !it.urgent; save(); };
    const del = document.createElement('button'); del.className = 'del'; del.textContent = '×'; del.setAttribute('aria-label', 'Delete');
    del.onclick = () => { data.items.splice(i, 1); save(); };
    li.append(b, tx, fl, del); ul.appendChild(li);
  }
}
function add(e) {
  e.preventDefault();
  const v = $('new').value.trim(); if (!v) return;
  if (data.items.length >= 40) { toast('The list is full (40). Clear done tasks first.'); return; }
  data.items.push({ text: v, done: false, urgent, day: view });
  $('new').value = ''; urgent = false; $('urg').classList.remove('on');
  save();
}
function toggleUrg() { urgent = !urgent; $('urg').classList.toggle('on', urgent); }
function setHide(v) { data.hide_done = v; save(); }
function clearDone() { data.items = data.items.filter(it => !it.done); save(); }
function shift(n) { const d = dateOf(view); d.setDate(d.getDate() + n); view = key(d); render(); }
function goToday() { view = todayKey(); render(); }
load();
setInterval(() => { if (document.visibilityState === 'visible' && document.activeElement !== $('new')) load(); }, 15000);
</script>
</body>
</html>
)=====";

#endif // TODO_HTML_H
