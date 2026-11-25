/*===================================================================================================================*/
/* the web page of the thermostat                                                                            */
/*===================================================================================================================*/
/* The status and configuration page of the device, streamed in chunks with the core's chunked transfer encoding:
   every row is formatted into a small stack buffer with snprintf and written straight to the client, so there is
   no page String in the heap (no 'Reallocating large String' reallocs, no fragmentation of the DRAM heap), no
   operator+ / concat / changeBuffer frames on the cont stack that runs loop() ('Stack smashing detected' reported
   by cont_check), and each row checks its own length so a row that does not fit is reported by key instead of
   silently truncating.

   The page opens with a dashboard window that shows the live sensor and thermostat values (current temperature,
   current humidity, target temperature, heating state), followed by the configuration windows and the console
   output window of print_log.h as the last window at the bottom; the command console_clear that empties it is
   handled in main.cpp.

   The page is rebuilt from the current configuration on every request, which is what makes POST work: the browser
   sends the changed values back to the same URL. The style is one flash literal with a light and a dark palette
   (prefers-color-scheme), rounded cards and the platform UI font, so the page follows the system appearance of the
   phone or desktop that opens it. */

/*===================================================================================================================*/
/* includes                                                                                                   */
/*===================================================================================================================*/
#include "website.h"
#include "print_log.h"  /* last include: renames the identifier Serial to the console capture tee, see print_log.h */
#include <inttypes.h>   /* PRIu32 / PRId32 for the page's format rows */

/*===================================================================================================================*/
/* page resources: the static page HTML, as flash literals                                                  */
/*===================================================================================================================*/
/* The literals sit in the text section, exactly where the page used to keep them unnamed. Named, the repeating
   fragments share one literal (the POST forms, the table closes, the two restart buttons) and the style of
   the page is one literal the compiler concatenates, so the style costs one chunk of the chunked response
   instead of many. The bytes the page sends do not change with the number of literals. */

static const char htmlHeadDoctype[] PROGMEM = "<!DOCTYPE html> <html>\n";
static const char htmlHeadMeta[] PROGMEM =
    "<head><meta charset='utf-8'>\n"
    "<meta name='viewport' content='width=device-width, initial-scale=1.0, viewport-fit=cover'>\n"
    "<meta name='theme-color' content='#f2f2f7' media='(prefers-color-scheme: light)'>\n"
    "<meta name='theme-color' content='#000000' media='(prefers-color-scheme: dark)'>\n";
static const char htmlHeadClose[] PROGMEM = "</head>\n";
static const char htmlBodyOpen[] PROGMEM = "<body>\n<div class='wrap'>\n";
static const char htmlTableOpen[] PROGMEM = "<table>";
static const char htmlTableClose[] PROGMEM = "</table>";
static const char htmlCommandsTableOpen[] PROGMEM = "<div class='tablewrap'><table class='cmd'>";
static const char htmlCommandsTableClose[] PROGMEM = "</table></div>";
static const char htmlFormOpen[] PROGMEM = "<form method='POST' action='/' autocomplete='off'>";
static const char htmlFormClose[] PROGMEM = "</form>";
static const char htmlRestartButton[] PROGMEM = "<button class='plain' onclick=\"window.location.href='/restart'\">Restart</button>";
static const char htmlRestartWaitingButton[] PROGMEM = "<button class='plain' onclick=\"window.location.href='/'\">Restarting, reload site</button>";
static const char htmlThermostatStepForm[] PROGMEM =
    "<form method='POST' action='/' autocomplete='off' class='row live'>"
    "<button type='submit' name='TargetStep' value='-5' class='round' aria-label='Lower target temperature'>&minus;</button>"
    "<button type='submit' name='TargetStep' value='5' class='round' aria-label='Raise target temperature'>+</button>"
    "</form>";
/* the live update: the tab pulls /api/state every 5 s and patches the dashboard nodes instead of reloading
   the page, and the +/- and Heat/Off forms post through fetch so a press neither reloads nor jumps to the top.
   Without JavaScript (or without fetch/URLSearchParams) the plain forms work as before: POST, 303, fresh page. */
static const char htmlLiveScript[] PROGMEM =
    "<script>\n"
    "(function(){\n"
    "if(!window.fetch||typeof URLSearchParams==='undefined'||!document.getElementById){return;}\n"
    "function id(n){return document.getElementById(n);}\n"
    "function text(n,v){var e=id(n);if(e){e.textContent=v;}}\n"
    "function fmt(v){return v===null?'\\u2014':Number(v).toFixed(1);}\n"
    "var lastLog=-1;\n"
    "function pullConsole(total){\n"
    "if(total===lastLog){return;}\n"
    "lastLog=total;\n"
    "var x=new XMLHttpRequest();\n"
    "x.open('GET','/api/console',true);\n"
    "x.onreadystatechange=function(){\n"
    "if(x.readyState!==4){return;}\n"
    "if(x.status!==200){lastLog=-1;return;}\n"
    "var e=id('v-console');\n"
    "if(!e){return;}\n"
    "var atEnd=(e.scrollHeight-e.scrollTop-e.clientHeight)<24;\n"
    "e.innerHTML=x.responseText;\n"
    "if(atEnd){e.scrollTop=e.scrollHeight;}\n"
    "};\n"
    "x.send();\n"
    "}\n"
    "function apply(s){\n"
    "var heat=!!s.heat;\n"
    "text('v-target',fmt(s.target));\n"
    "text('v-current',fmt(s.current));\n"
    "text('v-humidity',fmt(s.humidity));\n"
    "text('v-mode',heat?'Heat':'Off');\n"
    "var d=id('v-dial');\n"
    "if(d){d.style.setProperty('--p',s.angle+'deg');d.className=heat?'dial':'dial off';}\n"
    "var bh=id('b-heat');\n"
    "if(bh){bh.className=heat?'active heat':'';}\n"
    "var bo=id('b-off');\n"
    "if(bo){bo.className=heat?'':'active off';}\n"
    "var dot=id('v-dot');\n"
    "if(dot){dot.className='dot '+(s.relay?'heat':(heat?'ok':'off'));}\n"
    "text('v-state',s.relay?'Heating':(heat?'Idle':'Off'));\n"
    "text('v-relay',s.relay?'Relay on':(heat?'Relay off':'Thermostat off'));\n"
    "text('v-lines-total',s.log);\n"
    "pullConsole(s.log);\n"
    "var sb=id('v-sensor-bad'),sw=id('v-sensor-wait'),box=id('v-sensor');\n"
    "var err=!!s.sensor,wait=(!err&&s.current===null);\n"
    "if(sb){sb.style.display=err?'inline-block':'none';}\n"
    "if(sw){sw.style.display=wait?'inline-block':'none';}\n"
    "if(box){box.style.display=(err||wait)?'block':'none';}\n"
    "}\n"
    "function refresh(){\n"
    "var x=new XMLHttpRequest();\n"
    "x.open('GET','/api/state',true);\n"
    "x.onreadystatechange=function(){\n"
    "if(x.readyState!==4||x.status!==200){return;}\n"
    "try{apply(JSON.parse(x.responseText));}catch(e){}\n"
    "};\n"
    "x.send();\n"
    "}\n"
    "function send(f,submitter){\n"
    "var body=new URLSearchParams();\n"
    "for(var i=0;i<f.elements.length;i++){var el=f.elements[i];\n"
    "if(!el.name||el.disabled||(el.type==='submit'&&el!==submitter)){continue;}\n"
    "body.append(el.name,el.value);}\n"
    "fetch('/',{method:'POST',body:body,redirect:'manual'}).then(function(){setTimeout(refresh,120);},function(){setTimeout(refresh,120);});\n"
    "}\n"
    "document.addEventListener('submit',function(ev){\n"
    "var f=ev.target,sub=ev.submitter;\n"
    "if(!f||!sub||typeof f.className!=='string'||(' '+f.className+' ').indexOf(' live ')<0){return;}\n"
    "ev.preventDefault();\n"
    "send(f,sub);\n"
    "},true);\n"
    "setInterval(refresh,5000);\n"
    "document.addEventListener('visibilitychange',function(){if(!document.hidden){refresh();}});\n"
    "refresh();\n"
    "})();\n"
    "</script>\n";

