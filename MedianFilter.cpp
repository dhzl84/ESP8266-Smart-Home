#include "MedianFilter.h"

MedianFilter::MedianFilter()
{
   MedianFilter::init();
}

MedianFilter::~MedianFilter()
{
   /* do nothing */
}

void MedianFilter::init()
{
   ValueQueueFilled = false;
   valueSampleID = 0;
   for (int i=0; i<CFG_MEDIAN_QUEUE_SIZE; i++)
   {
      ValueQueue[i] = (int)0;
   }
}

void MedianFilter::pushValue(int raw_Value)
{
   ValueQueue[valueSampleID] = raw_Value;
   valueSampleID++;
   if (valueSampleID == CFG_MEDIAN_QUEUE_SIZE)
   {
      valueSampleID = 0;
      if (ValueQueueFilled == false)
      {
         ValueQueueFilled = true;
      }
   }
}

int MedianFilter::getFilteredValue(void)
{
   float tempValue = (int) 0;

   if (ValueQueueFilled == true)
   {
      for (int i=0; i<CFG_MEDIAN_QUEUE_SIZE; i++)
      {
         tempValue += (ValueQueue[i]);
      }
      tempValue = (tempValue / (int) (CFG_MEDIAN_QUEUE_SIZE));
   }
   else /* return partially filtered value until queue is filled */
   {
      if (valueSampleID > 0)
      {
         for (int i=0; i<valueSampleID; i++)
         {
            tempValue += (ValueQueue[i]);
         }
         tempValue = (tempValue / valueSampleID);
      }
   }
   return tempValue;
}

bool MedianFilter::getRawSampleValue(int sampleID, int *rawSampleValue)
{
   if ((sampleID < 0) || (sampleID >= CFG_MEDIAN_QUEUE_SIZE))
   {
      Serial.println("out of bounds access to MedianFilter::getRawSampleValue");
      return false; /* index out of bounds */
   }
   else
   {
      *rawSampleValue = ValueQueue[sampleID];
      return true;
   }
}
