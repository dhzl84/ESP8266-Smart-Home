#include "main.h"

float    intToFloat(int16_t intValue)     { return (static_cast<float>(intValue/10.0)); }
int16_t  floatToInt(float floatValue)     { return (static_cast<int16_t>(floatValue * 10)); }
String boolToStringOnOff(bool boolean)    { return (boolean == true ? "on" : "off"); }
String boolToStringHeatOff(bool boolean)  { return (boolean == true ? "heat" : "off"); }

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

void SetNextTimeInterval(uint32_t& timer, const uint32_t step) {  //NOLINT: pass by reference
  timer += step;
  const int32_t passed = TimePassedSince(timer);
  if (passed < 0) { return; }   // Event has not yet happened, which is fine.
  if (static_cast<uint32_t>(passed) > step) {
    // No need to keep running behind, start again.
    timer = millis() + step;
    return;
  }
  // Try to get in sync again.
  timer = millis() + (step - passed);
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

char* millisFormatted(void) {
  static char str[16];
  uint32_t t = millis()/1000;

  uint32_t d = t / 86400;
  t = t % 86400;
  uint32_t h = t / 3600;
  t = t % 3600;
  uint16_t m = t / 60;
  uint16_t s = t % 60;

  snprintf(str, sizeof(str), "%uT %02u:%02u:%02u", d, h, m, s);
  #ifdef CFG_DEBUG
  Serial.println(str);
  #endif  // CFG_DEBUG

  return str;
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