static const char htmlPageFooter[] PROGMEM = "</div>\n</body>\n</html>\n";

/* the whole look of the page in one literal: a light palette and a dark palette, cards, pills, forms and the
   console window. Sizes are fluid, the layout is one centred column that folds on narrow screens. */
static const char htmlStyleBlock[] PROGMEM =
    "<style>\n"
    ":root{color-scheme:light dark;--bg:#f2f2f7;--card:#ffffff;--text:#1d1d1f;--muted:#6e6e73;--line:rgba(0,0,0,.08);--fill:rgba(120,120,128,.12);--accent:#0071e3;--accent-soft:rgba(0,113,227,.12);--heat:#ff9500;--heat-soft:rgba(255,149,0,.15);--ok:#248a3d;--ok-soft:rgba(52,199,89,.15);--err:#d70015;--err-soft:rgba(255,59,48,.14);--shadow:0 1px 2px rgba(0,0,0,.04),0 10px 30px rgba(0,0,0,.05)}\n"
    "@media(prefers-color-scheme:dark){:root{--bg:#000000;--card:#1c1c1e;--text:#f5f5f7;--muted:#98989d;--line:rgba(255,255,255,.10);--fill:rgba(120,120,128,.24);--accent:#0a84ff;--accent-soft:rgba(10,132,255,.18);--heat:#ff9f0a;--heat-soft:rgba(255,159,10,.18);--ok:#30d158;--ok-soft:rgba(48,209,88,.18);--err:#ff453a;--err-soft:rgba(255,69,58,.18);--shadow:none}}\n"
    "*{box-sizing:border-box}\n"
    "html{-webkit-text-size-adjust:100%}\n"
    "body{margin:0;background:var(--bg);color:var(--text);font:15px/1.45 -apple-system,BlinkMacSystemFont,'SF Pro Text','Segoe UI',Roboto,Helvetica,Arial,sans-serif;-webkit-font-smoothing:antialiased}\n"
    ".wrap{max-width:760px;margin:0 auto;padding:20px 16px 48px}\n"
    ".top{display:flex;align-items:flex-start;justify-content:space-between;gap:12px;flex-wrap:wrap;margin:4px 0 18px}\n"
    ".brand{font-size:20px;font-weight:600;letter-spacing:-.015em}\n"
    ".brand small{display:block;margin-top:2px;font-size:12px;font-weight:400;color:var(--muted);letter-spacing:0}\n"
    ".pills{display:flex;flex-wrap:wrap;gap:6px;justify-content:flex-end}\n"
    ".pill{display:inline-block;padding:5px 11px;border-radius:999px;background:var(--fill);color:var(--muted);font-size:12px;font-weight:500;white-space:nowrap}\n"
    ".pill.ok{background:var(--ok-soft);color:var(--ok)}\n"
    ".pill.bad{background:var(--err-soft);color:var(--err)}\n"
    ".pill.heat{background:var(--heat-soft);color:var(--heat)}\n"
    ".card{background:var(--card);border:1px solid var(--line);border-radius:18px;padding:20px;margin:0 0 14px;box-shadow:var(--shadow)}\n"
    ".thermo{padding:22px 20px 20px;text-align:center}\n"
    ".dial-wrap{position:relative;width:250px;height:250px;margin:12px auto 4px;display:flex;flex-direction:column;align-items:center;justify-content:center}\n"
    ".dial{position:absolute;inset:0;border-radius:50%;background:conic-gradient(from 225deg,var(--heat) 0deg var(--p),var(--fill) var(--p) 270deg,rgba(0,0,0,0) 270deg);-webkit-mask:radial-gradient(closest-side,rgba(0,0,0,0) calc(100% - 15px),#000 calc(100% - 14px));mask:radial-gradient(closest-side,rgba(0,0,0,0) calc(100% - 15px),#000 calc(100% - 14px))}\n"
    ".dial.off{background:conic-gradient(from 225deg,var(--muted) 0deg var(--p),var(--fill) var(--p) 270deg,rgba(0,0,0,0) 270deg)}\n"
    ".setpoint{font-size:62px;font-weight:600;line-height:1;letter-spacing:-.035em;font-variant-numeric:tabular-nums}\n"
    ".setpoint .unit{font-size:20px;font-weight:500;letter-spacing:0;color:var(--muted);margin-left:6px}\n"
    ".measured{display:flex;align-items:baseline;gap:7px;margin-top:10px;font-size:15px;color:var(--muted)}\n"
    ".measured .cap{font-size:11px;font-weight:600;letter-spacing:.06em;text-transform:uppercase}\n"
    ".thermo .row{justify-content:center;margin-top:18px}\n"
    ".segmented{display:flex;gap:8px;margin:16px auto 0;max-width:260px}\n"
    ".segmented button{flex:1;padding:9px 0;border:1px solid var(--line);border-radius:12px;background:rgba(0,0,0,0);color:var(--muted);font-weight:600;font-size:14px}\n"
    ".segmented button.active{background:var(--heat-soft);border-color:rgba(0,0,0,0);color:var(--heat)}\n"
    ".segmented button.active.off{background:var(--fill);color:var(--text)}\n"
    ".label{font-size:12px;font-weight:600;letter-spacing:.06em;text-transform:uppercase;color:var(--muted)}\n"
    ".value{margin-top:4px;font-size:64px;font-weight:600;line-height:1.05;letter-spacing:-.035em;font-variant-numeric:tabular-nums}\n"
    ".unit{margin-left:5px;font-size:24px;font-weight:500;letter-spacing:0;color:var(--muted)}\n"
    ".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(210px,1fr));gap:14px;margin:0 0 14px}\n"
    ".grid .card{margin:0;padding:18px}\n"
    ".stat .value{font-size:30px;letter-spacing:-.02em}\n"
    ".stat .value .unit{font-size:16px}\n"
    ".sub{margin-top:8px;font-size:12px;color:var(--muted)}\n"
    ".state{display:flex;align-items:center;gap:9px;margin-top:12px;font-size:22px;font-weight:600;letter-spacing:-.01em}\n"
    ".dot{width:11px;height:11px;border-radius:50%;background:var(--muted);flex:0 0 auto}\n"
    ".dot.heat{background:var(--heat);animation:pulse 2s ease-out infinite}\n"
    "@keyframes pulse{0%{box-shadow:0 0 0 0 var(--heat-soft)}70%{box-shadow:0 0 0 9px rgba(0,0,0,0)}100%{box-shadow:0 0 0 0 rgba(0,0,0,0)}}\n"
    "details.card>summary{cursor:pointer;list-style:none;display:flex;align-items:center;justify-content:space-between;font-weight:600;font-size:15px}\n"
    "details.card>summary::-webkit-details-marker{display:none}\n"
    "details.card>summary::after{content:'+';font-size:20px;font-weight:400;color:var(--muted);line-height:1}\n"
    "details.card[open]>summary::after{content:'\\2212'}\n"
    "details.card[open]>summary{margin-bottom:12px}\n"
    "table{width:100%;border-collapse:collapse;font-size:14px}\n"
    "td{padding:11px 0;border-bottom:1px solid var(--line);vertical-align:top;word-break:break-word}\n"
    "tr:last-child td{border-bottom:0}\n"
    "td:first-child{width:46%;padding-right:14px;color:var(--muted)}\n"
    "td:nth-child(2){font-weight:500}\n"
    ".tablewrap{margin-top:4px;overflow-x:auto}\n"
    "table.cmd{font-size:13px}\n"
    "table.cmd td{width:auto;padding:9px 12px 9px 0;color:var(--text);font-weight:400}\n"
    "table.cmd td:first-child{width:auto;color:var(--text);font-weight:600;white-space:nowrap;padding-right:16px}\n"
    "table.cmd tr:first-child td{color:var(--muted);font-weight:600}\n"
    ".formgrid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:16px;margin-top:14px}\n"
    ".field{display:flex;flex-direction:column;gap:7px}\n"
    ".field>label{font-size:12px;font-weight:600;letter-spacing:.06em;text-transform:uppercase;color:var(--muted)}\n"
    ".row{display:flex;align-items:center;gap:10px;flex-wrap:wrap}\n"
    "select,input[type=text]{-webkit-appearance:none;appearance:none;width:100%;min-width:0;flex:1;padding:10px 12px;border:1px solid var(--line);border-radius:12px;background-color:var(--fill);color:var(--text);font:inherit}\n"
    "select{padding-right:34px;background-image:linear-gradient(45deg,transparent 50%,var(--muted) 50%),linear-gradient(135deg,var(--muted) 50%,transparent 50%);background-position:calc(100% - 18px) calc(50% - 2px),calc(100% - 13px) calc(50% - 2px);background-size:5px 5px,5px 5px;background-repeat:no-repeat,no-repeat}\n"
    "input::placeholder{color:var(--muted);opacity:1}\n"
    "select:focus,input[type=text]:focus{outline:2px solid var(--accent-soft);border-color:var(--accent)}\n"
    "input[type=submit],button{-webkit-appearance:none;appearance:none;border:0;border-radius:12px;padding:10px 16px;background:var(--accent);color:#fff;font:inherit;font-weight:600;cursor:pointer}\n"
    "input[type=submit]:active,button:active{opacity:.8}\n"
    "input[type=submit]:hover,button:hover{filter:brightness(1.06)}\n"
    "button.plain{background:var(--fill);color:var(--text);border:1px solid var(--line)}\n"
    ".stat .row{margin-top:14px}\n"
    "button.round{width:46px;height:46px;padding:0;border:1px solid var(--line);border-radius:50%;background:var(--fill);color:var(--text);font-size:22px;font-weight:500;line-height:1}\n"
    "button.wide{width:100%}\n"
    "pre.console{margin:12px 0 0;padding:14px;border-radius:14px;background:#0f1113;color:#b8e48a;font:12px/1.45 ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;height:30em;overflow:auto;white-space:pre-wrap;word-break:break-all}\n"
    "footer{margin-top:24px;text-align:center;font-size:12px;color:var(--muted)}\n"
    "@media(max-width:520px){.wrap{padding:16px 12px 40px}.card{padding:16px}.dial-wrap{width:210px;height:210px}.setpoint{font-size:52px}.pills{justify-content:flex-start}.field .row{flex-direction:column;align-items:stretch}}\n"
    "</style>\n";


