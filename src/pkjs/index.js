// Cardia - phone side.
//
// Keeps a rolling log of heart-rate/HRV samples and rhythm episodes from the
// watch in localStorage. Can stream each record to a URL you configure, and
// builds a Markdown report (with Mermaid charts) you can save or share from the
// configuration page.

var STATUS = 0;
var EPISODE = 1;
var RHYTHM = ['No signal', 'Normal', 'Elevated', 'Low', 'Irregular'];

var MAX_SAMPLES = 1500;   // ~6 h at one sample / 15 s
var MAX_EPISODES = 200;

// --- storage --------------------------------------------------------------
function load(key, dflt) {
  try { return JSON.parse(localStorage.getItem(key)) || dflt; }
  catch (e) { return dflt; }
}
function save(key, val) {
  try { localStorage.setItem(key, JSON.stringify(val)); } catch (e) {}
}
function getSamples() { return load('samples', []); }
function getEpisodes() { return load('episodes', []); }
function getConfig() { return load('config', {}); }

// --- cloud streaming ----------------------------------------------------
function stream(record) {
  var url = getConfig().syncUrl;
  if (!url) return;
  try {
    var xhr = new XMLHttpRequest();
    xhr.open('POST', url, true);
    // text/plain avoids a CORS pre-flight and is what Google Apps Script
    // web apps accept; the body is still JSON (see tools/cardia-sheet-sync.gs).
    xhr.setRequestHeader('Content-Type', 'text/plain;charset=utf-8');
    xhr.onerror = function () { console.log('sync POST failed'); };
    xhr.send(JSON.stringify(record));
  } catch (e) { console.log('sync error ' + e); }
}

// --- incoming watch messages ------------------------------------------
Pebble.addEventListener('ready', function () {
  console.log('Cardia JS ready. samples=' + getSamples().length +
              ' episodes=' + getEpisodes().length);
  // Ask the watch to replay stored episodes (incl. ones the background worker
  // recorded while the app was closed).
  Pebble.sendAppMessage({ DUMP: 1 });
});

Pebble.addEventListener('appmessage', function (e) {
  var d = e.payload;

  if (d.MSG_KIND === STATUS) {
    var s = {
      t: Math.round(Date.now() / 1000),
      bpm: d.BPM || 0,
      status: RHYTHM[d.STATUS] || '?',
      rmssd: d.RMSSD || 0,
      pnn50: d.PNN50 || 0,
      sdnn: d.SDNN || 0,
      motion: d.MOTION || 0,
      steps: d.STEPS || 0        // cumulative steps today (resets at midnight)
    };
    console.log('[sample] ' + s.bpm + 'bpm ' + s.status + ' rmssd=' + s.rmssd +
                ' pnn50=' + s.pnn50 + ' steps=' + s.steps + ' mot=' + s.motion);
    var samples = getSamples();
    samples.push(s);
    while (samples.length > MAX_SAMPLES) samples.shift();
    save('samples', samples);
    stream({ kind: 'sample', v: s });
    return;
  }

  if (d.MSG_KIND === EPISODE) {
    var ep = {
      start: d.EP_START,
      startISO: new Date(d.EP_START * 1000).toISOString(),
      duration_s: d.EP_DURATION || 0,
      ongoing: !!d.EP_ONGOING,
      type: RHYTHM[d.STATUS] || '?',
      hr_min: d.EP_HR_MIN || 0,
      hr_peak: d.EP_HR_PEAK || 0,
      score: d.EP_SCORE || 0
    };
    console.log('[episode] ' + JSON.stringify(ep));
    if (ep.ongoing || !ep.start) return;

    var eps = getEpisodes();
    var found = false;
    for (var i = 0; i < eps.length; i++) {
      if (eps[i].start === ep.start) { eps[i] = ep; found = true; break; }
    }
    if (!found) eps.push(ep);
    eps.sort(function (a, b) { return a.start - b.start; });
    while (eps.length > MAX_EPISODES) eps.shift();
    save('episodes', eps);
    if (!found) stream({ kind: 'episode', v: ep });
  }
});

