#ifndef LANDING_HTML_H
#define LANDING_HTML_H

// The phone start page (the QR code behind a tap on "Raina Clock"): one card for each part of the clock, then the
// maintenance links. %APP_VERSION% and %DEVICE_NAME% are filled in by WifiManager::handleLanding().

const char landing_html[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="theme-color" content="#11111b">
<title>Raina Clock</title>
<style>
:root { --bg:#11111b; --card:#1e1e2e; --line:#313244; --text:#cdd6f4; --dim:#7f849c; --lilac:#cba6f7; --peach:#fab387; --red:#f38ba8; }
* { box-sizing: border-box; -webkit-tap-highlight-color: transparent; }
body { margin: 0; background: var(--bg); color: var(--text); font: 17px/1.4 system-ui, -apple-system, 'Segoe UI', sans-serif; padding: calc(16px + env(safe-area-inset-top)) 16px 32px; }
.top { display: flex; align-items: center; gap: 12px; margin-bottom: 18px; }
.logo { width: 46px; height: 46px; border-radius: 23px; background: var(--lilac); display: grid; place-items: center; font-size: 26px; }
h1 { margin: 0; font-size: 24px; }
.sub { color: var(--dim); font-size: 13px; }
.grid { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
a.card { display: block; text-decoration: none; color: var(--text); background: var(--card); border: 1px solid var(--line); border-radius: 18px; padding: 16px; min-height: 120px; transition: transform .1s; }
a.card:active { transform: scale(.97); }
.ic { font-size: 30px; line-height: 1; }
.card b { display: block; margin-top: 10px; font-size: 18px; }
.card span { display: block; color: var(--dim); font-size: 13px; margin-top: 2px; }
h2 { font-size: 13px; color: var(--dim); text-transform: uppercase; letter-spacing: .08em; margin: 26px 4px 8px; }
.list { background: var(--card); border: 1px solid var(--line); border-radius: 18px; overflow: hidden; }
.list a { display: flex; justify-content: space-between; padding: 14px 16px; color: var(--text); text-decoration: none; border-bottom: 1px solid var(--line); }
.list a:last-child { border-bottom: 0; }
.list a::after { content: '\203A'; color: var(--dim); }
.list a.danger { color: var(--red); }
footer { text-align: center; color: var(--dim); font-size: 12px; margin-top: 24px; }
</style>
</head>
<body>
<div class="top">
  <div class="logo">&#128103;</div>
  <div><h1>Raina Clock</h1><div class="sub">%DEVICE_NAME%</div></div>
</div>
<div class="grid">
  <a class="card" href="/alarm"><div class="ic">&#9200;</div><b>Clock</b><span>Alarm time, count down, volume</span></a>
  <a class="card" href="/settings#weather"><div class="ic">&#9925;</div><b>Weather</b><span>Location, units, time zone</span></a>
  <a class="card" href="/todo"><div class="ic">&#9989;</div><b>To-do</b><span>Plan the day, urgent, done</span></a>
  <a class="card" href="/settings"><div class="ic">&#9881;&#65039;</div><b>Settings</b><span>Wi-Fi, look, sensor, system</span></a>
</div>
<h2>Maintenance</h2>
<div class="list">
  <a href="/log">Event log</a>
  <a href="/update">Firmware update</a>
  <a href="/clear_cache" onclick="return confirm('Clear the weather cache on the SD card?')">Clear weather cache</a>
  <a class="danger" href="/reset?confirm=yes" onclick="return confirm('Erase all settings and restart the clock?')">Factory reset</a>
</div>
<footer>Raina Clock %APP_VERSION%</footer>
</body>
</html>
)=====";

#endif // LANDING_HTML_H