static uint16_t htmlPageBytes;  /* bytes of the page streamed in the current response */

#if CFG_BOARD_ESP32
/* ESP32's WebServer has no chunked streaming API, so the page is assembled into
   this buffer by the same send helpers and sent once at the end of the page. */
static String htmlPageBuffered;
#endif

#define HTML_ROW_BUF_SIZE 400  /* worst row: update_server_address with a 255 char configured address */

static void htmlRowTruncated(const char* key) {
  logSetLevel(LOG_WARNING);   /* level 0: a row the page could not send whole, in every build */
  Serial.print("HTML row truncated: ");
  Serial.println(key);
}

/* Send one chunk. The buffer may be a flash literal or a RAM stack buffer; the core picks
   the matching copy for each (StreamConstPtr tests the address at runtime). */
static void htmlSendChunk(const char* buf, size_t len) {
  #if CFG_BOARD_ESP32
  htmlPageBuffered.concat(buf, len);  /* buffer: sent as one body with webServer.send() */
  #else
  webServer.sendContent(buf, len);
  #endif
  htmlPageBytes += static_cast<uint16_t>(len);
}

/* A page fragment that must not cost DRAM: the literal gets its own flash section (the core's PROGMEM attribute,
   unique per line) and its length is known right here, so nothing is measured and no ceiling applies. This is the
   shape for every fragment that is only ever sent; text that is formatted into (snprintf, htmlRow2, htmlPill)
   has to stay in RAM. */
