#ifndef HCSR04_h
#define HCSR04_h

#include "Arduino.h"

class HCSR04
{
  public:
    HCSR04(int trigger, int echo);
	  HCSR04(int trigger, int echo, int minRange, int maxRange);
    long distance;
    long echoInMicroseconds();
    long distanceInCentimeters();
    void ToSerial();
    String ToString();
    int16_t distanceInCms();    
  private:
    int _trigger;
    int _echo;
    int _minRange = -1;
    int _maxRange = -1;
    byte triginterval_lowDelay_us = 5;
    byte triginterval_HighDelay_us = 15;
};

#endif