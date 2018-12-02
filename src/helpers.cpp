#include "main.h"

float  intToFloat(int intValue)           { return ((float)(intValue/10.0)); }
int    floatToInt(float floatValue)       { return ((int)(floatValue * 10)); }
String boolToStringOnOff(bool boolean)    { return (boolean == true ? "on" : "off"); }
String boolToStringHeatOff(bool boolean)  { return (boolean == true ? "heat" : "off"); }

/* Thanks to Tasmota  for timer functions */
long TimeDifference(unsigned long prev, unsigned long next)
{
  // Return the time difference as a signed value, taking into account the timers may overflow.
  // Returned timediff is between -24.9 days and +24.9 days.
  // Returned value is positive when "next" is after "prev"
  long signed_diff = 0;
  // To cast a value to a signed long, the difference may not exceed half 0xffffffffUL (= 4294967294)
  const unsigned long half_max_unsigned_long = 2147483647u;  // = 2^31 -1
  if (next >= prev) {
    const unsigned long diff = next - prev;
    if (diff <= half_max_unsigned_long) {                    // Normal situation, just return the difference.
      signed_diff = static_cast<long>(diff);                 // Difference is a positive value.
    } else {
      // prev has overflow, return a negative difference value
      signed_diff = static_cast<long>((0xffffffffUL - next) + prev + 1u);
      signed_diff = -1 * signed_diff;
    }
  } else {
    // next < prev
    const unsigned long diff = prev - next;
    if (diff <= half_max_unsigned_long) {                    // Normal situation, return a negative difference value
      signed_diff = static_cast<long>(diff);
      signed_diff = -1 * signed_diff;
    } else {
      // next has overflow, return a positive difference value
      signed_diff = static_cast<long>((0xffffffffUL - prev) + next + 1u);
    }
  }
  return signed_diff;
}

long TimePassedSince(unsigned long timestamp)
{
  // Compute the number of milliSeconds passed since timestamp given.
  // Note: value can be negative if the timestamp has not yet been reached.
  return TimeDifference(timestamp, millis());
}

bool TimeReached(unsigned long timer)
{
  // Check if a certain timeout has been reached.
  const long passed = TimePassedSince(timer);
  return (passed >= 0);
}

void SetNextTimeInterval(unsigned long& timer, const unsigned long step)
{
  timer += step;
  const long passed = TimePassedSince(timer);
  if (passed < 0) { return; }   // Event has not yet happened, which is fine.
  if (static_cast<unsigned long>(passed) > step) {
    // No need to keep running behind, start again.
    timer = millis() + step;
    return;
  }
  // Try to get in sync again.
  timer = millis() + (step - passed);
}

/* sensor calibration parameters are received and stored as a string <offset>;<factor> */
bool splitSensorDataString(String sensorCalib, int *offset, int *factor)
{
   bool ret = false;
   String delimiter = ";";
   size_t pos = 0;

   pos = (sensorCalib.indexOf(delimiter));

   /* don't accept empty substring or strings without delimiter */
   if ((pos > 0) && (pos < UINT32_MAX))
   {
      ret = true;
      *offset = (sensorCalib.substring(0, pos)).toInt();
      *factor = (sensorCalib.substring(pos+1)).toInt();
   }
   #ifdef CFG_DEBUG
   else
   {
      Serial.println("Malformed sensor calibration string >> calibration update denied");
   }

   Serial.println("Delimiter: " + delimiter);
   Serial.println("Position of delimiter: " + String(pos));
   Serial.println("Offset: " + String(*offset));
   Serial.println("Factor: " + String(*factor));
   #endif
   return ret;
}