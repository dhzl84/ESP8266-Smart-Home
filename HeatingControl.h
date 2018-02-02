#ifndef HEATINGCONTROL_H_
#define HEATINGCONTROL_H_

#define minTargetTemp 150
#define maxTargetTemp 250

class HeatingControl
{
public:
   HeatingControl(void);
   ~HeatingControl(void);
   void init(void);
   void setHeatingEnabled(bool value);
   void setTargetTemperature(int value);
   void increaseTargetTemperature(unsigned int value);
   void decreaseTargetTemperature(unsigned int value);
   void resetTransmitRequest();
   void resetNewData();
   bool getHeatingEnabled(void);
   int  getTargetTemperature(void);
   bool getNewData();
   bool getHeatingAllowed();
   void setHeatingAllowed(bool value);
   void toggleHeatingAllowed();
private:
   bool heatingEnabled;
   int  targetTemperature;
   bool newData;
   bool heatingAllowed;
};



#endif /* HEATINGCONTROL_H_ */
