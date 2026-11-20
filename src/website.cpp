/*===================================================================================================================*/
/* the web page of the thermostat                                                                            */
/*===================================================================================================================*/
/* The status and configuration page of the device, streamed in chunks with the core's chunked transfer encoding:
   every row is formatted into a small stack buffer with snprintf and written straight to the client, so there is
   no page String in the heap (no 'Reallocating large String' reallocs, no fragmentation of the DRAM heap), no
   operator+ / concat / changeBuffer frames on the cont stack that runs loop() ('Stack smashing detected' reported
   by cont_check), and each row checks its own length so a row that does not fit is reported by key instead of
   silently truncating.

   The page is rebuilt from the current configuration on every request, which is what makes POST work: the browser
   sends the changed values back to the same URL. The console output window of print_log.h is the last window at
   the bottom; the command console_clear that empties it is handled in main.cpp. */

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
   fragments share one literal (the three POST forms, the table closes, the two restart buttons) and the style of
   the page is one literal the compiler concatenates, so the style costs one chunk of the chunked response
   instead of six. The bytes the page sends do not change. */

static const char htmlHeadDoctype[] = "<!DOCTYPE html> <html>\n";
static const char htmlHeadMeta[] = "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
static const char htmlHeadClose[] = "</head>\n";
static const char htmlBodyOpen[] = "<body>\n";
static const char htmlTableOpen[] = "<table>";
static const char htmlTableClose[] = "</table>";
static const char htmlCommandsTableOpen[] = "<table style='font-size: 12px'>";
static const char htmlFormOpen[] = "<form method='POST' autocomplete='off'>";
static const char htmlFormClose[] = "</form>";
static const char htmlRestartButton[] = "<button onclick=\"window.location.href='/restart'\"> Restart </button>";
static const char htmlRestartWaitingButton[] = "<button onclick=\"window.location.href='/'\"> Restarting, reload site </button>";
static const char htmlPageFooter[] = "</body>\n" "</html>\n";
static const char htmlStyleBlock[] =
    "<style>html { font-family: Helvetica; font-size: 16px; display: inline-block; margin: 0px auto; text-align: left;}\n"
    "body {background-color: #202226; color: #A0A2A8}\n"
    "p {color: #2686c1; margin-bottom: 10px;}\n"
    "table {color: #A0A2A8;}\n"
    "pre { color: #B8E48A; background-color: #0D0F12; border: 1px solid #41484F; border-radius: 4px; padding: 4px 8px; margin: 6px 0px 0px 0px; font-family: 'Courier'; font-size: 12px; line-height: 1.2; height: 30em; overflow-y: scroll; white-space: pre-wrap; word-break: break-all; }\n"
    "</style>\n";


static uint16_t htmlPageBytes;  /* bytes of the page streamed in the current response */

#if CFG_BOARD_ESP32
/* ESP32's WebServer has no chunked streaming API, so the page is assembled into
   this buffer by the same send helpers and sent once at the end of the page. */
static String htmlPageBuffered;
#endif

#define HTML_ROW_BUF_SIZE 400  /* worst row: update_server_address with a 255 char configured address */