// --- report generation -------------------------------------------------
function pad(n) { return (n < 10 ? '0' : '') + n; }
function hhmm(sec) {
  var dt = new Date(sec * 1000);
  return pad(dt.getHours()) + ':' + pad(dt.getMinutes());
}
// local "YYYY-MM-DDTHH:mm:ss" for Mermaid gantt (its dateFormat X / unix
// handling is unreliable, so feed it a parsed local datetime instead).
function localDT(sec) {
  var d = new Date(sec * 1000);
  return d.getFullYear() + '-' + pad(d.getMonth() + 1) + '-' + pad(d.getDate()) +
         'T' + pad(d.getHours()) + ':' + pad(d.getMinutes()) + ':' + pad(d.getSeconds());
}
function dur(sec) {
  var m = Math.floor(sec / 60), s = sec % 60;
  return m + ':' + pad(s);
}

// evenly pick <=count entries, averaging each bucket; nulls where no signal
function downsample(samples, field, count) {
  if (!samples.length) return [];
  var out = [];
  var per = samples.length / count;
  for (var b = 0; b < count; b++) {
    var lo = Math.floor(b * per), hi = Math.floor((b + 1) * per);
    if (hi <= lo) hi = lo + 1;
    var sum = 0, n = 0;
    for (var i = lo; i < hi && i < samples.length; i++) {
      var v = samples[i][field];
      if (v > 0) { sum += v; n++; }
    }
    out.push(n ? Math.round(sum / n) : null);
  }
  return out;
}

function fillNulls(arr) {
  var last = 0;
  return arr.map(function (v) { if (v === null) return last; last = v; return v; });
}

// per-sample step counts from the cumulative "steps today" field, handling the
// midnight reset. Returns an array aligned with `samples` (first entry 0).
function stepDeltas(samples) {
  var out = [0];
  for (var i = 1; i < samples.length; i++) {
    var d = samples[i].steps - samples[i - 1].steps;
    if (d < 0) d = samples[i].steps;   // counter reset at midnight
    out.push(d);
  }
  return out;
}

// mermaid bar chart (x is elapsed minutes 0..span)
function barchart(title, yLabel, spanMin, series) {
  var hi = Math.max.apply(null, series.concat([1]));
  hi = Math.ceil((hi * 1.1) / 5) * 5 || 5;
  return '```mermaid\n' + MM_INIT + 'xychart-beta\n' +
    '    title "' + title + '"\n' +
    '    x-axis "minutes" 0 --> ' + Math.max(1, Math.round(spanMin)) + '\n' +
    '    y-axis "' + yLabel + '" 0 --> ' + hi + '\n' +
    '    bar [' + series.join(', ') + ']\n```\n';
}

var MM_INIT = '%%{init: {"themeVariables": {"xyChart": {"plotColorPalette": "#c0392b,#2b6cb0"}}}}%%\n';

function xychart(title, yLabel, spanMin, series, forceLo, forceHi) {
  var vals = series.filter(function (v) { return v !== null; });
  if (!vals.length) return '';
  var lo = forceLo !== undefined ? forceLo : Math.floor((Math.min.apply(null, vals) - 5) / 5) * 5;
  var hi = forceHi !== undefined ? forceHi : Math.ceil((Math.max.apply(null, vals) + 5) / 5) * 5;
  if (lo < 0) lo = 0;
  if (hi <= lo) hi = lo + 10;
  return '```mermaid\n' + MM_INIT + 'xychart-beta\n' +
    '    title "' + title + '"\n' +
    '    x-axis "minutes" 0 --> ' + Math.max(1, Math.round(spanMin)) + '\n' +
    '    y-axis "' + yLabel + '" ' + lo + ' --> ' + hi + '\n' +
    '    line [' + fillNulls(series).join(', ') + ']\n```\n';
}

