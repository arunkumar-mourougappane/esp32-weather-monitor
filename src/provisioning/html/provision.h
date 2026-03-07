// Embedded provisioning page — minified dark-themed HTML.
// Served by ProvisionWebServer from DRAM/flash.
// Sections: WiFi, Location+API, Time, Security PIN.
#pragma once

static const char PROVISION_HTML[] = R"rawliteral(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>M5Paper Setup</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0d0d1a;color:#dde;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;display:flex;align-items:center;justify-content:center;min-height:100vh;padding:16px}
.card{background:#13132a;border:1px solid #2a2a50;border-radius:18px;padding:28px 24px;width:100%;max-width:460px;box-shadow:0 8px 40px #0006}
h1{font-size:1.45rem;font-weight:700;margin-bottom:6px;background:linear-gradient(135deg,#818cf8,#c084fc);-webkit-background-clip:text;-webkit-text-fill-color:transparent}
.sub{color:#778;font-size:.85rem;margin-bottom:24px}
fieldset{border:1px solid #2a2a50;border-radius:12px;padding:14px 16px;margin-bottom:18px}
legend{color:#818cf8;font-size:.7rem;font-weight:700;text-transform:uppercase;letter-spacing:.1em;padding:0 8px}
.f{margin-bottom:12px}
.f:last-child{margin-bottom:0}
label{display:block;font-size:.78rem;color:#99a;margin-bottom:5px}
input{width:100%;background:#0d0d1a;border:1px solid #2a2a50;border-radius:8px;padding:9px 13px;color:#dde;font-size:.88rem;outline:none;transition:border-color .2s}
input:focus{border-color:#818cf8}
.hint{font-size:.72rem;color:#556;margin-top:3px}
.row{display:flex;gap:10px}
.row .f{flex:1}
.err{color:#f87171;font-size:.78rem;margin-top:6px;display:none}
button{width:100%;background:linear-gradient(135deg,#818cf8,#c084fc);border:none;border-radius:10px;padding:13px;color:#fff;font-size:.95rem;font-weight:600;cursor:pointer;margin-top:4px;letter-spacing:.03em;transition:opacity .2s}
button:hover{opacity:.88}
.ok{background:#0d1f0d;border:1px solid #2a502a;border-radius:12px;padding:22px;text-align:center;display:none}
.ok h2{color:#4ade80;margin-bottom:8px;font-size:1.1rem}
.ok p{color:#778;font-size:.85rem}
</style></head><body>
<div class="card">
<h1>&#9729;&#65039; M5Paper Weather</h1>
<p class="sub">Complete setup to connect your device</p>
<form id="frm" onsubmit="return go()">
<fieldset>
<legend>&#128246; WiFi</legend>
<div class="f"><label>SSID</label><input name="ssid" placeholder="Network name" required autocomplete="off"></div>
<div class="f"><label>Password</label><input type="password" name="pass" placeholder="Network password" autocomplete="new-password"></div>
</fieldset>
<fieldset>
<legend>&#127760; Location &amp; API</legend>
<div class="f"><label>Google Weather API Key</label><input name="api_key" placeholder="AIza..." required autocomplete="off"></div>
<div class="f"><label>Display City Name</label><input name="city" placeholder="Chicago" required></div>
<div class="f"><label>State / Province <span style="color:#556">(optional)</span></label><input name="state" placeholder="Illinois"></div>
<div class="row">
<div class="f"><label>Country (ISO)</label><input name="country" placeholder="US" maxlength="3" required></div>
</div>
<div class="row">
<div class="f"><label>Latitude</label><input name="lat" placeholder="41.8781" required pattern="-?\d+(\.\d+)?"></div>
<div class="f"><label>Longitude</label><input name="lon" placeholder="-87.6298" required pattern="-?\d+(\.\d+)?"></div>
</div>
</fieldset>
<fieldset>
<legend>&#128336; Time</legend>
<div class="f"><label>Timezone (POSIX)</label><input name="tz" placeholder="CST6CDT,M3.2.0,M11.1.0" required><div class="hint">POSIX TZ string &mdash; use <a href="https://github.com/nayarsystems/posix_tz_db" target="_blank" style="color:#818cf8">this list</a></div></div>
<div class="f"><label>NTP Server</label><input name="ntp" value="pool.ntp.org" required></div>
</fieldset>
<fieldset>
<legend>&#128274; Security PIN</legend>
<div class="f"><label>PIN (4–8 digits)</label><input type="password" name="pin" id="pin" inputmode="numeric" pattern="[0-9]{4,8}" placeholder="&bull;&bull;&bull;&bull;" required></div>
<div class="f"><label>Confirm PIN</label><input type="password" name="pin2" id="pin2" inputmode="numeric" pattern="[0-9]{4,8}" placeholder="&bull;&bull;&bull;&bull;" required></div>
<div class="err" id="perr">PINs do not match</div>
</fieldset>
<div class="err" id="ferr"></div>
<button type="submit">Save &amp; Restart &#8594;</button>
</form>
<div class="ok" id="ok">
<h2>&#10003; Saved!</h2>
<p>Device is restarting and connecting to your WiFi&hellip;</p>
</div>
</div>
<script>
function go(){
  var p=document.getElementById('pin').value,p2=document.getElementById('pin2').value;
  var pe=document.getElementById('perr'),fe=document.getElementById('ferr');
  if(p!==p2){pe.style.display='block';return false;}
  pe.style.display='none';fe.style.display='none';
  var fd=new FormData(document.getElementById('frm'));
  fetch('/save',{method:'POST',body:fd})
    .then(function(r){
      if(r.ok){document.getElementById('frm').style.display='none';document.getElementById('ok').style.display='block';}
      else r.text().then(function(t){fe.textContent=t||'Error saving. Try again.';fe.style.display='block';});
    }).catch(function(){fe.textContent='Network error.';fe.style.display='block';});
  return false;
}
</script>
</body></html>
)rawliteral";