#define HTML_SEND_FLASH(fragment)                                                              \
  do {                                                                                         \
    static const char htmlFlashFragment[] PROGMEM __attribute__((aligned(4))) = (fragment);     \
    htmlSendChunk(htmlFlashFragment, sizeof(htmlFlashFragment) - 1);                            \
  } while (0)

/* Longest text the page hands to htmlSendTextPtr as a bare pointer (a String's buffer, a conditional
   literal). Arrays - the page literals, their PROGMEM copies and the stack buffers - carry their own
   extent in the template overload below, so they are never measured against this ceiling. */
static const size_t HTML_SEND_MAX = 512;

/* One character of a text that may live in flash: the PROGMEM page literals sit in .irom0.text, and there
   the ESP8266 answers a byte load with exception 3 (LoadStoreErrorCause) - only 32 bit aligned word reads
   are allowed in flash, which is what pgm_read_byte() issues. In DRAM it is a plain load. */
static char htmlTextAt(const char* text, size_t index) {
  #if CFG_BOARD_ESP8266
  return static_cast<char>(pgm_read_byte(text + index));
  #else
  return text[index];
  #endif  // CFG_BOARD_ESP8266
}

/* Measure a text inside a caller-supplied bound: the scan stops at the first '\0' or at maxLen, so a
   buffer that is not '\0'-terminated is never read past its end (CWE-126). */
static size_t htmlTextLength(const char* text, size_t maxLen) {
  size_t len = 0;
  while ((len < maxLen) && (htmlTextAt(text, len) != '\0')) {
    len++;
  }
  return len;
}

/* An array argument arrives with its extent, so the length is exact and the bound is never guessed: this
   covers the page literals, the PROGMEM copies of them and the stack buffers that hold snprintf output.
   The overload set matters: a non-template 'const char*' wins against this template for an array argument,
   so the pointers go through htmlSendTextPtr() - with both under one name every literal would take the
   pointer overload, be measured against HTML_SEND_MAX and the style block and the script cut in half. */
template <size_t N>
static void htmlSendText(const char (&text)[N]) {
  htmlSendChunk(text, htmlTextLength(text, N));
}

/* A pointer argument has no extent here, so it is measured against the page's own ceiling. */
static void htmlSendTextPtr(const char* text) {
  htmlSendChunk(text, htmlTextLength(text, HTML_SEND_MAX));
}

static void htmlRowAppend(int rowLen, const char* key, const char* row, size_t rowSize) {
  if (rowLen < 0) {
    return;
  }
  if (static_cast<size_t>(rowLen) >= rowSize) {
    htmlRowTruncated(key);
    rowLen = static_cast<int>(rowSize) - 1;  /* send what fits, the row is complete HTML */
  }
  htmlSendChunk(row, static_cast<size_t>(rowLen));
}

static void htmlRow2(const char* key, const char* value) {
  char row[HTML_ROW_BUF_SIZE];
  htmlRowAppend(snprintf(row, sizeof(row), "<tr><td>%s</td><td>%s</td></tr>", key, value), key, row, sizeof(row));
}

static void htmlRow2Str(const char* key, const String& value) {  /* core APIs that return String */
  htmlRow2(key, value.c_str());
}

static void htmlRow2U32(const char* key, uint32_t value, const char* suffix = "") {
  char row[HTML_ROW_BUF_SIZE];
  htmlRowAppend(snprintf(row, sizeof(row), "<tr><td>%s</td><td>%" PRIu32 "%s</td></tr>", key, value, suffix), key, row, sizeof(row));
}

static void htmlRow2I32(const char* key, int32_t value, const char* suffix = "") {
  char row[HTML_ROW_BUF_SIZE];
  htmlRowAppend(snprintf(row, sizeof(row), "<tr><td>%s</td><td>%" PRId32 "%s</td></tr>", key, value, suffix), key, row, sizeof(row));
}

static void htmlRow4(const char* key, const char* value, const char* current_value, const char* description) {
  char row[HTML_ROW_BUF_SIZE];
  htmlRowAppend(snprintf(row, sizeof(row), "<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>", key, value, current_value, description), key, row, sizeof(row));
}

static void htmlRow4Int(const char* key, const char* value, int16_t current_value, const char* description) {
  char row[HTML_ROW_BUF_SIZE];
  htmlRowAppend(snprintf(row, sizeof(row), "<tr><td>%s</td><td>%s</td><td>%d</td><td>%s</td></tr>", key, value, current_value, description), key, row, sizeof(row));
}

/* one status pill: 'ok', 'bad', 'heat' or 'muted' picks the colour, the text is a page literal */
static void htmlPill(const char* text, const char* state) {
  char row[128];
  htmlRowAppend(snprintf(row, sizeof(row), "<span class='pill %s'>%s</span>", state, text), "pill", row, sizeof(row));
}

/* where the setpoint sits on the dial: MINIMUM..MAXIMUM_TARGET_TEMP is the 270 degree arc of the ring, drawn with
   a conic-gradient whose stop is the angle emitted as --p. The live update endpoint sends the same number. */
static uint16_t thermostatDialAngle(void) {
  int16_t angle = static_cast<int16_t>((static_cast<int16_t>(myThermostat.getTargetTemperature()) - static_cast<int16_t>(MINIMUM_TARGET_TEMP)) * 270 /
                                       (static_cast<int16_t>(MAXIMUM_TARGET_TEMP) - static_cast<int16_t>(MINIMUM_TARGET_TEMP)));
  if (angle < 0) {
    angle = 0;
  } else if (angle > 270) {
    angle = 270;
  }
  return static_cast<uint16_t>(angle);
}

/* ---- dashboard window, first window of the page ----
   The live sensor and thermostat values: one thermostat card with the mode, the target temperature as the big
   number inside a dial ring, the measured temperature below it, the -/+ stepper and the Heat/Off switch, followed
   by the humidity and heating state cards. A value that the sensor could not deliver yet is a dash instead of a
   wrong number: the thermostat keeps THERMOSTAT_SENSOR_INIT_VALUE until the first successful read, and the
   sensor error flag stays set while the reads fail. */