function buildMarkdown() {
  var samples = getSamples();
  var eps = getEpisodes();
  var now = new Date();
  var md = '# Cardia — heart report\n\n';
  md += '_Generated ' + now.toISOString().replace('T', ' ').slice(0, 16) + '_\n\n';

  if (samples.length) {
    var t0 = samples[0].t, t1 = samples[samples.length - 1].t;
    var spanMin = (t1 - t0) / 60;
    md += 'Window: **' + hhmm(t0) + ' – ' + hhmm(t1) + '**  ·  ' +
          samples.length + ' samples  ·  ~' + Math.round(spanMin) + ' min\n\n';

    var bpms = samples.map(function (s) { return s.bpm; }).filter(function (v) { return v > 0; });
    if (bpms.length) {
      md += 'Heart rate: min **' + Math.min.apply(null, bpms) + '**, ' +
            'avg **' + Math.round(bpms.reduce(function (a, b) { return a + b; }, 0) / bpms.length) + '**, ' +
            'max **' + Math.max.apply(null, bpms) + '** bpm\n\n';
    }

    var totalSteps = stepDeltas(samples).reduce(function (a, b) { return a + b; }, 0);
    if (totalSteps > 0) {
      md += 'Steps: **' + totalSteps + '** during the window' +
            ' (' + samples[samples.length - 1].steps + ' so far today)\n\n';
    }
  } else {
    md += '_No samples recorded yet — open the Cardia app on the watch for a while._\n\n';
  }

  var byType = {};
  eps.forEach(function (e) { byType[e.type] = (byType[e.type] || 0) + 1; });
  md += 'Episodes: **' + eps.length + '**';
  var parts = Object.keys(byType).map(function (k) { return k + ' ' + byType[k]; });
  if (parts.length) md += '  (' + parts.join(', ') + ')';
  md += '\n\n';

  md += '> **Not a medical device.** The optical sensor is not an ECG and cannot ' +
        'diagnose atrial fibrillation or anything else. Discuss repeated flags ' +
        'with a clinician.\n\n';

  if (samples.length > 2) {
    var span = (samples[samples.length - 1].t - samples[0].t) / 60;
    var N = Math.min(120, samples.length);
    md += '## Heart rate\n\n' +
          xychart('Heart rate (bpm)', 'bpm', span, downsample(samples, 'bpm', N), 40, undefined);
    md += '## Beat-to-beat variability (RMSSD)\n\n' +
          xychart('RMSSD (ms)', 'ms', span, downsample(samples, 'rmssd', N), 0, undefined);
    md += '## pNN50\n\n' +
          xychart('pNN50 (%)', '%', span, downsample(samples, 'pnn50', N), 0, 100);

    // steps: bucket per-sample deltas into ~N bars across the window
    var deltas = stepDeltas(samples);
    if (deltas.reduce(function (a, b) { return a + b; }, 0) > 0) {
      var bars = [], per = samples.length / N;
      for (var b = 0; b < N; b++) {
        var lo = Math.floor(b * per), hi = Math.floor((b + 1) * per);
        if (hi <= lo) hi = lo + 1;
        var sum = 0;
        for (var i = lo; i < hi && i < deltas.length; i++) sum += deltas[i];
        bars.push(sum);
      }
      md += '## Steps\n\n' + barchart('Steps per interval', 'steps', span, bars);
    }
  }

  if (eps.length) {
    // Pick an axis format (and tick spacing) from how much time the episodes
    // span, so short spans don't render every tick as the same minute.
    var gFirst = eps[0].start, gLast = 0;
    eps.forEach(function (e) { gLast = Math.max(gLast, e.start + Math.max(1, e.duration_s)); });
    var gSpan = Math.max(1, gLast - gFirst);
    var axisFmt, tick;
    if (gSpan < 900)        { axisFmt = '%H:%M:%S'; tick = Math.max(5, Math.round(gSpan / 6)) + 'second'; }
    else if (gSpan < 86400) { axisFmt = '%H:%M';    tick = Math.max(1, Math.round(gSpan / 360)) + 'minute'; }
    else                    { axisFmt = '%m-%d %H:%M'; tick = Math.max(1, Math.round(gSpan / 8 / 3600)) + 'hour'; }

    md += '## Episodes\n\n```mermaid\ngantt\n    title Episodes\n' +
          '    dateFormat YYYY-MM-DDTHH:mm:ss\n    axisFormat ' + axisFmt +
          '\n    tickInterval ' + tick + '\n    section Rhythm\n';
    eps.forEach(function (e) {
      var d = Math.max(1, e.duration_s);
      var label = (e.type + (e.score ? ' (' + e.score + ')' : '')).replace(/[:,]/g, ' ');
      md += '    ' + label + ' :' + localDT(e.start) + ', ' + d + 's\n';
    });
    md += '```\n\n';

    if (parts.length > 1) {
      md += '```mermaid\npie showData title Episode types\n';
      Object.keys(byType).forEach(function (k) {
        md += '    "' + k + '" : ' + byType[k] + '\n';
      });
      md += '```\n\n';
    }

    md += '| When | Type | Duration | HR range | Score |\n';
    md += '|---|---|---|---|---|\n';
    eps.slice().reverse().forEach(function (e) {
      var dt = new Date(e.start * 1000);
      md += '| ' + (dt.getMonth() + 1) + '/' + dt.getDate() + ' ' +
            pad(dt.getHours()) + ':' + pad(dt.getMinutes()) + ' | ' +
            e.type + ' | ' + dur(e.duration_s) + ' | ' +
            e.hr_min + '–' + e.hr_peak + ' | ' + e.score + ' |\n';
    });
    md += '\n';
  }

  // 1-minute table
  if (samples.length > 4) {
    var sd = stepDeltas(samples);
    md += '<details>\n<summary>Minute-by-minute data</summary>\n\n';
    md += '| Time | bpm | RMSSD | pNN50 | steps | motion | status |\n' +
          '|---|---|---|---|---|---|---|\n';
    var bucket = null, rows = [];
    samples.forEach(function (s, idx) {
      var key = Math.floor(s.t / 60);
      if (!bucket || bucket.key !== key) {
        if (bucket) rows.push(bucket);
        bucket = { key: key, t: s.t, bpm: [], rmssd: [], pnn50: [], motion: [], steps: 0, status: s.status };
      }
      if (s.bpm > 0) bucket.bpm.push(s.bpm);
      bucket.rmssd.push(s.rmssd); bucket.pnn50.push(s.pnn50);
      bucket.motion.push(s.motion);
      bucket.steps += sd[idx];
      bucket.status = s.status;
    });
    if (bucket) rows.push(bucket);
    function avg(a) { return a.length ? Math.round(a.reduce(function (x, y) { return x + y; }, 0) / a.length) : 0; }
    rows.forEach(function (r) {
      md += '| ' + hhmm(r.t) + ' | ' + (avg(r.bpm) || '—') + ' | ' + avg(r.rmssd) +
            ' | ' + avg(r.pnn50) + ' | ' + r.steps +
            ' | ' + avg(r.motion) + ' | ' + r.status + ' |\n';
    });
    md += '\n</details>\n';
  }

  return md;
}

