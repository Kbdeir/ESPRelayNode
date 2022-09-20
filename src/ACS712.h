#ifndef ACS712_h
#define ACS712_h

#include <Arduino.h>

#ifdef ESP32
#define ADC_SCALE 4095.0
#else
#define ADC_SCALE 1023.0
#endif

#define VREF 5.0
#define DEFAULT_FREQUENCY 50
#define CALIBRATION_SAMPLE_SIZE 100
#define ADCREAD_SAMPLE_SIZE	10
const float alpha = 0.1f;

enum ACS712_type {ACS712_05B, ACS712_20A, ACS712_30A};

class ACS712 {
public:
	//static char result_frame[20];
	float ACS_frame_result;
	int ACS_threshold_elasticity_count = 0;
	ACS712(ACS712_type type, uint8_t _pin);
	int calibrate();
	void setZeroPoint(int _zero);
	void setSensitivity(float sens);
	float getCurrentDC();
	float getCurrentAC(uint16_t frequency = 50);
	float getCurrentAC_Alfafilter(uint16_t frequency = 50);

	void watch();

private:
	int zero = int(ADC_SCALE/2);
	float sensitivity;
	uint8_t pin;

	long lastSendMillis = 0;
	float adcValue = 0.0f;
	float mV = 0.0f;
	float eCurrent = 0.0f;
	float currentValue, previousValue = 0.0f;

	int getVPP();

};

#endif
