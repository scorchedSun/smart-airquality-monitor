#pragma once

#include <functional>
#include <string>
#include <ESPAsyncWebServer.h>
#define WEBSERVER_H  // Prevent ArduinoOTA from pulling in conflicting WebServer
#include <ArduinoJson.h>
#include "ConfigManager.h"
#include "config_keys.h"
#include "Logger.h"
#include <Update.h>

class WebConfig {
public:
    using ConfigChangeCallback = std::function<void()>;

private:
    AsyncWebServer server;
    ConfigChangeCallback on_config_changed;
    bool ap_mode = false;
    size_t update_content_len = 0;

    static const char* getConfigPage() {
        static const char page[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SMAQ Config</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Segoe UI',system-ui,sans-serif;background:#1a1a2e;color:#e0e0e0;padding:20px}
.card{background:#16213e;border-radius:12px;padding:24px;margin:12px auto;max-width:480px;box-shadow:0 4px 20px rgba(0,0,0,.3)}
h1{text-align:center;color:#0af;margin-bottom:8px;font-size:1.5em}
.subtitle{text-align:center;color:#888;margin-bottom:20px;font-size:.85em}
.section{margin-top:18px}
.section h2{font-size:1em;color:#0af;border-bottom:1px solid #2a3a5e;padding-bottom:6px;margin-bottom:12px}
.field{margin-bottom:14px}
label{display:block;font-size:.85em;color:#aaa;margin-bottom:4px}
input[type=text],input[type=password],input[type=number]{width:100%;padding:8px 12px;background:#0d1b3e;border:1px solid #2a3a5e;border-radius:6px;color:#e0e0e0;font-size:.9em}
input:focus{outline:none;border-color:#0af}
.range-wrap{display:flex;align-items:center;gap:10px}
.range-wrap input[type=range]{flex:1;accent-color:#0af}
.range-val{min-width:36px;text-align:right;font-weight:600;color:#0af}
.toggle{display:flex;align-items:center;justify-content:space-between}
.switch{position:relative;width:48px;height:26px}
.switch input{opacity:0;width:0;height:0}
.slider{position:absolute;cursor:pointer;inset:0;background:#2a3a5e;border-radius:26px;transition:.3s}
.slider:before{content:'';position:absolute;height:20px;width:20px;left:3px;bottom:3px;background:#666;border-radius:50%;transition:.3s}
input:checked+.slider{background:#0af}
input:checked+.slider:before{transform:translateX(22px);background:#fff}
.btn{display:block;width:100%;padding:12px;background:linear-gradient(135deg,#0af,#06d);color:#fff;border:none;border-radius:8px;font-size:1em;font-weight:600;cursor:pointer;margin-top:20px;transition:opacity .2s}
.btn:hover{opacity:.85}
.btn:active{opacity:.7}
.btn-reboot{background:linear-gradient(135deg,#f80,#d40);margin-top:12px}
.status{text-align:center;font-size:.8em;margin-top:10px;min-height:1.2em}
.ok{color:#0f8}
.err{color:#f44}
</style>
</head>
<body>
<div class="card">
<h1>&#x1F32C; Air Quality Monitor</h1>
<p class="subtitle">Device Configuration</p>

<form id="cfg" autocomplete="off">
<div class="section"><h2>WiFi</h2>
<div class="field"><label>SSID</label><input type="text" id="wifi_ssid"></div>
<div class="field"><label>Password</label><input type="password" id="wifi_pass"></div>
</div>

<div class="section"><h2>MQTT</h2>
<div class="field"><label>Broker</label><input type="text" id="mqtt_broker"></div>
<div class="field"><label>Port</label><input type="number" id="mqtt_port" min="1" max="65535"></div>
<div class="field"><label>User</label><input type="text" id="mqtt_user"></div>
<div class="field"><label>Password</label><input type="password" id="mqtt_pass"></div>
</div>

<div class="section"><h2>Device</h2>
<div class="field"><label>Friendly Name</label><input type="text" id="friendly_name"></div>
<div class="field"><label>Host Name</label><input type="text" id="host_name"></div>

<div class="field toggle">
<label>Display Enabled</label>
<label class="switch"><input type="checkbox" id="enable_display"><span class="slider"></span></label>
</div>

<div class="field">
<label>Display Interval (s)</label>
<div class="range-wrap"><input type="range" id="display_interval" min="5" max="15" step="5"><span class="range-val" id="rv_di">10</span></div>
</div>

<div class="field">
<label>Report Interval (min)</label>
<div class="range-wrap"><input type="range" id="report_interval" min="1" max="15" step="1"><span class="range-val" id="rv_ri">5</span></div>
</div>

<div class="field">
<label>Fan Speed (%)</label>
<div class="range-wrap"><input type="range" id="fan_speed" min="0" max="100" step="5"><span class="range-val" id="rv_fs">20</span></div>
</div>
</div>

<div class="section"><h2>Logging</h2>
<div class="field"><label>Syslog Server IP</label><input type="text" id="syslog_ip"></div>
<div class="field"><label>Syslog Port</label><input type="number" id="syslog_port" min="1" max="65535"></div>
</div>

<button type="submit" class="btn">Save Configuration</button>
<div class="status" id="st"></div>
</form>
<button class="btn btn-reboot" id="rebootBtn" onclick="doReboot()">&#x1F504; Reboot Device</button>
<a href="/update"><button class="btn btn-reboot" type="button" style="background:linear-gradient(135deg,#555,#333)">&#x2B06; Firmware Update</button></a>
</div>

<script>
const ids=['wifi_ssid','wifi_pass','mqtt_broker','mqtt_port','mqtt_user','mqtt_pass',
'friendly_name','host_name','enable_display','display_interval','report_interval',
'fan_speed','syslog_ip','syslog_port'];
const rangeMap={display_interval:'rv_di',report_interval:'rv_ri',fan_speed:'rv_fs'};
var dirty=false, pollTimer=null;

function load(){
  if(dirty)return;
  fetch('/api/config').then(r=>r.json()).then(d=>{
    if(dirty)return;
    ids.forEach(k=>{
      var el=document.getElementById(k);if(!el||!(k in d))return;
      if(el.type==='checkbox')el.checked=(d[k]==='1'||d[k]===true||d[k]==='true');
      else el.value=d[k];
      if(rangeMap[k])document.getElementById(rangeMap[k]).textContent=d[k];
    });
  }).catch(()=>{});
}

function startPoll(){pollTimer=setInterval(load,5000);}
function markDirty(){dirty=true;if(pollTimer){clearInterval(pollTimer);pollTimer=null;}}

ids.forEach(k=>{
  var el=document.getElementById(k);if(!el)return;
  el.addEventListener('input',markDirty);
  el.addEventListener('change',markDirty);
});

Object.keys(rangeMap).forEach(k=>{
  var el=document.getElementById(k);
  if(el)el.addEventListener('input',()=>{document.getElementById(rangeMap[k]).textContent=el.value;});
});

document.getElementById('cfg').addEventListener('submit',function(e){
  e.preventDefault();
  var data={};
  ids.forEach(k=>{
    var el=document.getElementById(k);if(!el)return;
    data[k]=el.type==='checkbox'?(el.checked?'1':'0'):el.value;
  });
  var st=document.getElementById('st');
  fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)})
    .then(r=>{st.className='status '+(r.ok?'ok':'err');st.textContent=r.ok?'Saved!':'Error saving';
      if(r.ok){dirty=false;startPoll();}
    })
    .catch(()=>{st.className='status err';st.textContent='Connection failed';});
});

function doReboot(){
  if(!confirm('Reboot the device now?'))return;
  fetch('/api/reboot',{method:'POST'}).then(()=>{
    document.getElementById('st').className='status ok';
    document.getElementById('st').textContent='Rebooting...';
  }).catch(()=>{});
}

load();
startPoll();
</script>
</body></html>
)rawhtml";
        return page;
    }

    static const char* getUpdatePage() {
        static const char page[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SMAQ Update</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Segoe UI',system-ui,sans-serif;background:#1a1a2e;color:#e0e0e0;padding:20px}
.card{background:#16213e;border-radius:12px;padding:24px;margin:12px auto;max-width:480px;box-shadow:0 4px 20px rgba(0,0,0,.3)}
h1{text-align:center;color:#0af;margin-bottom:8px;font-size:1.5em}
.subtitle{text-align:center;color:#888;margin-bottom:20px;font-size:.85em}
.drop{border:2px dashed #2a3a5e;border-radius:12px;padding:40px 20px;text-align:center;cursor:pointer;transition:border-color .3s,background .3s}
.drop:hover,.drop.over{border-color:#0af;background:rgba(0,170,255,.05)}
.drop p{color:#888;margin-bottom:8px}
.drop .fname{color:#0af;font-weight:600;margin-top:8px}
input[type=file]{display:none}
.progress{display:none;margin-top:16px}
.bar-bg{background:#0d1b3e;border-radius:8px;overflow:hidden;height:24px}
.bar{height:100%;background:linear-gradient(90deg,#0af,#06d);border-radius:8px;transition:width .3s;width:0%;display:flex;align-items:center;justify-content:center;font-size:.8em;font-weight:600}
.btn{display:block;width:100%;padding:12px;background:linear-gradient(135deg,#0af,#06d);color:#fff;border:none;border-radius:8px;font-size:1em;font-weight:600;cursor:pointer;margin-top:16px;transition:opacity .2s}
.btn:hover{opacity:.85}
.btn:disabled{opacity:.4;cursor:default}
.btn-back{background:linear-gradient(135deg,#555,#333);margin-top:12px}
.status{text-align:center;font-size:.85em;margin-top:10px;min-height:1.2em}
.ok{color:#0f8}.err{color:#f44}.warn{color:#fa0}
</style>
</head>
<body>
<div class="card">
<h1>&#x2B06; Firmware Update</h1>
<p class="subtitle">Upload a .bin firmware file</p>

<div class="drop" id="drop" onclick="document.getElementById('file').click()">
<p>&#x1F4C1; Drop firmware file here or click to browse</p>
<p class="fname" id="fname"></p>
</div>
<input type="file" id="file" accept=".bin">

<div class="progress" id="progress">
<div class="bar-bg"><div class="bar" id="bar">0%</div></div>
</div>

<button class="btn" id="uploadBtn" disabled onclick="doUpload()">Upload Firmware</button>
<a href="/"><button class="btn btn-back" type="button">&larr; Back to Config</button></a>
<div class="status" id="st"></div>
</div>

<script>
var fileInput=document.getElementById('file'),drop=document.getElementById('drop'),
    fname=document.getElementById('fname'),btn=document.getElementById('uploadBtn'),
    bar=document.getElementById('bar'),prog=document.getElementById('progress'),
    st=document.getElementById('st'),selectedFile=null;

fileInput.addEventListener('change',function(){pickFile(this.files[0]);});
drop.addEventListener('dragover',function(e){e.preventDefault();drop.classList.add('over');});
drop.addEventListener('dragleave',function(){drop.classList.remove('over');});
drop.addEventListener('drop',function(e){e.preventDefault();drop.classList.remove('over');if(e.dataTransfer.files.length)pickFile(e.dataTransfer.files[0]);});

function pickFile(f){
  if(!f||!f.name.endsWith('.bin')){st.className='status err';st.textContent='Please select a .bin file';return;}
  selectedFile=f;fname.textContent=f.name+' ('+Math.round(f.size/1024)+' KB)';btn.disabled=false;
  st.className='status';st.textContent='';
}

function doUpload(){
  if(!selectedFile)return;
  btn.disabled=true;prog.style.display='block';
  st.className='status warn';st.textContent='Uploading... do not close this page!';
  var xhr=new XMLHttpRequest();
  xhr.open('POST','/api/update',true);
  xhr.upload.onprogress=function(e){if(e.lengthComputable){var p=Math.round(e.loaded/e.total*100);bar.style.width=p+'%';bar.textContent=p+'%';}};
  xhr.onload=function(){
    if(xhr.status===200){st.className='status ok';st.textContent='Update successful! Rebooting...';bar.style.width='100%';bar.textContent='Done!';}
    else{st.className='status err';st.textContent='Update failed: '+xhr.responseText;btn.disabled=false;}
  };
  xhr.onerror=function(){st.className='status err';st.textContent='Connection lost';btn.disabled=false;};
  var fd=new FormData();fd.append('firmware',selectedFile);
  xhr.send(fd);
}
</script>
</body></html>
)rawhtml";
        return page;
    }

public:
    WebConfig(uint16_t port = 80) : server(port) {}

    void begin(ConfigChangeCallback callback = nullptr) {
        on_config_changed = callback;

        server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
            request->send(200, "text/html", getConfigPage());
        });

        server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest* request) {
            auto& cm = ConfigManager::getInstance();
            StaticJsonDocument<768> doc;
            doc[cfg::keys::wifi_ssid]         = cm.getString(cfg::keys::wifi_ssid, cfg::defaults::wifi_ssid);
            doc[cfg::keys::wifi_pass]         = cm.getString(cfg::keys::wifi_pass, cfg::defaults::wifi_pass);
            doc[cfg::keys::mqtt_broker]       = cm.getString(cfg::keys::mqtt_broker, cfg::defaults::mqtt_broker);
            doc[cfg::keys::mqtt_port]         = cm.getInt(cfg::keys::mqtt_port, cfg::defaults::mqtt_port);
            doc[cfg::keys::mqtt_user]         = cm.getString(cfg::keys::mqtt_user, cfg::defaults::mqtt_user);
            doc[cfg::keys::mqtt_pass]         = cm.getString(cfg::keys::mqtt_pass, cfg::defaults::mqtt_pass);
            doc[cfg::keys::friendly_name]     = cm.getString(cfg::keys::friendly_name, cfg::defaults::friendly_name);
            doc[cfg::keys::host_name]         = cm.getString(cfg::keys::host_name, cfg::defaults::host_name);
            doc[cfg::keys::enable_display]    = cm.getBool(cfg::keys::enable_display, cfg::defaults::enable_display) ? "1" : "0";
            doc[cfg::keys::display_interval]  = cm.getInt(cfg::keys::display_interval, cfg::defaults::display_interval);
            doc[cfg::keys::report_interval]   = cm.getInt(cfg::keys::report_interval, cfg::defaults::report_interval);
            doc[cfg::keys::fan_speed]         = cm.getInt(cfg::keys::fan_speed, cfg::defaults::fan_speed);
            doc[cfg::keys::syslog_server_ip]  = cm.getString(cfg::keys::syslog_server_ip, cfg::defaults::syslog_server_ip);
            doc[cfg::keys::syslog_server_port] = cm.getInt(cfg::keys::syslog_server_port, cfg::defaults::syslog_server_port);

            std::string output;
            serializeJson(doc, output);
            request->send(200, "application/json", output.c_str());
        });

        server.on("/api/config", HTTP_POST,
            [](AsyncWebServerRequest* request) {},
            nullptr,
            [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
                StaticJsonDocument<768> doc;
                if (deserializeJson(doc, data, len) != DeserializationError::Ok) {
                    request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                    return;
                }

                auto& cm = ConfigManager::getInstance();

                auto putStr = [&](const char* key) {
                    if (doc.containsKey(key)) cm.putString(key, doc[key].as<std::string>());
                };
                auto putIntFromStr = [&](const char* key) {
                    if (doc.containsKey(key)) {
                        std::string val = doc[key].as<std::string>();
                        cm.putInt(key, atoi(val.c_str()));
                    }
                };

                putStr(cfg::keys::wifi_ssid);
                putStr(cfg::keys::wifi_pass);
                putStr(cfg::keys::mqtt_broker);
                putIntFromStr(cfg::keys::mqtt_port);
                putStr(cfg::keys::mqtt_user);
                putStr(cfg::keys::mqtt_pass);
                putStr(cfg::keys::friendly_name);
                putStr(cfg::keys::host_name);
                putStr(cfg::keys::syslog_server_ip);
                putIntFromStr(cfg::keys::syslog_server_port);

                if (doc.containsKey(cfg::keys::enable_display)) {
                    std::string val = doc[cfg::keys::enable_display].as<std::string>();
                    cm.putBool(cfg::keys::enable_display, val == "1" || val == "true");
                }
                putIntFromStr(cfg::keys::display_interval);
                putIntFromStr(cfg::keys::report_interval);
                putIntFromStr(cfg::keys::fan_speed);

                request->send(200, "application/json", "{\"ok\":true}");

                if (on_config_changed) on_config_changed();
            }
        );

        server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest* request) {
            request->send(200, "application/json", "{\"ok\":true}");
            delay(500);
            ESP.restart();
        });

        // ── Firmware Update Page ──
        server.on("/update", HTTP_GET, [](AsyncWebServerRequest* request) {
            request->send(200, "text/html", getUpdatePage());
        });

        server.on("/api/update", HTTP_POST,
            // Request complete handler
            [this](AsyncWebServerRequest* request) {
                bool success = !Update.hasError();
                request->send(success ? 200 : 500, "text/plain",
                              success ? "Update OK. Rebooting..." : "Update failed.");
                if (success) {
                    delay(500);
                    ESP.restart();
                }
            },
            // File upload handler (called per chunk)
            [this](AsyncWebServerRequest* request, const String& filename,
                   size_t index, uint8_t* data, size_t len, bool final) {
                if (index == 0) {
                    update_content_len = request->contentLength();
                    logger.log(Logger::Level::Info, "Web OTA start: %s, size: %u",
                               filename.c_str(), (uint32_t)update_content_len);
                    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
                        logger.log(Logger::Level::Error, "Update.begin failed");
                        Update.printError(Serial);
                    }
                }
                if (Update.isRunning()) {
                    if (Update.write(data, len) != len) {
                        logger.log(Logger::Level::Error, "Update.write failed");
                        Update.printError(Serial);
                    }
                }
                if (final) {
                    if (Update.end(true)) {
                        logger.log(Logger::Level::Info, "Web OTA complete");
                    } else {
                        logger.log(Logger::Level::Error, "Update.end failed");
                        Update.printError(Serial);
                    }
                }
            }
        );

        server.begin();
        logger.log(Logger::Level::Info, "WebConfig server started");
    }

    void setupCaptivePortal(const std::string& apName) {
        ap_mode = true;
        WiFi.softAP(apName.c_str());
        logger.log(Logger::Level::Info, "AP started: %s, IP: %s", apName.c_str(),
                   WiFi.softAPIP().toString().c_str());
    }

    bool isApMode() const { return ap_mode; }

    void stop() {
        server.end();
    }

    void restart() {
        server.begin();
    }
};
