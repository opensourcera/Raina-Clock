#ifndef ALARM_HTML_H
#define ALARM_HTML_H

const char alarm_html[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Alarm</title>
<style>
body { font-family: 'Inter', system-ui, sans-serif; background: #1e1e2e; color: #cdd6f4; margin: 0; padding: 20px; display: flex; justify-content: center; align-items: flex-start; min-height: 100vh; box-sizing: border-box; }
.card { background: #181825; border-radius: 12px; padding: 24px; width: 100%; max-width: 500px; box-shadow: 0 8px 30px rgba(0,0,0,0.3); border: 1px solid #313244; }
h2 { color: #f9e2af; margin: 0 0 4px 0; font-weight: 600; text-align: center; }
.sub { text-align: center; color: #a6adc8; font-size: 14px; margin: 0 0 18px 0; }
.next { text-align: center; font-size: 20px; color: #a6e3a1; margin-bottom: 18px; }
.next.ring { color: #f38ba8; }
label { display: block; margin-bottom: 6px; color: #a6adc8; font-size: 14px; }
input[type='time'], input[type='number'], select { width: 100%; padding: 10px; margin-bottom: 16px; border-radius: 6px; border: 1px solid #45475a; background: #313244; color: #cdd6f4; font-size: 16px; box-sizing: border-box; }
input[type='range'] { width: 100%; margin-bottom: 16px; }
.row { display: flex; gap: 8px; margin-bottom: 16px; }
.row button { flex: 1; }
button { padding: 11px 12px; background: #cba6f7; border: none; border-radius: 6px; color: #11111b; font-size: 15px; font-weight: bold; cursor: pointer; }
button:hover { background: #f5c2e7; }
button.quiet { background: #313244; color: #cdd6f4; font-weight: normal; }
button.quiet:hover { background: #45475a; }
button.danger { background: #f38ba8; }
.check { display: flex; align-items: center; gap: 10px; margin-bottom: 16px; }
.check input { transform: scale(1.3); }
.check label { margin: 0; }
.section { color: #89b4fa; font-size: 16px; margin: 20px 0 12px 0; border-bottom: 1px solid #313244; padding-bottom: 4px; }
.btn-back { display: block; margin-top: 16px; color: #a6adc8; text-decoration: none; font-size: 14px; text-align: center; }
.note { color: #6c7086; font-size: 13px; margin-top: -8px; margin-bottom: 14px; }
#status { color: #a6adc8; font-size: 13px; min-height: 18px; }
</style>
</head>
<body>
<div class='card'>
<h2>Alarm</h2>
<p class='sub'>Rings on the clock. Snooze and Stop are on its screen.</p>
<div id='next' class='next'>...</div>

<div class='section'>Wake me in</div>
<div class='row'>
  <button onclick='inHours(6)'>6 hours</button>
  <button onclick='inHours(7)'>7 hours</button>
  <button onclick='inHours(8)'>8 hours</button>
</div>
<p class='note'>One shot from now. The exact minute is shown above once set.</p>

<div class='section'>Or at a time</div>
<label for='time'>Time</label>
<input type='time' id='time'>
<div class='check'><input type='checkbox' id='repeat' checked><label for='repeat'>Every day</label></div>
<div class='check'><input type='checkbox' id='enabled'><label for='enabled'>Alarm on</label></div>

<div class='section'>Sound</div>
<label for='volume'>Volume <span id='volv'></span></label>
<input type='range' id='volume' min='0' max='100'>
<label for='snooze'>Snooze minutes</label>
<input type='number' id='snooze' min='1' max='60'>
<div class='row'>
  <button class='quiet' onclick='test()'>Test sound</button>
  <button class='quiet' onclick='stopSound()'>Stop sound</button>
</div>
<button onclick='save()'>Save</button>

<p id='status'></p>

<a class='btn-back' href='/'>&larr; Back</a>
</div>
<script>
const $ = id => document.getElementById(id);
async function load() {
  const a = await (await fetch('/api/alarm')).json();
  const n = $('next'); n.textContent = a.ringing ? 'Ringing now' : a.next; n.className = 'next' + (a.ringing ? ' ring' : '');
  $('time').value = String(a.hour).padStart(2, '0') + ':' + String(a.minute).padStart(2, '0');
  $('repeat').checked = a.repeat; $('enabled').checked = a.enabled;
  $('volume').value = a.volume; $('volv').textContent = a.volume + '%'; $('snooze').value = a.snooze_minutes;
}
$('volume').oninput = e => $('volv').textContent = e.target.value + '%';
async function post(body) {
  const r = await fetch('/api/alarm', {method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(body)});
  if (!r.ok) alert('The clock refused it: ' + await r.text());
  await load();
}
function inHours(h) { post({in_hours: h}); }
function save() {
  const [h, m] = $('time').value.split(':').map(Number);
  post({hour: h, minute: m, repeat: $('repeat').checked, enabled: $('enabled').checked, volume: Number($('volume').value), snooze_minutes: Number($('snooze').value)});
}
async function test() { await fetch('/api/alarm/test', {method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({volume: Number($('volume').value)})}); $('status').textContent = 'Beeping on the clock.'; }
async function stopSound() { await fetch('/api/alarm/stop', {method: 'POST'}); $('status').textContent = 'Stopped.'; }
load();
</script>
</body>
</html>
)=====";

#endif // ALARM_HTML_H
