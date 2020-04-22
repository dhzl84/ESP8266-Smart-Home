#include "main.h"

float    intToFloat(int16_t intValue)     { return (static_cast<float>(intValue/10.0f)); }
int16_t  floatToInt(float floatValue)     { return (static_cast<int16_t>(floatValue * 10)); }
String boolToStringOnOff(bool boolean)    { return (boolean == true ? "on" : "off"); }
String boolToStringHeatOff(bool boolean)  { return (boolean == true ? "heat" : "off"); }
String sensErrorToString(bool boolean)    { return (boolean == true ? "error" : "ok"); }

/* Thanks to Tasmota  for timer functions */
int32_t TimeDifference(uint32_t prev, uint32_t next) {
  // Return the time difference as a signed value, taking into account the timers may overflow.
  // Returned timediff is between -24.9 days and +24.9 days.
  // Returned value is positive when "next" is after "prev"
  int32_t signed_diff = 0;
  // To cast a value to a signed int32_t, the difference may not exceed half 0xffffffffUL (= 4294967294)
  const uint32_t half_max_unsigned_long = 2147483647u;  // = 2^31 -1
  if (next >= prev) {
    const uint32_t diff = next - prev;
    if (diff <= half_max_unsigned_long) {                    // Normal situation, just return the difference.
      signed_diff = static_cast<int32_t>(diff);                 // Difference is a positive value.
    } else {
      // prev has overflow, return a negative difference value
      signed_diff = static_cast<int32_t>((0xffffffffUL - next) + prev + 1u);
      signed_diff = -1 * signed_diff;
    }
  } else {
    // next < prev
    const uint32_t diff = prev - next;
    if (diff <= half_max_unsigned_long) {                    // Normal situation, return a negative difference value
      signed_diff = static_cast<int32_t>(diff);
      signed_diff = -1 * signed_diff;
    } else {
      // next has overflow, return a positive difference value
      signed_diff = static_cast<int32_t>((0xffffffffUL - prev) + next + 1u);
    }
  }
  return signed_diff;
}

int32_t TimePassedSince(uint32_t timestamp) {
  // Compute the number of milliSeconds passed since timestamp given.
  // Note: value can be negative if the timestamp has not yet been reached.
  return TimeDifference(timestamp, millis());
}

bool TimeReached(uint32_t timer) {
  // Check if a certain timeout has been reached.
  const int32_t passed = TimePassedSince(timer);
  return (passed >= 0);
}

void SetNextTimeInterval(volatile uint32_t *timer, const uint32_t step) {
  *timer += step;
  const int32_t passed = TimePassedSince(*timer);
  if (passed < 0) { return; }   // Event has not yet happened, which is fine.
  if (static_cast<uint32_t>(passed) > step) {
    // No need to keep running behind, start again.
    *timer = millis() + step;
    return;
  }
  // Try to get in sync again.
  *timer = millis() + (step - passed);
}

/* sensor calibration parameters are received and stored as a string <offset>;<factor> */
bool splitSensorDataString(String sensorCalib, int16_t *offset, int16_t *factor) {
  bool ret = false;
  String delimiter = ";";
  size_t pos = 0;

  pos = (sensorCalib.indexOf(delimiter));

  /* don't accept empty substring or strings without delimiter */
  if ((pos > 0) && (pos < UINT32_MAX)) {
    ret = true;
    *offset = (sensorCalib.substring(0, pos)).toInt();
    *factor = (sensorCalib.substring(pos+1)).toInt();
  } else {
    #ifdef CFG_DEBUG
    Serial.println("Malformed sensor calibration string >> calibration update denied");
    #endif
  }

  #ifdef CFG_DEBUG
  Serial.println("Delimiter: " + delimiter);
  Serial.println("Position of delimiter: " + String(pos));
  Serial.println("Offset: " + String(*offset));
  Serial.println("Factor: " + String(*factor));
  #endif
  return ret;
}

String millisFormatted(void) {
  char char_buffer[16];
  uint32_t t = millis()/1000u;

  uint32_t d = t / 86400u;
  t = t % 86400;
  uint32_t h = t / 3600u;
  t = t % 3600;
  uint16_t m = t / 60;
  uint16_t s = t % 60;

  (void) snprintf(char_buffer, sizeof(char_buffer), "%uT %02u:%02u:%02u", d, h, m, s);
  #ifdef CFG_DEBUG
  Serial.println(char_buffer);
  #endif  // CFG_DEBUG

  return String(char_buffer);
}

String wifiStatusToString(wl_status_t status) {
  String ret;

  switch (status) {
    case WL_IDLE_STATUS:
      ret = "WL_IDLE_STATUS";
      break;
    case WL_NO_SSID_AVAIL:
      ret = "WL_NO_SSID_AVAIL";
      break;
    case WL_SCAN_COMPLETED:
      ret = "WL_SCAN_COMPLETED";
      break;
    case WL_CONNECTED:
      ret = "WL_CONNECTED";
      break;
    case WL_CONNECT_FAILED:
      ret = "WL_CONNECT_FAILED";
      break;
    case WL_CONNECTION_LOST:
      ret = "WL_CONNECTION_LOST";
      break;
    case WL_DISCONNECTED:
      ret = "WL_DISCONNECTED";
      break;
    case WL_NO_SHIELD:
      ret = "WL_NO_SHIELD";
      break;
    default:
      ret = "value unknown";
      break;
  }
  return ret;
}