static void htmlWindowDashboard(void) {
  char valueBuf[16];
  char angleBuf[12];  /* up to "65535deg" + NUL: gcc cannot see the 0..270 clamp inside thermostatDialAngle() */
  const bool sensorError      = myThermostat.getSensorError();
  const bool temperatureReady = (sensorError == false) && (myThermostat.getFilteredTemperature() != THERMOSTAT_SENSOR_INIT_VALUE);
  const bool humidityReady    = (sensorError == false) && (myThermostat.getFilteredHumidity() != THERMOSTAT_SENSOR_INIT_VALUE);
  const bool thermostatHeat   = (myThermostat.getThermostatMode() == TH_HEAT);
  const bool heating          = thermostatHeat && myThermostat.getActualState();

  const char* stateText  = heating ? "Heating" : (thermostatHeat ? "Idle" : "Off");
  const char* stateClass = heating ? "heat" : (thermostatHeat ? "ok" : "off");

  snprintf(angleBuf, sizeof(angleBuf), "%ddeg", thermostatDialAngle());

  /* THERMOSTAT: mode, target and measured temperature, stepper and mode switch in one card. The ids are the
     handles of the live update (htmlLiveScript): the tab patches these nodes instead of reloading the page. */
  HTML_SEND_FLASH("<section class='card thermo'><div class='label'>Mode: <span id='v-mode'>");
  htmlSendTextPtr(thermostatHeat ? "Heat" : "Off");
  HTML_SEND_FLASH("</span></div>");

  HTML_SEND_FLASH("<div class='dial-wrap'><div id='v-dial' class='dial");
  if (thermostatHeat == false) {
    HTML_SEND_FLASH(" off");
  }
  HTML_SEND_FLASH("' style='--p:");
  htmlSendText(angleBuf);
  HTML_SEND_FLASH("'></div><div class='setpoint'><span id='v-target'>");
  formatScaled(valueBuf, sizeof(valueBuf), myThermostat.getTargetTemperature(), 1);
  htmlSendText(valueBuf);
  HTML_SEND_FLASH("</span><span class='unit'>&deg;C</span></div><div class='measured'><span class='cap'>Current</span><span id='v-current'>");
  if (temperatureReady) {
    formatScaled(valueBuf, sizeof(valueBuf), myThermostat.getFilteredTemperature(), 1);
    htmlSendText(valueBuf);
  } else {
    HTML_SEND_FLASH("&mdash;");
  }
  HTML_SEND_FLASH("</span> &deg;C</div></div>");

  htmlSendText(htmlThermostatStepForm);  /* - / + buttons, 0.5 °C per press, POST TargetStep */

  HTML_SEND_FLASH("<form method='POST' action='/' autocomplete='off' class='segmented live'>");
  if (thermostatHeat) {
    HTML_SEND_FLASH("<button type='submit' name='ThermostatMode' value='1' id='b-heat' class='active heat'>Heat</button>");
    HTML_SEND_FLASH("<button type='submit' name='ThermostatMode' value='0' id='b-off'>Off</button>");
  } else {
    HTML_SEND_FLASH("<button type='submit' name='ThermostatMode' value='1' id='b-heat'>Heat</button>");
    HTML_SEND_FLASH("<button type='submit' name='ThermostatMode' value='0' id='b-off' class='active off'>Off</button>");
  }
  HTML_SEND_FLASH("</form>");

  HTML_SEND_FLASH("<div class='sub'>Hysteresis &plusmn;");
  formatScaled(valueBuf, sizeof(valueBuf), myThermostat.getThermostatHysteresis(), 1);
  htmlSendText(valueBuf);
  HTML_SEND_FLASH(" &deg;C</div>");

  /* the sensor state is one of two pills, both in the page so the live update only flips their display */
  HTML_SEND_FLASH("<div class='sub' id='v-sensor' style='display:");
  htmlSendTextPtr((sensorError || (temperatureReady == false)) ? "block" : "none");
  HTML_SEND_FLASH("'><span class='pill bad' id='v-sensor-bad' style='display:");
  htmlSendTextPtr(sensorError ? "inline-block" : "none");
  HTML_SEND_FLASH("'>Sensor error</span><span class='pill muted' id='v-sensor-wait' style='display:");
  htmlSendTextPtr(((sensorError == false) && (temperatureReady == false)) ? "inline-block" : "none");
  HTML_SEND_FLASH("'>Sensor starting</span></div>");
  HTML_SEND_FLASH("</section>\n");

  /* CARD GRID: humidity and heating state */
  HTML_SEND_FLASH("<div class='grid'>");

  HTML_SEND_FLASH("<div class='card stat'><div class='label'>Current humidity</div><div class='value'><span id='v-humidity'>");
  if (humidityReady) {
    formatScaled(valueBuf, sizeof(valueBuf), myThermostat.getFilteredHumidity(), 1);
    htmlSendText(valueBuf);
  } else {
    HTML_SEND_FLASH("&mdash;");
  }
  HTML_SEND_FLASH("</span><span class='unit'>%</span></div><div class='sub'>");
  htmlSendTextPtr(myConfig.sensor_type == cDHT22 ? "DHT22 sensor" : "BME280 sensor");
  HTML_SEND_FLASH("</div></div>");

  /* heating state: the relay, not the mode - the mode switch lives in the thermostat card above */
  HTML_SEND_FLASH("<div class='card stat'><div class='label'>Heating state</div><div class='state'><span class='dot ");
  htmlSendTextPtr(stateClass);
  HTML_SEND_FLASH("' id='v-dot'></span><span id='v-state'>");
  htmlSendTextPtr(stateText);
  HTML_SEND_FLASH("</span></div><div class='sub'><span id='v-relay'>");
  htmlSendTextPtr(heating ? "Relay on" : (thermostatHeat ? "Relay off" : "Thermostat off"));
  HTML_SEND_FLASH("</span></div></div>");

  HTML_SEND_FLASH("</div>\n");
}

/* ---- console output window, the card above the restart section ----
   The bytes the program captured from its Serial.print / println / printf calls (print_log.h) are replayed as
   text, escaped for HTML, in the order the software sent them: every complete line the ring buffer still holds,
   a line the ring cut in the middle marked. The page works on a snapshot taken before the window is streamed, so
   bytes printed while the response is still being sent cannot change what was already sent.
   The capture is not wrapped in CFG_DEBUG: release builds print less, but what they print is shown as well. */
/* the console text itself, escaped for HTML and ready for the <pre>: the page streams it into the window and
   the live update fetches the same bytes into the open <pre>, so both show one implementation. */
