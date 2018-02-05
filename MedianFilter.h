#ifndef MEDIANFILTER_H_
#define MEDIANFILTER_H_

#include "Arduino.h"

#define CFG_MEDIAN_QUEUE_SIZE 15 /* number of Value samples used for median calculation */
class MedianFilter;

class MedianFilter
{
public:
	MedianFilter(void);
	~MedianFilter(void);
   void init(void);
   void pushValue(int raw_Value);
   int getFilteredValue(void);
   bool getRawSampleValue(int sampleID, int *rawSampleValue);
private:
   bool ValueQueueFilled;
   int valueSampleID;
   int ValueQueue[CFG_MEDIAN_QUEUE_SIZE];
};

#endif /* MEDIANFILTER_H_ */