static void htmlRowTruncated(const char* key) {
  logSetLevel(0);   /* level 0: a row the page could not send whole, in every build */
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

static void htmlSendText(const char* text) {
  htmlSendChunk(text, strlen(text));
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
  htmlRowAppend(snprintf(row, sizeof(row), "<tr><td>%s:</td><td>%s</td></tr>", key, value), key, row, sizeof(row));
}

static void htmlRow2Str(const char* key, const String& value) {  /* core APIs that return String */
  htmlRow2(key, value.c_str());
}

static void htmlRow2U32(const char* key, uint32_t value, const char* suffix = "") {
  char row[HTML_ROW_BUF_SIZE];
  htmlRowAppend(snprintf(row, sizeof(row), "<tr><td>%s:</td><td>%" PRIu32 "%s</td></tr>", key, value, suffix), key, row, sizeof(row));
}

static void htmlRow2I32(const char* key, int32_t value, const char* suffix = "") {
  char row[HTML_ROW_BUF_SIZE];
  htmlRowAppend(snprintf(row, sizeof(row), "<tr><td>%s:</td><td>%" PRId32 "%s</td></tr>", key, value, suffix), key, row, sizeof(row));
}

static void htmlRow4(const char* key, const char* value, const char* current_value, const char* description) {
  char row[HTML_ROW_BUF_SIZE];
  htmlRowAppend(snprintf(row, sizeof(row), "<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>", key, value, current_value, description), key, row, sizeof(row));
}

static void htmlRow4Int(const char* key, const char* value, int16_t current_value, const char* description) {
  char row[HTML_ROW_BUF_SIZE];
  htmlRowAppend(snprintf(row, sizeof(row), "<tr><td>%s</td><td>%s</td><td>%d</td><td>%s</td></tr>", key, value, current_value, description), key, row, sizeof(row));
}

/* ---- console output window, last window at the bottom of the page ----
   The bytes the program captured from its Serial.print / println / printf calls (print_log.h) are replayed as
   text, escaped for HTML, in the order the software sent them: at most LOG_MAX_LINES of the newest lines, older
   lines dropped, a line the ring buffer cut in the middle marked. The page works on a snapshot taken before the
   window is streamed, so bytes printed while the response is still being sent cannot change what was already sent.
   The capture is not wrapped in CFG_DEBUG: release builds print less, but what they print is shown as well. */
static void htmlWindowConsole(void) {
  LogWindow window;
  logWindowSnapshot(&window, LOG_MAX_LINES);

  char numBuf[24];

  htmlSendText("<p><b>Console Output</b></p>");
  htmlSendText("<p><small>showing ");
  snprintf(numBuf, sizeof(numBuf), "%" PRIu32, window.linesShown);
  htmlSendText(numBuf);
  htmlSendText(" of ");
  snprintf(numBuf, sizeof(numBuf), "%" PRIu32, window.linesTotal);
  htmlSendText(numBuf);
  htmlSendText(" captured lines");
  if ((window.start < window.end) && (logLineStart(window.start) == false)) {
    htmlSendText(", first line cut off by the capture buffer");
  }
  htmlSendText("</small></p>");

  htmlSendText("<pre>");
  if (window.linesTotal > window.linesShown) {
    htmlSendText("&hellip; older lines dropped\n");
  }
  char row[128];  /* text run, flushed to the client whenever it is full */
  size_t rowLen = 0;
  for (uint32_t i = window.start; i < window.end; i++) {
    if (rowLen + 6 >= sizeof(row)) {  /* spare room for the longest escape sequence */
      htmlSendChunk(row, rowLen);
      rowLen = 0;
    }
    logHtmlAppend(row, &rowLen, logByteAt(i));
  }
  if (rowLen > 0) {
    htmlSendChunk(row, rowLen);
  }
  htmlSendText("</pre>");
}

void htmlStreamPage(void) {
  float rssiInPercent = WiFi.RSSI();
  rssiInPercent = isnan(rssiInPercent) ? -100.0 : min(max(2 * (rssiInPercent + 100.0), 0.0), 100.0);
  char numBuf[16];  /* one scratch buffer for values that need formatting */

  htmlPageBytes = 0;
  #if CFG_BOARD_ESP32
  htmlPageBuffered = String();  /* buffer the whole page, sent with webServer.send() below */
  #else
  if (webServer.chunkedResponseModeStart(200, "text/html") == false) {
    #ifdef CFG_DEBUG
    logSetLevel(2);   /* level 2: the dumps, hidden at a runtime level below 2 */
    Serial.println("Client speaks HTTP/1.0, chunked page not sent");
    #endif /* CFG_DEBUG */
    return;
  }
  #endif

  htmlSendText(htmlHeadDoctype);
  /* HEAD */
  htmlSendText(htmlHeadMeta);
  htmlSendText("<title> ");
  htmlSendText(myConfig.name);
  htmlSendText("</title>\n");
  htmlSendText(htmlStyleBlock);
  htmlSendText(htmlHeadClose);
  /* BODY */
  htmlSendText(htmlBodyOpen);
  htmlSendText("<p><b>System information</b></p>");
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
  htmlRow2("MQTT Status", myMqttClient.connected() ? "connected" : "disconnected");
  htmlRow2U32("MQTT Connects", mqtt_connect_counter);
  htmlRow2Str("MQTT Last Error", myMqttHelper.mqttLastErrorToString((int8_t)myMqttClient.lastError()));
  htmlRow2("Local Time", time_buffer);
  htmlRow2U32("Looptime mean [&micro;s]", MyLooptime.get_time_duration_mean());
  htmlRow2U32("Looptime min [&micro;s]", MyLooptime.get_time_duration_min());
  htmlRow2U32("Looptime max [&micro;s]", MyLooptime.get_time_duration_max());
  htmlSendText(htmlTableClose);
  /* Change Input Method */
  htmlSendText("<p><b>Change Input Method</b></p>");
  htmlSendText(htmlFormOpen);

  if (myConfig.input_method == cPUSH_BUTTONS) {
    htmlSendText("<select name='InputMethod'> <option value='0'>Rotary Encoder </option> <option value='1' selected> Push Buttons </option> </select>&nbsp;<input type='submit' value='Submit'>");
  } else {
    htmlSendText("<select name='InputMethod'> <option value='0' selected> Rotary Encoder </option> <option value='1'> Push Buttons </option> </select>&nbsp;<input type='submit' value='Submit'>");
  }
  htmlSendText(htmlFormClose);
  /* Change sensor */
  htmlSendText("<p><b>Change Sensor</b></p>");
  htmlSendText(htmlFormOpen);
  htmlSendText("<select name='Sensor'> ");
  if (myConfig.sensor_type == cDHT22) {  /* select currently configured input method */
    htmlSendText("<option value='0' selected >DHT22 </option> <option value='1'> BME280 </option>");
  } else {
    htmlSendText("<option value='0'>DHT22 </option> <option value='1' selected > BME280 </option>");
  }
  htmlSendText("</select>&nbsp;<input type='submit' value='Submit'>");
  htmlSendText(htmlFormClose);

  /* Command Line */
  htmlSendText("<p><b>Command Line</b></p>");
  htmlSendText(htmlFormOpen);
  htmlSendText("<input type='text' name='cmd' value='key:value'>&nbsp;<input type='submit' value='Submit'>");
  htmlSendText(htmlFormClose);
  htmlSendText("<p><b>Commands</b></p>");
  /* COMMANDS TABLE */
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
  htmlRow4Int("log_level", "0..2", logRuntimeLevel, "Console window verbosity: 0 failures and state, 1 the running values, 2 the field dumps");
  htmlSendText(htmlTableClose);
  /* Restart Device */
  htmlSendText("<p><b>Restart Device</b></p>");
  if (systemRestartRequest == false) {
  htmlSendText(htmlRestartButton);
  } else {
  htmlSendText(htmlRestartWaitingButton);
  }
  htmlWindowConsole();  /* last window: the captured console output, see htmlWindowConsole() above */
  htmlSendText(htmlPageFooter);

  #if CFG_BOARD_ESP32
  webServer.send(200, "text/html", htmlPageBuffered);
  #else
  htmlSendChunk("", 0);  /* zero length chunk closes the chunked response */
  #endif

  #ifdef CFG_DEBUG
  logSetLevel(2);   /* level 2: the dumps, hidden at a runtime level below 2 */
  /* Serial.print() keeps the deep frames small; Serial.printf() allocates a 128 byte
     local buffer inside Print::vprintf in the cont stack that runs loop() */
  logSetLevel(2);   /* level 2: the dumps, hidden at a runtime level below 2 */
  Serial.print("HTML page bytes streamed: ");
  Serial.println(htmlPageBytes);
  logSetLevel(2);   /* level 2: the dumps, hidden at a runtime level below 2 */
  Serial.print("  Free Heap: ");
  Serial.println(ESP.getFreeHeap());
  logSetLevel(2);   /* level 2: the dumps, hidden at a runtime level below 2 */
  Serial.print("  Free ContStack at page build: ");
  Serial.println(ESP.getFreeContStack());
  #endif /* CFG_DEBUG */
}