static void htmlConsoleBody(const LogWindow* window) {
  char row[128];  /* text run, flushed to the client whenever it is full */
  size_t rowLen = 0;
  for (uint32_t i = window->start; i < window->end; i++) {
    if (rowLen + 6 >= sizeof(row)) {  /* spare room for the longest escape sequence */
      htmlSendChunk(row, rowLen);
      rowLen = 0;
    }
    logHtmlAppend(row, &rowLen, logByteAt(i));
  }
  if (rowLen > 0) {
    htmlSendChunk(row, rowLen);
  }
}

static void htmlWindowConsole(void) {
  LogWindow window;
  logWindowSnapshot(&window);

  // "%u" with an explicit cast: the "%" PRIu32 concatenation reads as a runtime format string to
  // static analysers (CWE-134), and unsigned int is 32 bit on both boards
  char numBuf[24];

  HTML_SEND_FLASH("<section class='card'><div class='row' style='justify-content:space-between'>");
  HTML_SEND_FLASH("<div class='label'>Console output</div><div class='sub' style='margin:0'>");
  snprintf(numBuf, sizeof(numBuf), "%u", static_cast<unsigned int>(window.linesTotal));
  HTML_SEND_FLASH("<span id='v-lines-total'>");
  htmlSendText(numBuf);
  HTML_SEND_FLASH("</span> lines");
  if ((window.start < window.end) && (logLineStart(window.start) == false)) {
    HTML_SEND_FLASH(", first line cut off by the capture buffer");
  }
  HTML_SEND_FLASH("</div></div>");

  HTML_SEND_FLASH("<pre class='console' id='v-console'>");
  htmlConsoleBody(&window);
  HTML_SEND_FLASH("</pre></section>\n");
}