// --- configuration page ----------------------------------------------
Pebble.addEventListener('showConfiguration', function () {
  var md = buildMarkdown();
  var jsonData = JSON.stringify({
    generated: new Date().toISOString(),
    samples: getSamples(),
    episodes: getEpisodes()
  }, null, 1);
  var cfg = getConfig();

  var page =
'<!doctype html><html><head><meta charset="utf-8">' +
'<meta name="viewport" content="width=device-width,initial-scale=1">' +
'<title>Cardia report</title>' +
'<script src="https://cdn.jsdelivr.net/npm/marked/marked.min.js"></script>' +
'<script src="https://cdn.jsdelivr.net/npm/mermaid@11/dist/mermaid.min.js"></script>' +
'<style>' +
'body{font:15px/1.5 -apple-system,Roboto,sans-serif;margin:0;padding:16px;max-width:820px}' +
'h1{font-size:20px}h2{font-size:16px;margin-top:24px}' +
'button,input{font:14px inherit;padding:9px 12px;margin:4px 4px 4px 0;border:1px solid #999;border-radius:8px;background:#fff}' +
'button{background:#2b6cb0;color:#fff;border-color:#2b6cb0}' +
'input{min-width:60%}table{border-collapse:collapse}td,th{border:1px solid #ccc;padding:3px 8px}' +
'#raw{width:100%;height:180px;font:12px monospace}.mermaid{overflow-x:auto}' +
'.bar{position:sticky;top:0;background:#fff;padding:8px 0;border-bottom:1px solid #eee}' +
'label{display:block;margin:10px 0 2px;font-weight:bold}.hint{color:#666;font-size:13px}' +
'</style></head><body>' +
'<div class="bar">' +
'<button onclick="save(\'md\')">Download .md</button>' +
'<button onclick="save(\'json\')">Download .json</button>' +
'<button onclick="share()">Share report</button>' +
'<button onclick="copyMd()">Copy</button>' +
'<button onclick="clearData()" style="background:#c53030;border-color:#c53030">Clear data</button>' +
'</div>' +
'<label>Export file name</label>' +
'<input id="prefix" placeholder="cardia"> <span class="hint">&rarr; ' +
'<span id="egname">cardia</span>-report-&lt;date&gt;.md</span>' +
'<p class="hint">A web page can\'t choose a save <i>folder</i> on Android — ' +
'downloads go to your Downloads folder. Use <b>Share report</b> &rarr; ' +
'<b>Save to Files</b> to put it in a specific folder (the picker remembers ' +
'the last one you used).</p>' +
'<label>Live sync URL (optional)</label>' +
'<input id="url" placeholder="https://script.google.com/…/exec">' +
'<p class="hint">Every sample &amp; episode is POSTed here as JSON. Ready-made ' +
'Google Apps Script &rarr; Sheet: <code>tools/cardia-sheet-sync.gs</code>.</p>' +
'<button onclick="done()">Save settings &amp; close</button>' +
'<div id="report">Rendering…</div><h2>Raw Markdown</h2><textarea id="raw"></textarea>' +
'<script>' +
'var MD=' + JSON.stringify(md) + ';' +
'var JD=' + JSON.stringify(jsonData) + ';' +
'var CFG=' + JSON.stringify(cfg) + ';' +
'document.getElementById("raw").value=MD;' +
'document.getElementById("url").value=CFG.syncUrl||"";' +
'document.getElementById("prefix").value=CFG.filePrefix||"cardia";' +
'document.getElementById("prefix").oninput=function(){' +
'  document.getElementById("egname").textContent=(this.value.trim()||"cardia");};' +
'try{' +
'  mermaid.initialize({startOnLoad:false,theme:"neutral"});' +
'  var html=marked.parse(MD);' +
'  html=html.replace(/<pre><code class="language-mermaid[^"]*">([\\s\\S]*?)<\\/code><\\/pre>/g,' +
'    function(_,c){return \'<pre class="mermaid">\'+c.replace(/&lt;/g,"<").replace(/&gt;/g,">").replace(/&amp;/g,"&").replace(/&quot;/g,\'"\')+\'</pre>\';});' +
'  document.getElementById("report").innerHTML=html;' +
'  mermaid.run();' +
'}catch(e){document.getElementById("report").textContent="(preview failed: "+e+")";}' +
'function fname(kind){' +
'  var p=(document.getElementById("prefix").value.trim()||"cardia").replace(/[^A-Za-z0-9_-]+/g,"-");' +
'  var d=new Date(),z=function(n){return(n<10?"0":"")+n;};' +
'  var stamp=d.getFullYear()+z(d.getMonth()+1)+z(d.getDate())+"-"+z(d.getHours())+z(d.getMinutes());' +
'  return kind==="json"?p+"-data-"+stamp+".json":p+"-report-"+stamp+".md";' +
'}' +
'function payload(kind){return kind==="json"?[JD,"application/json"]:[MD,"text/markdown"];}' +
'function save(kind){' +
'  var n=fname(kind),x=payload(kind);' +
'  try{' +
'    var b=new Blob([x[0]],{type:x[1]}),a=document.createElement("a");' +
'    a.href=URL.createObjectURL(b);a.download=n;document.body.appendChild(a);a.click();a.remove();' +
'  }catch(e){location.href="data:"+x[1]+";base64,"+btoa(unescape(encodeURIComponent(x[0])));}' +
'}' +
'function share(){' +
'  var n=fname("md");' +
'  try{' +
'    var f=new File([MD],n,{type:"text/markdown"});' +
'    if(navigator.canShare&&navigator.canShare({files:[f]})){navigator.share({files:[f],title:n});return;}' +
'  }catch(e){}' +
'  try{navigator.share({title:n,text:MD});}catch(e){save("md");}' +
'}' +
'function copyMd(){var r=document.getElementById("raw");r.select();try{document.execCommand("copy");}catch(e){}}' +
'function clearData(){if(confirm("Delete all stored samples and episodes on the phone?"))' +
'  location.href="pebblejs://close#"+encodeURIComponent(JSON.stringify({clear:1}));}' +
'function done(){location.href="pebblejs://close#"+encodeURIComponent(JSON.stringify({' +
'  syncUrl:document.getElementById("url").value.trim(),' +
'  filePrefix:document.getElementById("prefix").value.trim()}));}' +
'</script></body></html>';

  Pebble.openURL('data:text/html;charset=utf-8;base64,' +
    btoa(unescape(encodeURIComponent(page))));
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response) return;
  var r;
  try { r = JSON.parse(decodeURIComponent(e.response)); } catch (x) { return; }
  if (r.clear) {
    localStorage.removeItem('samples');
    localStorage.removeItem('episodes');
    console.log('Cardia data cleared');
    return;
  }
  var cfg = getConfig();
  if ('syncUrl' in r) cfg.syncUrl = r.syncUrl || '';
  if ('filePrefix' in r) cfg.filePrefix = r.filePrefix || '';
  save('config', cfg);
  console.log('config saved: syncUrl ' + (cfg.syncUrl ? 'set' : 'off') +
              ', filePrefix "' + (cfg.filePrefix || 'cardia') + '"');
});
