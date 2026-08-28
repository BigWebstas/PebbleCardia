/**
 * Cardia -> Google Sheet sync.
 *
 * Appends every heart-rate/HRV sample and every rhythm episode the Cardia
 * watchapp sends from the phone into two tabs of a Google Sheet.
 *
 * SETUP
 *  1. Make a new Google Sheet.  Extensions -> Apps Script.
 *  2. Delete the stub code, paste this whole file, Save.
 *  3. Run the `setup` function once (pick it in the toolbar, click Run,
 *     authorise when prompted).  It creates the "Samples" and "Episodes" tabs.
 *  4. Deploy -> New deployment -> gear icon -> Web app.
 *        Description:      cardia
 *        Execute as:       Me
 *        Who has access:   Anyone
 *     Deploy, copy the Web app URL (ends in /exec).
 *  5. In the Cardia config page (Pebble app -> Cardia -> the gear/settings
 *     icon) paste that URL into "Live sync URL" and tap "Save & close".
 *
 *  Re-deploy (Manage deployments -> edit -> new version) if you change this
 *  script; the /exec URL stays the same.
 *
 * The watch posts, as text, either:
 *   {"kind":"sample","v":{"t":<unix s>,"bpm":..,"status":"..","rmssd":..,
 *                         "pnn50":..,"sdnn":..,"motion":..,"steps":<today>}}
 *   {"kind":"episode","v":{"start":<unix s>,"startISO":"..","type":"..",
 *                          "duration_s":..,"hr_min":..,"hr_peak":..,"score":..}}
 */

var SAMPLE_HEADERS  = ['unix', 'time', 'bpm', 'status', 'rmssd_ms', 'pnn50_pct',
                       'sdnn_ms', 'steps_today', 'motion'];
var EPISODE_HEADERS = ['unix', 'time', 'type', 'duration_s', 'hr_min', 'hr_peak', 'score'];
var TZ = Session.getScriptTimeZone();

function setup() {
  tab_('Samples', SAMPLE_HEADERS);
  tab_('Episodes', EPISODE_HEADERS);
}

function doGet() {
  return json_({ ok: true, service: 'cardia-sheet-sync' });
}

function doPost(e) {
  if (!e || !e.postData || !e.postData.contents) {
    return json_({ ok: false, error: 'no body' });
  }
  var lock = LockService.getScriptLock();
  try {
    lock.waitLock(15000);
    var body = JSON.parse(e.postData.contents);
    var v = body.v || {};

    if (body.kind === 'sample') {
      tab_('Samples', SAMPLE_HEADERS).appendRow([
        v.t, when_(v.t), num_(v.bpm), v.status || '',
        num_(v.rmssd), num_(v.pnn50), num_(v.sdnn), num_(v.steps), num_(v.motion)
      ]);
    } else if (body.kind === 'episode') {
      tab_('Episodes', EPISODE_HEADERS).appendRow([
        v.start, when_(v.start), v.type || '',
        num_(v.duration_s), num_(v.hr_min), num_(v.hr_peak), num_(v.score)
      ]);
    } else {
      return json_({ ok: false, error: 'unknown kind: ' + body.kind });
    }
    return json_({ ok: true });
  } catch (err) {
    return json_({ ok: false, error: String(err) });
  } finally {
    lock.releaseLock();
  }
}

function tab_(name, headers) {
  var ss = SpreadsheetApp.getActiveSpreadsheet();
  var sh = ss.getSheetByName(name);
  if (!sh) {
    sh = ss.insertSheet(name);
    sh.appendRow(headers);
    sh.setFrozenRows(1);
    sh.getRange(1, 1, 1, headers.length).setFontWeight('bold');
  }
  return sh;
}

function when_(unixSec) {
  if (!unixSec) return '';
  return Utilities.formatDate(new Date(unixSec * 1000), TZ, 'yyyy-MM-dd HH:mm:ss');
}

function num_(x) { return (x === undefined || x === null) ? '' : Number(x); }

function json_(obj) {
  return ContentService.createTextOutput(JSON.stringify(obj))
    .setMimeType(ContentService.MimeType.JSON);
}