void htmlStreamPage(void) {
  int16_t pageRssi = 0;
  const bool pageRssiValid = readValidRssi(&pageRssi);
  float rssiInPercent = 0.0;
  if (pageRssiValid) {
    rssiInPercent = min(max(2 * (static_cast<float>(pageRssi) + 100.0f), 0.0f), 100.0f);
  }
  char rssiBuf[16];  /* the dBm figure the percentage above hides */
  if (pageRssiValid) {
    (void)snprintf(rssiBuf, sizeof(rssiBuf), "%d dBm", static_cast<int>(pageRssi));
  } else {
    strlcpy(rssiBuf, "n/a", sizeof(rssiBuf));
  }
  #if CFG_BOARD_ESP8266
  char numBuf[16];  /* one scratch buffer for the Vcc row */
  #endif

  htmlPageBytes = 0;
  #if CFG_BOARD_ESP32
  htmlPageBuffered = String();  /* buffer the whole page, sent with webServer.send() below */
  #else
  if (webServer.chunkedResponseModeStart(200, "text/html") == false) {
    #if LOG_LEVEL_BUILD >= 2
    logSetLevel(LOG_INFO);   /* level 2: the dumps, hidden at a runtime level below 2 */
    Serial.println("Client speaks HTTP/1.0, chunked page not sent");
    #endif  /* LOG_LEVEL_BUILD >= 2 */
    return;
  }
  #endif

  htmlSendText(htmlHeadDoctype);
  /* HEAD */
  htmlSendText(htmlHeadMeta);
  HTML_SEND_FLASH("<title> ");
  htmlSendText(myConfig.name);
  HTML_SEND_FLASH("</title>\n");
  htmlSendText(htmlStyleBlock);
  htmlSendText(htmlHeadClose);
  /* BODY */
  htmlSendText(htmlBodyOpen);

  /* header: device name, firmware and the connection pills */
  HTML_SEND_FLASH("<header class='top'><div class='brand'>");
  htmlSendText(myConfig.name);
  HTML_SEND_FLASH("<small>Thermostat &middot; FW ");
  htmlSendText(VERSION);
  HTML_SEND_FLASH("</small></div><div class='pills'>");
  htmlPill(WiFi.status() == WL_CONNECTED ? "Wi-Fi" : "Wi-Fi off", WiFi.status() == WL_CONNECTED ? "ok" : "bad");
  htmlPill(myMqttClient.connected() ? "MQTT" : "MQTT off", myMqttClient.connected() ? "ok" : "bad");
  HTML_SEND_FLASH("</div></header>\n");

  /* sensor and thermostat values */
  htmlWindowDashboard();

  /* SYSTEM INFORMATION */
  HTML_SEND_FLASH("<details class='card'><summary>System information</summary>");
  htmlSendText(htmlTableOpen);
  /*= KEY ======================================||= VALUE ================================*/
  htmlRow2("Name", myConfig.name);
  htmlRow2Str("Chip ID", getEspChipId());
  #if CFG_BOARD_ESP8266
  snprintf(numBuf, sizeof(numBuf), "%d.%02d", static_cast<int>(ESP.getVcc() / 1000), static_cast<int>(ESP.getVcc() % 1000) / 10);
  htmlRow2("Vcc", numBuf);
  htmlRow2Str("Arduino Core", ESP.getCoreVersion());
  htmlRow2Str("Reset Reason", ESP.getResetReason());
  htmlRow2U32("Flash Size", ESP.getFlashChipRealSize());
  #elif CFG_BOARD_ESP32
  htmlRow2Str("Reset Reason CPU 0", getEspResetReason(rtc_get_reset_reason(0)));
  htmlRow2Str("Reset Reason CPU 1", getEspResetReason(rtc_get_reset_reason(1)));
  htmlRow2U32("CPU Frequency", static_cast<uint32_t>(ESP.getCpuFreqMHz()), " MHz");
  htmlRow2U32("Flash Speed", static_cast<uint32_t>(ESP.getFlashChipSpeed() / 1000000), " MHz");
  htmlRow2U32("Flash Size", ESP.getFlashChipSize());
  htmlRow2U32("Flash Mode", ESP.getFlashChipMode());
  htmlRow2("SDK Verison", ESP.getSdkVersion());
  #else
  #endif
  htmlRow2U32("Sketch Size", ESP.getSketchSize() / 1024, " kB");
  htmlRow2U32("Free for Sketch", ESP.getFreeSketchSpace() / 1024, " kB");
  htmlRow2U32("Free Heap", ESP.getFreeHeap() / 1024, " kB");
  htmlRow2Str("Time since Reset", millisFormatted());
  htmlRow2("Installed Firmware", VERSION);
  htmlRow2("Available Firmware", myConfig.available_firmware_version.c_str());
  htmlRow2Str("IPv4", WiFi.localIP().toString());
  htmlRow2Str("WiFi Status", wifiStatusToString(WiFi.status()));
  htmlRow2I32("WiFi Strength", static_cast<int32_t>(rssiInPercent), " %");
  htmlRow2Str("WiFi Signal", rssiBuf);
  #if CFG_BOARD_ESP8266
  htmlRow2U32("WiFi Drops", wifiDisconnectTotal);
  #endif
  htmlRow2("MQTT Status", myMqttClient.connected() ? "connected" : "disconnected");
  htmlRow2U32("MQTT Connects", mqtt_connect_counter);
  htmlRow2Str("MQTT Last Error", myMqttHelper.mqttLastErrorToString((int8_t)myMqttClient.lastError()));
  htmlRow2("Local Time", time_buffer);
  htmlRow2U32("Looptime mean [&micro;s]", MyLooptime.get_time_duration_mean());
  htmlRow2U32("Looptime min [&micro;s]", MyLooptime.get_time_duration_min());
  htmlRow2U32("Looptime max [&micro;s]", MyLooptime.get_time_duration_max());
  htmlSendText(htmlTableClose);
  HTML_SEND_FLASH("</details>\n");

  /* CONFIGURATION: input method and sensor, one POST form each */
  HTML_SEND_FLASH("<section class='card'><div class='label'>Configuration</div><div class='formgrid'>");

  htmlSendText(htmlFormOpen);
  HTML_SEND_FLASH("<div class='field'><label for='input_method'>Input method</label><div class='row'><select name='InputMethod' id='input_method'>");
  if (myConfig.input_method == cPUSH_BUTTONS) {
    HTML_SEND_FLASH("<option value='0'>Rotary Encoder </option> <option value='1' selected> Push Buttons </option>");
  } else {
    HTML_SEND_FLASH("<option value='0' selected> Rotary Encoder </option> <option value='1'> Push Buttons </option>");
  }
  HTML_SEND_FLASH("</select><input type='submit' value='Apply'></div></div>");
  htmlSendText(htmlFormClose);

  htmlSendText(htmlFormOpen);
  HTML_SEND_FLASH("<div class='field'><label for='sensor'>Sensor</label><div class='row'><select name='Sensor' id='sensor'>");
  if (myConfig.sensor_type == cDHT22) {  /* select currently configured input method */
    HTML_SEND_FLASH("<option value='0' selected >DHT22 </option> <option value='1'> BME280 </option>");
  } else {
    HTML_SEND_FLASH("<option value='0'>DHT22 </option> <option value='1' selected > BME280 </option>");
  }
  HTML_SEND_FLASH("</select><input type='submit' value='Apply'></div></div>");
  htmlSendText(htmlFormClose);

  HTML_SEND_FLASH("</div></section>\n");

  /* COMMAND LINE */
  HTML_SEND_FLASH("<section class='card'><div class='label'>Command line</div>");
  htmlSendText(htmlFormOpen);
  HTML_SEND_FLASH("<div class='row' style='margin-top:12px'><input type='text' name='cmd' placeholder='key:value' aria-label='Command'><input type='submit' value='Send'></div>");
  htmlSendText(htmlFormClose);
  HTML_SEND_FLASH("</section>\n");

  /* COMMANDS TABLE */
  HTML_SEND_FLASH("<details class='card'><summary>Commands</summary>");
  htmlSendText(htmlCommandsTableOpen);
  htmlRow4("<b>Key</b>", "<b>Value</b>", "<b>Current Value</b>", "<b>Description</b>");
  htmlRow4("discover", "0 | 1", "none", "MQTT on demand device discovery: 1 = discover; 0 = remove");
  htmlRow4Int("discovery_enabled", "0 | 1", myConfig.discovery_enabled, "MQTT Autodiscovery: 1 = enabled; 0 = disabled");
  htmlRow4("name", "string", myConfig.name, "Define a name for this device");
  htmlRow4("mqtt_server_address", "string", myConfig.mqtt_host, "Address of the MQTT server");
  htmlRow4Int("mqtt_server_port", "1 .. 65535", myConfig.mqtt_port, "Port of the MQTT server");
  htmlRow4("mqtt_user", "string", myConfig.mqtt_user, "MQTT user");
  htmlRow4("mqtt_pwd", "string", "***", "MQTT password");
  htmlRow4Int("mqtt_pub_cycle", "Range: 1 .. 255, LSB: 1 min", myConfig.mqtt_publish_cycle, "Publish cycle for MQTT messages in minutes");
  htmlRow4("update_server_address", "string", myConfig.update_server_address, "Address of the update server");
  htmlRow4Int("auto_update", "0 | 1", myConfig.auto_update, "Automatically apply available updates: 1 = Auto Updates enabled; 0 = Auto Updates disabled");
  htmlRow4("update_firmware", "0 | 1", "none", "On demand firmware update: 1 = update");
  htmlRow4Int("planned_restart", "0 | 1", myConfig.planned_restart, "Execute planned restart");
  htmlRow4Int("calibration_offset", "Range: -50 .. +50, LSB: 0.1 &deg;C", myConfig.calibration_offset, "Offset calibration for temperature sensor.");
  htmlRow4Int("calibration_factor", "Range: +50 .. +200, LSB: 1 %", myConfig.calibration_factor, "Linearity calibration for temperature sensor.");
  htmlRow4Int("display_enabled", "0 | 1", myConfig.display_enabled, "Display always on or only on user interaction");
  htmlRow4Int("display_brightness", "Range: 0 .. +255, LSB: 1 step", myConfig.display_brightness, "Brightness of OLED display");
  htmlRow4("timezone", "string", myConfig.timezone, "Timezone for NTP, pick from this list: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv");
  htmlRow4("console_clear", "1", "none", "Clear the console output window (last window at the bottom of the page)");
  htmlRow4Int("log_level", "0..2", logRuntimeLevel, "Console window verbosity: 0 errors, 1 warnings, 2 info");
  htmlSendText(htmlCommandsTableClose);
  HTML_SEND_FLASH("</details>\n");

  /* CONSOLE LOG LEVEL: the runtime verbosity of the window right below, and the button that empties it. The
     level is not persisted: the next boot starts at LOG_WARNING again. Info only exists where the image
     compiled it in, so a release build does not offer it. The form is 'live', so neither press reloads the
     page: the tab posts, then polls and repaints the console window. */
  HTML_SEND_FLASH("<section class='card'><div class='label'>Console log level</div>");
  HTML_SEND_FLASH("<form method='POST' action='/' autocomplete='off' class='live'>");
  HTML_SEND_FLASH("<div class='field'><div class='row'><select name='LogLevel' aria-label='Console log level'>");
  htmlSendTextPtr(logRuntimeLevel == LOG_ERROR ? "<option value='0' selected>Errors</option>" : "<option value='0'>Errors</option>");
  htmlSendTextPtr(logRuntimeLevel == LOG_WARNING ? "<option value='1' selected>Warnings</option>" : "<option value='1'>Warnings</option>");
  #if LOG_LEVEL_BUILD >= 2
  htmlSendTextPtr(logRuntimeLevel == LOG_INFO ? "<option value='2' selected>Info</option>" : "<option value='2'>Info</option>");
  #endif
  HTML_SEND_FLASH("</select><input type='submit' value='Set Log Level'>");
  HTML_SEND_FLASH("<button type='submit' name='cmd' value='console_clear:1' class='plain'>Clear Log</button></div></div>");
  #if LOG_LEVEL_BUILD < 2
  HTML_SEND_FLASH("<div class='sub'>Info is compiled into debug builds only.</div>");
  #endif
  htmlSendText(htmlFormClose);
  HTML_SEND_FLASH("</section>\n");

  htmlWindowConsole();  /* the captured console output, see htmlWindowConsole() above */

  /* RESTART DEVICE */
  HTML_SEND_FLASH("<section class='card'><div class='row' style='justify-content:space-between'><div><div class='label'>Restart device</div><div class='sub'>Reboots the controller and reloads this page.</div></div>");
  if (systemRestartRequest == false) {
    htmlSendText(htmlRestartButton);
  } else {
    htmlSendText(htmlRestartWaitingButton);
  }
  HTML_SEND_FLASH("</div></section>\n");

  htmlSendText(htmlLiveScript);
  HTML_SEND_FLASH("<footer>ESP8266 Smart Home</footer>\n");
  htmlSendText(htmlPageFooter);

  #if CFG_BOARD_ESP32
  webServer.send(200, "text/html", htmlPageBuffered);
  #else
  htmlSendChunk("", 0);  /* zero length chunk closes the chunked response */
  #endif

  #if LOG_LEVEL_BUILD >= 2
  logSetLevel(LOG_INFO);   /* level 2: the dumps, hidden at a runtime level below 2 */
  /* Serial.print() keeps the deep frames small; Serial.printf() allocates a 128 byte
     local buffer inside Print::vprintf in the cont stack that runs loop() */
  logSetLevel(LOG_INFO);   /* level 2: the dumps, hidden at a runtime level below 2 */
  Serial.print("HTML page bytes streamed: ");
  Serial.println(htmlPageBytes);
  logSetLevel(LOG_INFO);   /* level 2: the dumps, hidden at a runtime level below 2 */
  Serial.print("  Free Heap: ");
  Serial.println(ESP.getFreeHeap());
  #if CFG_BOARD_ESP8266
  logSetLevel(LOG_INFO);   /* level 2: the dumps, hidden at a runtime level below 2 */
  Serial.print("  Free ContStack at page build: ");   /* ESP8266 only, the ESP32 core has no cont stack */
  Serial.println(ESP.getFreeContStack());
  #endif
  #endif  /* LOG_LEVEL_BUILD >= 2 */
}

