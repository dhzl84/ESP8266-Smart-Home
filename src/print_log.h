#ifndef PRINT_LOG_H_
#define PRINT_LOG_H_

/*===================================================================================================================*/
/* console output capture                                                                                        */
/*===================================================================================================================*/
/* Every byte the program sends to the console (Serial.print / Serial.println / Serial.printf) is copied into a
   RAM ring buffer before it is handed to the UART, so the status page can replay the console in a window of its own
   at the bottom (htmlWindowConsole() in website.cpp).

   The capture is NOT wrapped in CFG_DEBUG: release builds emit fewer lines (the verbose prints are compiled out by
   the source), but everything a build does print is captured and displayed, so the window is present in debug and
   release builds alike.

   Capture point: the core's Print class renders numbers, strings and printf formats into characters and always hands
   them over to the virtual write() below; overriding write() therefore sees the exact byte sequence the software
   sends - including several prints that share one line - and forwards it unchanged to the real console. Nothing is
   reformatted and no argument is evaluated a second time.

   Redirect: the #define at the bottom of this header renames the identifier Serial to consoleTee for the code that
   follows it, so a project translation unit that includes this header AFTER all of its library headers captures only
   its own prints; library inline code that is compiled before the redirect still writes to the real console.
   Include this header last, and never spell the bare identifier Serial after it.

   Timestamp: the tee starts every line with a [hh:mm:ss] prefix, written straight into the capture and the console
   so it cannot recurse through the tee. It is the NTP local time once the clock is synced; before that (boot, no
   network) it is the uptime since reset, so early lines still carry a usable time base. */

/* tunables, override from platformio.ini build flags (-DLOG_RING_BYTES=8192, -DLOG_MAX_LINES=100) */
#ifndef LOG_RING_BYTES
  #define LOG_RING_BYTES 6144  /* bytes of console text kept in RAM; ~30 bytes per line covers LOG_MAX_LINES lines */
#endif
#ifndef LOG_MAX_LINES
  #define LOG_MAX_LINES 200    /* newest lines the web window replays */
#endif
/* where the captured text goes besides the web page: the console/UART is written only when this is set, and
   the default follows the build type. Debug builds see the same text on the serial console; release builds
   keep the capture and the page only, so a release build that never calls Serial.begin() and has no usable
   console still captures and still shows the window. */
#ifndef LOG_TO_CONSOLE
  #ifdef CFG_DEBUG
    #define LOG_TO_CONSOLE 1
  #else
    #define LOG_TO_CONSOLE 0
  #endif
#endif
/* how loud the capture is at build time: 0 = failures and state changes, 1 = the running values, 2 = the
   per-field dumps. Levels 0 and 1 compile into every build; level 2 only into debug builds, because those
   dumps cost their print sites in the release flash. Raise LOG_LEVEL_BUILD to carry more text in a
   release image; the web command log_level selects 0/1 at runtime without recompiling. */
#ifndef LOG_LEVEL_BUILD
  #ifdef CFG_DEBUG
    #define LOG_LEVEL_BUILD 2
  #else
    #define LOG_LEVEL_BUILD 1
  #endif
#endif
#if (LOG_RING_BYTES < 256) || (LOG_RING_BYTES > 65535)
  #error "LOG_RING_BYTES must be between 256 and 65535 bytes"
#endif
#if (LOG_MAX_LINES < 1) || (LOG_MAX_LINES > 512)
  #error "LOG_MAX_LINES must be between 1 and 512 lines"
#endif

#include <stddef.h>
#include <stdint.h>

/*===================================================================================================================*/
/* ring buffer API, implemented in print_log.cpp                                                                 */
/*===================================================================================================================*/
void logCaptureByte(uint8_t c);                            /* called per byte from the tee below */
void logCaptureBytes(const uint8_t* buffer, size_t size);  // NOLINT: array parameter, must stay a reference
void logClear(void);                                       /* drop what is captured, start a fresh window */
uint32_t logBytesCaptured(void);                           /* bytes captured since boot or last clear */
uint32_t logBytesVisible(void);                            /* bytes of that which still fits in the ring */

/* a byte-addressed view of the newest console text; indexes count from the first captured byte */
char logByteAt(uint32_t index);
bool logLineStart(uint32_t index);                         /* is the byte in front of index a line feed? */

struct LogWindow {
  uint32_t linesTotal;  /* complete (line feed terminated) lines in the ring */
  uint32_t linesShown;  /* complete lines the window shows, LOG_MAX_LINES at the latest */
  uint32_t start;       /* index of the first byte the window shows */
  uint32_t end;         /* index one past the last byte the window shows */
};

/* snapshot of the newest LOG_MAX_LINES lines; taken before the window is streamed so that bytes written while the
   page is sent cannot change what has already been shown */
void logWindowSnapshot(LogWindow* window, uint16_t maxLines);  // NOLINT: out parameter, cannot be a reference

/* append one console byte to an HTML text run as it is shown in the window: carriage returns are dropped, the
   three markup significant bytes become their entities. The caller keeps six spare bytes in the row. */
void logHtmlAppend(char* row, size_t* rowLen, char c);  // NOLINT: out parameters

