#ifndef TODO_HTML_H
#define TODO_HTML_H

const char todo_html[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>To-do</title>
<style>
body { font-family: 'Inter', system-ui, sans-serif; background: #1e1e2e; color: #cdd6f4; margin: 0; padding: 20px; display: flex; justify-content: center; align-items: flex-start; min-height: 100vh; box-sizing: border-box; }
.card { background: #181825; border-radius: 12px; padding: 24px; width: 100%; max-width: 500px; box-shadow: 0 8px 30px rgba(0,0,0,0.3); border: 1px solid #313244; }
h2 { color: #fab387; margin: 0 0 4px 0; font-weight: 600; text-align: center; }
.sub { text-align: center; color: #a6adc8; font-size: 14px; margin: 0 0 18px 0; }
.add { display: flex; gap: 8px; margin-bottom: 16px; }
.add input { flex: 1; padding: 12px; border-radius: 6px; border: 1px solid #45475a; background: #313244; color: #cdd6f4; font-size: 16px; }
.add input:focus { outline: none; border-color: #fab387; }
.add button { width: auto; margin: 0; padding: 12px 18px; }
button { padding: 10px 12px; background: #cba6f7; border: none; border-radius: 6px; color: #11111b; font-size: 15px; font-weight: bold; cursor: pointer; }
button:hover { background: #f5c2e7; }
ul { list-style: none; padding: 0; margin: 0; }
li { display: flex; align-items: center; gap: 10px; padding: 10px 6px; border-bottom: 1px solid #313244; }
li input[type='checkbox'] { transform: scale(1.4); margin: 0 4px; cursor: pointer; }
li .text { flex: 1; font-size: 16px; word-break: break-word; }
li.done .text { color: #6c7086; text-decoration: line-through; }
li .tools button { background: #313244; color: #cdd6f4; padding: 6px 9px; margin-left: 4px; font-size: 13px; }
li .tools button:hover { background: #45475a; }
.foot { display: flex; justify-content: space-between; align-items: center; margin-top: 16px; }
.foot button { background: #313244; color: #cdd6f4; font-weight: normal; }
.btn-back { color: #a6adc8; text-decoration: none; font-size: 14px; }
.btn-back:hover { color: #cdd6f4; }
.empty { text-align: center; color: #6c7086; padding: 24px 0; }
</style>
</head>
<body>
<div class='card'>
<h2>To-do</h2>
<p class='sub'>Shown on the clock. Up to 24 items.</p>
<div class='add'>
  <input id='newtext' maxlength='64' placeholder='Add an item' autocomplete='off'>
  <button onclick='addItem()'>Add</button>
</div>
<ul id='list'></ul>
<div class='foot'>
  <a class='btn-back' href='/'>&larr; Back</a>
  <button onclick='clearDone()'>Clear done</button>
</div>
</div>
<script>
let items = [];
function esc(s) { return s.replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c])); }
function render() {
  const ul = document.getElementById('list');
  if (!items.length) { ul.innerHTML = "<li class='empty'>Nothing yet. Add your first item above.</li>"; return; }
  ul.innerHTML = items.map((it, i) =>
    `<li class='${it.done ? 'done' : ''}'>
      <input type='checkbox' ${it.done ? 'checked' : ''} onchange='toggle(${i})'>
      <span class='text' ondblclick='edit(${i})'>${esc(it.text)}</span>
      <span class='tools'>
        <button title='Up' onclick='move(${i},-1)'>&uarr;</button>
        <button title='Down' onclick='move(${i},1)'>&darr;</button>
        <button title='Delete' onclick='del(${i})'>&#10005;</button>
      </span>
    </li>`).join('');
}
async function load() {
  const r = await fetch('/api/todo'); const j = await r.json(); items = j.items || []; render();
}
async function push() {
  const r = await fetch('/api/todo', {method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({items})});
  if (!r.ok) alert('The clock refused the change: ' + await r.text());
  await load();
}
function addItem() {
  const el = document.getElementById('newtext'); const t = el.value.trim();
  if (!t) return; if (items.length >= 24) { alert('The list is full (24 items).'); return; }
  items.push({text: t, done: false}); el.value = ''; push();
}
function toggle(i) { items[i].done = !items[i].done; push(); }
function del(i) { items.splice(i, 1); push(); }
function move(i, d) { const j = i + d; if (j < 0 || j >= items.length) return; [items[i], items[j]] = [items[j], items[i]]; push(); }
function edit(i) { const t = prompt('Edit item', items[i].text); if (t === null) return; const v = t.trim(); if (!v) return; items[i].text = v.slice(0, 64); push(); }
function clearDone() { items = items.filter(it => !it.done); push(); }
document.getElementById('newtext').addEventListener('keydown', e => { if (e.key === 'Enter') addItem(); });
load();
</script>
</body>
</html>
)=====";

#endif // TODO_HTML_H