bool splitHtmlCommand(String sInput, String *key, String *value) {
  bool ret = false;
  String delimiter = ":";
  size_t pos = 0;

  pos = (sInput.indexOf(delimiter));

  /* don't accept empty substring or strings without delimiter */
  if ((pos > 0) && (pos < UINT32_MAX)) {
    ret = true;
    *key = (sInput.substring(0, pos));
    *value = (sInput.substring(pos+1));
  } else {
    #ifdef CFG_DEBUG
    Serial.println("Malformed HTML Command string");
    #endif
  }
  return ret;
}

String getEspChipId(void) {
  #if CFG_BOARD_ESP32
    String str = (String(uint32_t(ESP.getEfuseMac() & 0x0000FFFF00000000), HEX) + String(uint32_t(ESP.getEfuseMac() & 0x00000000FFFFFFFF), HEX));
  #elif CFG_BOARD_ESP8266
    String str = String(ESP.getChipId(), HEX);
  #else
    String str = "undefined";
  #endif  /* CFG_BOARD_ESP32 */

  return str;
}

#if CFG_BOARD_ESP32
void listDir(fs::FS &fs, const char * dirname, uint8_t levels) {  // NOLINT
  Serial.printf("Listing directory: %s\r\n", dirname);

  File root = fs.open(dirname);
  if (!root) {
    Serial.println("- failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println(" - not a directory");
    return;
  }

  File file = root.openNextFile();
  while (file == true) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listDir(fs, file.name(), levels -1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("\tSIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}

String getEspResetReason(RESET_REASON reason) {
  String str_ret = "NO_MEAN";
  switch (reason) {
    case 1 :  str_ret = "POWERON_RESET: Vbat power on reset"; break;
    case 3 :  str_ret = "SW_RESET: Software reset digital core"; break;
    case 4 :  str_ret = "OWDT_RESET: Software reset digital core"; break;
    case 5 :  str_ret = "DEEPSLEEP_RESET: Deep Sleep reset digital core"; break;
    case 6 :  str_ret = "SDIO_RESET: Reset by SLC module, reset digital core"; break;
    case 7 :  str_ret = "TG0WDT_SYS_RESET: Timer Group0 Watch dog reset digital core"; break;
    case 8 :  str_ret = "TG1WDT_SYS_RESET: Timer Group1 Watch dog reset digital core"; break;
    case 9 :  str_ret = "RTCWDT_SYS_RESET: RTC Watch dog Reset digital core"; break;
    case 10 : str_ret = "INTRUSION_RESET: Instrusion tested to reset CPU"; break;
    case 11 : str_ret = "TGWDT_CPU_RESET: Time Group reset CPU"; break;
    case 12 : str_ret = "SW_CPU_RESET: Software reset CPU"; break;
    case 13 : str_ret = "RTCWDT_CPU_RESET: RTC Watch dog Reset CPU"; break;
    case 14 : str_ret = "EXT_CPU_RESET: for APP CPU, reseted by PRO CPU"; break;
    case 15 : str_ret = "RTCWDT_BROWN_OUT_RESET; Reset when the vdd voltage is not stable"; break;
    case 16 : str_ret = "RTCWDT_RTC_RESET: RTC Watch dog reset digital core and rtc module"; break;
    default : str_ret = "NO_MEAN";
  }
  return str_ret;
}
#endif  /* CFG_BOARD_ESP32 */

DiffTime::DiffTime() \
  : time_start_(0), \
    time_end_(0), \
    time_duration_(0), \
    time_duration_min_(UINT16_MAX), \
    time_duration_max_(0), \
    time_duration_mean_(0), \
    time_duration_mean_buffer_(0), \
    time_count_(0) {
}

DiffTime::~DiffTime() {
  /* do nothing */
}

void DiffTime::set_time_start(void) {
  time_start_ = millis();
}

void DiffTime::set_time_end(void) {
  time_end_ = millis();
  time_duration_ = (time_end_ - time_start_);

  if (time_duration_ > time_duration_max_) {
    time_duration_max_ = time_duration_;
  }
  if (time_duration_ < time_duration_min_) {
    time_duration_min_ = time_duration_;
  }
  if (time_count_ < 1000) {
    if ((time_duration_mean_buffer_ + time_duration_) > UINT32_MAX) {
      /* avoid buffer overflow */
      time_count_ = 0;
      time_duration_mean_buffer_ = 0;
    } else {
      time_duration_mean_buffer_ += time_duration_;
      time_count_++;
    }
  } else {
    time_duration_mean_ = static_cast<uint16_t>(time_duration_mean_buffer_ / time_count_);
    time_duration_mean_buffer_ = 0;
    time_count_ = 0;
    #ifdef CFG_DEBUG_RUNTIME
    Serial.println("Duration (last 1000): " + String(static_cast<float>(time_duration_mean_)) + " ms");
    Serial.println("Duration min: " + String(time_duration_min_) + " ms");
    Serial.println("Duration max: " + String(time_duration_max_) + " ms");
    #endif /* CFG_DEBUG */
  }
}

uint16_t DiffTime::get_time_duration(void)      { return time_duration_; }
uint16_t DiffTime::get_time_duration_mean(void) { return time_duration_mean_; }
uint16_t DiffTime::get_time_duration_min(void)  { return time_duration_min_; }
uint16_t DiffTime::get_time_duration_max(void)  { return time_duration_max_; }