/*===================================================================================================================*/
/* print level                                                                                                   */
/*===================================================================================================================*/
/* The print levels, named by what they mean rather than by a bare number. Every print site carries the level of
   text it emits; the tee below drops a line's bytes - for the capture and for the console alike - while its level
   is above the runtime level, so a line the level silences never exists as text anywhere, and no line is
   reformatted or re-evaluated.

   level ERROR   a malfunction, or the fallback to default values because the real data is missing: the device
                 cannot do what it is supposed to until this is fixed. Compiled into every build, always captured.
   level WARNING an issue that should be solved (a buffer too small, malformed input, a state worth acting on):
                 the device keeps working, but a developer should look at it.
   level INFO    everything else: the running values, the boot table, the per-field dumps - the routine telemetry.

   The three levels map onto the build/toolchain gates like this:
     INFO    compiled when LOG_LEVEL_BUILD >= 2 (the field dumps and the boot table, usually debug-only)
     WARNING compiled when LOG_LEVEL_BUILD >= 1 (a warning is always compiled as soon as any telemetry is)
     ERROR   compiled into every build (it is the minimum the device can say about itself)
   A site above level 1 emits one line; after the line feed the next line defaults back to INFO. */
#define LOG_ERROR   0
#define LOG_WARNING 1
#define LOG_INFO    2

/* the runtime level the tee compares against; one byte of RAM, settable from the web page (log_level command) */
extern uint8_t logRuntimeLevel;      /* defined in print_log.cpp; ERROR = malfunctions only, INFO = everything */
void logSetLevel(uint8_t level);     /* the level of the line printed next; resets to INFO after a line feed */
/*===================================================================================================================*/
/* console tee                                                                                                   */
/*===================================================================================================================*/
#include <Arduino.h>

/* the real console stream, resolved before the redirect below is visible */
static inline HardwareSerial& logConsoleStream(void) {
  return Serial;
}

class ConsoleTee : public Print {
 public:
  /* the capture point: store the bytes the software sends, then pass them on untouched.
     The return value reports the capture, not the UART: a release build compiles Serial.begin() out, so the core
     console has no stream and refuses every byte. Reporting the refused write back would stop the chunked print
     paths inside Print (Print::print(__FlashStringHelper*) and Print::write(buffer,len) break on a zero return) and
     the rest of the bytes would never reach the capture. Nothing in the project reads the console's write count. */
  size_t write(uint8_t c) override {
    if (lineLevel > logRuntimeLevel) {
      return 1;  /* a level the runtime silenced: no bytes exist, not in the capture and not on the console */
    }
    if (lineStart) {
      emitTimestamp();
      lineStart = false;
    }
    flushSegment(&c, 1);
    if (c == '\n') {
      lineLevel = LOG_INFO;  /* the next line defaults to INFO */
      lineStart = true;
    }
    return 1;
  }
  size_t write(const uint8_t* buffer, size_t size) override {  // NOLINT: array parameter
    if (lineLevel > logRuntimeLevel) {
      return size;
    }
    /* walk the chunk line by line so a multi-line write gets a prefix on each of its lines */
    size_t segment = 0;
    for (size_t i = 0; i < size; i++) {
      if (lineStart) {
        emitTimestamp();
        lineStart = false;
      }
      if (buffer[i] == '\n') {
        flushSegment(buffer + segment, i - segment + 1);
        lineLevel = LOG_INFO;  /* the next line defaults to INFO */
        lineStart = true;
        segment = i + 1;
      }
    }
    if (segment < size) {
      flushSegment(buffer + segment, size - segment);
    }
    return size;
  }
  /* the level of the line printed next, set by the sites above level 1 */
  void setLevel(uint8_t level) {
    lineLevel = level;
  }
  /* console configuration belongs to the real stream, the tee only forwards */
  void begin(uint32_t baud) {
#if LOG_TO_CONSOLE
    logConsoleStream().begin(baud);
#endif
  }
  void flush(void) override {
#if LOG_TO_CONSOLE
    logConsoleStream().flush();
#endif
  }

 private:
  /* hand a run of bytes to the capture and the console without going through the tee again */
  void flushSegment(const uint8_t* buffer, size_t size) {  // NOLINT: array parameter
    logCaptureBytes(buffer, size);
#if LOG_TO_CONSOLE
    logConsoleStream().write(buffer, size);
#endif
  }
  /* the [hh:mm:ss] line prefix, implemented in print_log.cpp; bypasses the tee so it cannot recurse */
  void emitTimestamp(void);

  uint8_t lineLevel = LOG_INFO;   /* level of the line being written; kept in the tee so no call site has to reset it */
  bool lineStart = true;          /* the next byte starts a line and gets the timestamp prefix */
};

/* defined in print_log.cpp, one instance for the whole program */
extern ConsoleTee consoleTee;

/*===================================================================================================================*/
/* from here on the identifier Serial means the tee (library headers must be included before this header)         */
/*===================================================================================================================*/
#if CFG_BOARD_ESP32
/* the ESP32 core already maps Serial to Serial0 with a macro; undo that first */
  #ifdef Serial
    #undef Serial
  #endif
#endif
#ifndef LOG_NO_CONSOLE_TEE
  #define Serial consoleTee
#endif

#endif  // PRINT_LOG_H_
