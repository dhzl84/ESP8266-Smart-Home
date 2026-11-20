/*===================================================================================================================*/
/* includes                                                                                                    */
/*===================================================================================================================*/
#include <string.h>
#include "print_log.h"

/*===================================================================================================================*/
/* console ring                                                                                                */
/*===================================================================================================================*/
/* Newest LOG_RING_BYTES bytes of console text. consoleBytesCaptured counts every byte since boot or the last
   clear, so the byte with index i always sits in slot (i % LOG_RING_BYTES): the newest byte is at
   (consoleBytesCaptured - 1) % LOG_RING_BYTES and the oldest byte still on record has index
   consoleBytesCaptured - logBytesVisible(). Writing at the slot of consoleBytesCaptured overwrites the oldest
   byte, which evicts whole lines from the front. */

static char consoleRing[LOG_RING_BYTES];
static uint32_t consoleBytesCaptured = 0;

void logCaptureByte(uint8_t c) {
  consoleRing[consoleBytesCaptured % LOG_RING_BYTES] = static_cast<char>(c);
  consoleBytesCaptured++;
}

void logCaptureBytes(const uint8_t* buffer, size_t size) {
  for (size_t i = 0; i < size; i++) {
    logCaptureByte(static_cast<uint8_t>(buffer[i]));
  }
}

void logClear(void) {
  consoleBytesCaptured = 0;
}

uint32_t logBytesCaptured(void) {
  return consoleBytesCaptured;
}

uint32_t logBytesVisible(void) {
  return (consoleBytesCaptured < LOG_RING_BYTES) ? consoleBytesCaptured : LOG_RING_BYTES;
}

char logByteAt(uint32_t index) {
  return consoleRing[index % LOG_RING_BYTES];
}

/* true when index starts a whole line: the byte in front of it is a line feed and is still in the ring.
   Index 0 is the start of the capture, so it starts a line as well. */
bool logLineStart(uint32_t index) {
  if (index == 0) {
    return true;
  }
  if (index <= (consoleBytesCaptured - logBytesVisible())) {
    return false;  /* the byte in front was overwritten, the line is only partially on record */
  }
  return logByteAt(index - 1) == '\n';
}

void logWindowSnapshot(LogWindow* window, uint16_t maxLines) {
  const uint32_t oldest = consoleBytesCaptured - logBytesVisible();

  uint32_t linesTotal = 0;
  for (uint32_t i = oldest; i < consoleBytesCaptured; i++) {
    if (logByteAt(i) == '\n') {
      linesTotal++;
    }
  }

  window->linesTotal = linesTotal;
  window->linesShown = (linesTotal > maxLines) ? maxLines : linesTotal;
  window->end = consoleBytesCaptured;

  /* the window shows the newest linesShown complete lines plus a possibly unfinished newest one:
     walk over the older linesTotal - linesShown line feeds and start in front of the next byte */
  const uint32_t linesHidden = linesTotal - window->linesShown;
  uint32_t linesSkipped = 0;
  window->start = oldest;
  for (uint32_t i = oldest; linesSkipped < linesHidden; i++) {
    if (logByteAt(i) != '\n') {
      continue;
    }
    linesSkipped++;
    window->start = i + 1;
  }
}

void logHtmlAppend(char* row, size_t* rowLen, char c) {
  if (c == '\r') {
    return;  /* the browser breaks the line at the line feed, the carriage return of println() is dropped */
  }
  const char* entity = nullptr;
  switch (c) {
    case '&':
      entity = "&amp;";
      break;
    case '<':
      entity = "&lt;";
      break;
    case '>':
      entity = "&gt;";
      break;
    default:
      break;
  }
  if (entity == nullptr) {
    row[(*rowLen)++] = c;
    return;
  }
  for (size_t i = 0; entity[i] != '\0'; i++) {
    row[(*rowLen)++] = entity[i];
  }
}

/*===================================================================================================================*/
/* the console tee: one instance, every translation unit that includes print_log.h prints through it             */
/*===================================================================================================================*/
ConsoleTee consoleTee;

/*===================================================================================================================*/
/* print level                                                                                                   */
/*===================================================================================================================*/
/* the verbosity the tee accepts at runtime: it starts at WARNING so errors and the should-be-solved issues show,
   and the web command log_level sets it (LOG_ERROR or LOG_INFO) without recompiling. LOG_LEVEL_BUILD is what the
   image compiled in, not what the tee shows — see print_log.h. */
uint8_t logRuntimeLevel = LOG_WARNING;

/* the level of the line printed next; LOG_ERROR for malfunctions and fallback, LOG_WARNING for a should-be-solved
   issue, LOG_INFO for the running telemetry. A line feed in the tee resets to INFO, so the level only has to be
   stated when it differs from the default. */
void logSetLevel(uint8_t level) {
  consoleTee.setLevel(level);
}
