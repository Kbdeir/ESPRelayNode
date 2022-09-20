#include "Arduino.h"
#include "HCSR04.h"


HCSR04::HCSR04(int trigger, int echo)
{
    pinMode(trigger, OUTPUT);
    pinMode(echo, INPUT);
    _trigger = trigger;
    _echo = echo;
}

HCSR04::HCSR04(int trigger, int echo, int minRange, int maxRange)
{
    pinMode(trigger, OUTPUT);
    pinMode(echo, INPUT);
    _trigger = trigger;
    _echo = echo;
	_minRange = minRange;
    _maxRange = maxRange;
}

long HCSR04::echoInMicroseconds()
{
    long res = 0;
    pinMode(_trigger, OUTPUT);
    pinMode(_echo, INPUT);
    digitalWrite(_trigger, LOW);
     delayMicroseconds(5);    
    digitalWrite(_trigger, HIGH);
    delayMicroseconds(15);
    digitalWrite(_trigger, LOW);  
    return pulseIn(_echo, HIGH);
}

long HCSR04::distanceInCentimeters()
{
    long duration = echoInMicroseconds();
    
    // Given the speed of sound in air is 332m/s = 3320cm/s = 0.0332cm/us). cm = (duration/2) / 29.1;     // Divide by 29.1 or multiply by 0.0343
    distance = (duration / 2) / 29.1;
	
	if (_minRange == -1 && _maxRange == -1)
	{
		return distance;
	}
	
	if (distance > _minRange && distance < _maxRange)
	{
		return distance;
	}	
	
	return -1;
}

void HCSR04::ToSerial()
{
    Serial.println(ToString());
}

int16_t HCSR04::distanceInCms()
{
    return (distanceInCentimeters() / 100);
}

String HCSR04::ToString()
{
    String distanceString = "{\"Protocol\":\"Bifrost\",\"Device\":\"HCSR04\",\"Driver version\":\"2.0.0\",\"Properties\":{\"Distance\":<<DISTANCE>>}}";
    distanceString.replace("<<DISTANCE>>", String(distanceInCentimeters()));
    return distanceString;
}