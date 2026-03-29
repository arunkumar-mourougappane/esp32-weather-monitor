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
select{width:100%;background:#0d0d1a;border:1px solid #2a2a50;border-radius:8px;padding:9px 13px;color:#dde;font-size:.88rem;outline:none;-webkit-appearance:none;appearance:none;cursor:pointer;transition:border-color .2s}
select:focus{border-color:#818cf8}
.hint{font-size:.72rem;color:#556;margin-top:3px}
.row{display:flex;gap:10px}
.row .f{flex:1}
.err{color:#f87171;font-size:.78rem;margin-top:6px;display:none}
button{width:100%;background:linear-gradient(135deg,#818cf8,#c084fc);border:none;border-radius:10px;padding:13px;color:#fff;font-size:.95rem;font-weight:600;cursor:pointer;margin-top:4px;letter-spacing:.03em;transition:opacity .2s}
button:hover{opacity:.88}
.ok{background:#0d1f0d;border:1px solid #2a502a;border-radius:12px;padding:22px;text-align:center;display:none}
.ok h2{color:#4ade80;margin-bottom:8px;font-size:1.1rem}
.ok p{color:#778;font-size:.85rem}
.wi{border:1px solid #2a2a50;border-radius:8px;padding:12px;margin-bottom:10px}
.wlbl{font-size:.7rem;color:#818cf8;font-weight:700;text-transform:uppercase;letter-spacing:.09em;margin-bottom:8px}
.rmb{background:none;border:1px solid #f87171;color:#f87171;border-radius:6px;padding:4px 10px;font-size:.75rem;cursor:pointer;width:auto;margin-top:6px}
.rmb:hover{background:#f8717118}
#addwb{background:#13132a;border:1px dashed #818cf8;color:#818cf8;border-radius:8px;padding:8px;font-size:.84rem;width:100%;margin-top:2px;cursor:pointer;font-weight:400;letter-spacing:0}
#addwb:hover{background:#818cf812}
</style></head><body>
<div class="card">
<h1>&#9729;&#65039; M5Paper Weather</h1>
<p class="sub">Complete setup to connect your device</p>
<form id="frm" onsubmit="return go()">
<fieldset>
<legend>&#128246; WiFi Networks</legend>
<div id="wl"></div>
<button type="button" id="addwb" onclick="addW()">+ Add Network</button>
<p class="hint" style="margin-top:8px">Up to 5 networks. The device connects to whichever network is available with the strongest signal.</p>
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
<div class="f"><label>Timezone</label>
<select name="tz" id="tz" required>
<optgroup label="United States">
<option value="EST5EDT,M3.2.0,M11.1.0">US/Eastern (EST/EDT) — New York, Miami</option>
<option value="CST6CDT,M3.2.0,M11.1.0" selected>US/Central (CST/CDT) — Chicago, Dallas</option>
<option value="MST7MDT,M3.2.0,M11.1.0">US/Mountain (MST/MDT) — Denver, Salt Lake City</option>
<option value="MST7">US/Arizona (MST, no DST) — Phoenix</option>
<option value="PST8PDT,M3.2.0,M11.1.0">US/Pacific (PST/PDT) — Los Angeles, Seattle</option>
<option value="AKST9AKDT,M3.2.0,M11.1.0">US/Alaska (AKST/AKDT) — Anchorage</option>
<option value="HST10">US/Hawaii (HST, no DST) — Honolulu</option>
</optgroup>
<optgroup label="Canada">
<option value="NST3:30NDT,M3.2.0,M11.1.0">Canada/Newfoundland (NST/NDT)</option>
<option value="AST4ADT,M3.2.0,M11.1.0">Canada/Atlantic (AST/ADT) — Halifax</option>
<option value="EST5EDT,M3.2.0,M11.1.0">Canada/Eastern — Toronto, Montreal</option>
<option value="CST6CDT,M3.2.0,M11.1.0">Canada/Central — Winnipeg</option>
<option value="MST7MDT,M3.2.0,M11.1.0">Canada/Mountain — Calgary, Edmonton</option>
<option value="PST8PDT,M3.2.0,M11.1.0">Canada/Pacific — Vancouver</option>
</optgroup>
<optgroup label="Latin America">
<option value="BRST3BRDT,M10.3.0,M2.3.0">Brazil/Sao Paulo (BRT/BRST)</option>
<option value "ART3">Argentina (ART, no DST) — Buenos Aires</option>
<option value="COT5">Colombia (COT) — Bogota</option>
<option value="MEX6CDT,M4.1.0,M10.5.0">Mexico/Central (CST/CDT) — Mexico City</option>
</optgroup>
<optgroup label="Europe">
<option value="GMT0BST,M3.5.0/1,M10.5.0">Europe/London (GMT/BST)</option>
<option value="WET0WEST,M3.5.0/1,M10.5.0/1">Europe/Lisbon (WET/WEST)</option>
<option value="CET-1CEST,M3.5.0,M10.5.0/3">Europe/Berlin, Paris, Rome (CET/CEST)</option>
<option value="CET-1CEST,M3.5.0,M10.5.0/3">Europe/Madrid, Amsterdam (CET/CEST)</option>
<option value="EET-2EEST,M3.5.0/3,M10.5.0/4">Europe/Athens, Bucharest (EET/EEST)</option>
<option value="EET-2EEST,M3.5.0/3,M10.5.0/4">Europe/Helsinki, Riga (EET/EEST)</option>
<option value="MSK-3">Europe/Moscow (MSK, no DST)</option>
</optgroup>
<optgroup label="Middle East / Africa">
<option value="TRT-3">Turkey/Istanbul (TRT)</option>
<option value="IRST-3:30IRDT,80/0,264/0">Iran (IRST/IRDT) — Tehran</option>
<option value="GST-4">Gulf (GST) — Dubai, Abu Dhabi</option>
<option value="PKT-5">Pakistan (PKT) — Karachi</option>
<option value="EAT-3">East Africa (EAT) — Nairobi</option>
<option value="CAT-2">Central Africa (CAT) — Johannesburg</option>
<option value="WAT-1">West Africa (WAT) — Lagos</option>
</optgroup>
<optgroup label="Asia">
<option value="IST-5:30">Asia/Kolkata (IST) — India</option>
<option value="NPT-5:45">Asia/Kathmandu (NPT) — Nepal</option>
<option value="BST-6">Asia/Dhaka (BST) — Bangladesh</option>
<option value="ICT-7">Asia/Bangkok (ICT) — Thailand, Vietnam</option>
<option value="CST-8">Asia/Shanghai (CST) — China (all zones)</option>
<option value="SGT-8">Asia/Singapore (SGT)</option>
<option value="HKT-8">Asia/Hong Kong (HKT)</option>
<option value="JST-9">Asia/Tokyo (JST) — Japan</option>
<option value="KST-9">Asia/Seoul (KST) — South Korea</option>
</optgroup>
<optgroup label="Pacific">
<option value="AEST-10AEDT,M10.1.0,M4.1.0/3">Australia/Sydney, Melbourne (AEST/AEDT)</option>
<option value="ACST-9:30ACDT,M10.1.0,M4.1.0/3">Australia/Adelaide (ACST/ACDT)</option>
<option value="AEST-10">Australia/Brisbane (AEST, no DST)</option>
<option value="AWST-8">Australia/Perth (AWST, no DST)</option>
<option value="NZST-12NZDT,M9.5.0,M4.1.0/3">Pacific/Auckland (NZST/NZDT)</option>
</optgroup>
<optgroup label="Other">
<option value="UTC0">UTC (no offset)</option>
</optgroup>
</select>
<p class="hint">Select the timezone closest to your location. All entries use POSIX TZ strings — validated before saving.</p>
</div>
<div class="f"><label>NTP Server</label><input name="ntp" value="pool.ntp.org" required></div>
</fieldset>
<fieldset>
<legend>&#8987; Sync</legend>
<div class="f"><label>Update Interval</label>
<select name="sync_interval" id="sync_interval" required>
<option value="15">Every 15 minutes</option>
<option value="30" selected>Every 30 minutes</option>
<option value="60">Every 1 hour</option>
<option value="120">Every 2 hours</option>
</select>
</div>
<div class="row">
<div class="f"><label>Night Mode Start (0&#8211;23 h)</label><input type="number" name="nm_start" value="22" min="0" max="23" required></div>
<div class="f"><label>Night Mode End (0&#8211;23 h)</label><input type="number" name="nm_end" value="6" min="0" max="23" required></div>
</div>
<p class="hint">Device skips WiFi fetches between these hours to save battery (~25&#8211;35% gain). Set both to the same value to disable.</p>
<div class="f"><label>Webhook URL (optional)</label><input name="webhook_url" placeholder="http://..."></div>
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
var wifis=[{s:'',p:''}];
function hesc(v){return v.replace(/&/g,'&amp;').replace(/"/g,'&quot;');}
function syncW(){
  var e=document.querySelectorAll('.wi');
  for(var i=0;i<e.length&&i<wifis.length;i++){
    var si=e[i].querySelector('input[data-r="s"]');
    var pi=e[i].querySelector('input[data-r="p"]');
    if(si)wifis[i].s=si.value;
    if(pi)wifis[i].p=pi.value;
  }
}
function renderW(){
  var wl=document.getElementById('wl');wl.innerHTML='';
  for(var i=0;i<wifis.length;i++){
    var d=document.createElement('div');d.className='wi';
    d.innerHTML='<div class="wlbl">Network '+(i+1)+(i===0?' <span style="color:#556">(primary)</span>':'')+'</div>'
      +'<div class="f"><label>SSID</label><input name="ssid_'+i+'" data-r="s" placeholder="Network name"'+(i===0?' required':'')+' autocomplete="off" value="'+hesc(wifis[i].s)+'"></div>'
      +'<div class="f"><label>Password</label><input type="password" name="pass_'+i+'" data-r="p" placeholder="Leave blank if open" autocomplete="new-password" value="'+hesc(wifis[i].p)+'"></div>'
      +(i>0?'<button type="button" class="rmb" onclick="rmW('+i+')">&#215;&nbsp;Remove</button>':'');
    wl.appendChild(d);
  }
  document.getElementById('addwb').style.display=wifis.length>=5?'none':'block';
}
function addW(){syncW();if(wifis.length>=5)return;wifis.push({s:'',p:''});renderW();}
function rmW(i){syncW();wifis.splice(i,1);renderW();}
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
renderW();
</script>
</body></html>
)rawliteral";
