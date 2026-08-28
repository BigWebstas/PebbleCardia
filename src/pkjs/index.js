// Cardia - phone side.
// Receives monitoring heartbeats and finalised episodes from the watch,
// keeps a rolling episode log in localStorage, and logs everything to the
// JS console so it can be pulled with `pebble logs` / the CLI.

var STATUS = 0;
var EPISODE = 1;

var RHYTHM = ['No signal', 'Normal', 'Elevated', 'Low', 'Irregular'];

function loadLog() {
  try { return JSON.parse(localStorage.getItem('episodes') || '[]'); }
  catch (e) { return []; }
}

function saveLog(log) {
  while (log.length > 100) log.shift();
  try { localStorage.setItem('episodes', JSON.stringify(log)); } catch (e) {}
}

Pebble.addEventListener('ready', function () {
  console.log('Cardia JS ready. Stored episodes: ' + loadLog().length);
});

Pebble.addEventListener('appmessage', function (e) {
  var d = e.payload;
  if (d.MSG_KIND === STATUS) {
    console.log('[status] ' + (d.BPM || '--') + ' bpm  ' +
      (RHYTHM[d.STATUS] || '?') +
      '  RMSSD=' + d.RMSSD + 'ms  pNN50=' + d.PNN50 + '%  SDNN=' + d.SDNN + 'ms' +
      '  motion=' + d.MOTION);
    return;
  }
  if (d.MSG_KIND === EPISODE) {
    var ep = {
      start: d.EP_START,
      startISO: new Date(d.EP_START * 1000).toISOString(),
      duration_s: d.EP_DURATION,
      ongoing: !!d.EP_ONGOING,
      type: RHYTHM[d.STATUS] || '?',
      hr_min: d.EP_HR_MIN,
      hr_peak: d.EP_HR_PEAK,
      score: d.EP_SCORE
    };
    console.log('[episode] ' + JSON.stringify(ep));
    if (!ep.ongoing) {
      var log = loadLog();
      log.push(ep);
      saveLog(log);
    }
  }
});

// Simple config page: shows the stored episode log as downloadable JSON.
Pebble.addEventListener('showConfiguration', function () {
  var log = loadLog();
  var html =
    '<!doctype html><meta name=viewport content="width=device-width">' +
    '<body style="font-family:sans-serif;padding:16px">' +
    '<h3>Cardia episode log</h3><p>' + log.length + ' episodes stored.</p>' +
    '<textarea style="width:100%;height:60vh">' +
    JSON.stringify(log, null, 2) + '</textarea>' +
    '<p><a href="pebblejs://close#">Done</a></p>';
  Pebble.openURL('data:text/html,' + encodeURIComponent(html));
});