/* JSON the live update pulls: the same numbers the cards show. A value the sensor could not deliver is null, the
   tab renders a dash. Small on purpose - it replaces a 12 kB page reload per refresh. */
void handleApiState(void) {
  const bool sensorError      = myThermostat.getSensorError();
  const bool temperatureReady = (sensorError == false) && (myThermostat.getFilteredTemperature() != THERMOSTAT_SENSOR_INIT_VALUE);
  const bool humidityReady    = (sensorError == false) && (myThermostat.getFilteredHumidity() != THERMOSTAT_SENSOR_INIT_VALUE);

  char targetBuf[10];
  char currentBuf[10];
  char humidityBuf[10];
  formatScaled(targetBuf, sizeof(targetBuf), myThermostat.getTargetTemperature(), 1);
  if (temperatureReady) {
    formatScaled(currentBuf, sizeof(currentBuf), myThermostat.getFilteredTemperature(), 1);
  } else {
    strlcpy(currentBuf, "null", sizeof(currentBuf));
  }
  if (humidityReady) {
    formatScaled(humidityBuf, sizeof(humidityBuf), myThermostat.getFilteredHumidity(), 1);
  } else {
    strlcpy(humidityBuf, "null", sizeof(humidityBuf));
  }

  LogWindow logWindow;
  logWindowSnapshot(&logWindow);

  char payload[256];  /* the dashboard values plus the link figures a drop report needs */
  int16_t apiRssi = 0;
  char linkField[40];  /* rssi/ch as a unit: both are meaningless when the link is down */
  if (readValidRssi(&apiRssi)) {
    (void)snprintf(linkField, sizeof(linkField), "\"rssi\":%d,\"ch\":%u",
                   static_cast<int>(apiRssi), static_cast<unsigned int>(WiFi.channel()));
  } else {
    strlcpy(linkField, "\"rssi\":null,\"ch\":null", sizeof(linkField));
  }
  char dropsField[24];  /* the drop counter is only meaningful on the ESP8266 board */
  #if CFG_BOARD_ESP8266
  (void)snprintf(dropsField, sizeof(dropsField), ",\"drops\":%u",
                 static_cast<unsigned int>(wifiDisconnectTotal));
  #else
  dropsField[0] = '\0';
  #endif
  (void)snprintf(payload, sizeof(payload),
                 "{\"target\":%s,\"current\":%s,\"humidity\":%s,\"heat\":%d,\"relay\":%d,\"sensor\":%d,\"angle\":%u,\"log\":%" PRIu32 ",%s,\"heap\":%u%s}",
                 targetBuf,
                 currentBuf,
                 humidityBuf,
                 myThermostat.getThermostatMode() == TH_HEAT ? 1 : 0,
                 myThermostat.getActualState() ? 1 : 0,
                 sensorError ? 1 : 0,
                 thermostatDialAngle(),
                 logWindow.linesTotal,
                 linkField,
                 static_cast<unsigned int>(ESP.getFreeHeap()),
                 dropsField);

  webServer.sendHeader("Cache-Control", "no-store");
  webServer.send(200, "application/json", payload);
}
/* the console text as HTML for the live update: the tab replaces the content of the open <pre> with it whenever
   the line counter in /api/state moved, so the window follows the device without reloading the page. */
void handleConsoleFragment(void) {
  LogWindow window;
  logWindowSnapshot(&window);

  htmlPageBytes = 0;
  #if CFG_BOARD_ESP32
  htmlPageBuffered = String();
  #else
  if (webServer.chunkedResponseModeStart(200, "text/html") == false) {
    return;
  }
  #endif

  htmlConsoleBody(&window);

  #if CFG_BOARD_ESP32
  webServer.send(200, "text/html", htmlPageBuffered);
  #else
  htmlSendChunk("", 0);
  #endif
}
