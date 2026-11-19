#ifndef PRINT_LOG_H_
#define PRINT_LOG_H_

/*===================================================================================================================*/
/* console output capture                                                                                        */
/*===================================================================================================================*/
/* Every byte the program sends to the console (Serial.print / Serial.println / Serial.printf) is copied into a
   RAM ring buffer before it is handed to the UART, so the status page can replay the console in a window of its own
   at the bottom (htmlWindowConsole() in main.cpp).

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
   Include this header last, and never spell the bare identifier Serial after it. */

/* tunables, override from platformio.ini build flags (-DLOG_RING_BYTES=8192, -DLOG_MAX_LINES=100) */
#ifndef LOG_RING_BYTES
  #define LOG_RING_BYTES 6144  /* bytes of console text kept in RAM; ~30 bytes per line covers LOG_MAX_LINES lines */
#endif
#ifndef LOG_MAX_LINES
  #define LOG_MAX_LINES 200    /* newest lines the web window replays */
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
/* console tee                                                                                                 */
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
    logCaptureByte(c);
    logConsoleStream().write(c);
    return 1;
  }
  size_t write(const uint8_t* buffer, size_t size) override {  // NOLINT: array parameter
    logCaptureBytes(buffer, size);
    logConsoleStream().write(buffer, size);
    return size;
  }
  /* console configuration belongs to the real stream, the tee only forwards */
  void begin(unsigned long baud) {
    logConsoleStream().begin(baud);
  }
  void flush(void) override {
    logConsoleStream().flush();
  }
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
