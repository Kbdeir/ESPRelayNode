#include "ACS712.h"

ACS712::ACS712(ACS712_type type, uint8_t _pin) {
	pin = _pin;

	// Different models of the sensor have their sensitivity:
	switch (type) {
		case ACS712_05B:
			sensitivity = 0.185;
			break;
		case ACS712_20A:
			sensitivity = 0.100;
			break;
		case ACS712_30A:
			sensitivity = 0.066;
			break;
	}
}

int ACS712::calibrate() {
	uint16_t acc = 0;
	for (int i = 0; i < CALIBRATION_SAMPLE_SIZE; i++) {
		acc += analogRead(pin);
	}
	zero = acc / CALIBRATION_SAMPLE_SIZE;
	return zero;
}

void ACS712::setZeroPoint(int _zero) {
	zero = _zero;
}

void ACS712::setSensitivity(float sens) {
	sensitivity = sens;
}

float ACS712::getCurrentDC() {
	int16_t acc = 0;
	for (int i = 0; i < ADCREAD_SAMPLE_SIZE; i++) {
		acc += analogRead(pin) - zero;
	}
	float I = (float)acc / ADCREAD_SAMPLE_SIZE / ADC_SCALE * VREF / sensitivity;
	return I;
}

float ACS712::getCurrentAC(uint16_t frequency) {
	uint32_t period = 1000000 / frequency;
	uint32_t t_start = micros();

	uint32_t Isum = 0, measurements_count = 0;
	int32_t Inow;

	while (micros() - t_start < period) {
		Inow = analogRead(pin) - zero;
		Isum += Inow*Inow;
		measurements_count++;
	}

	float Irms = sqrt(Isum / measurements_count) / ADC_SCALE * VREF / sensitivity;
	return Irms;
}


float ACS712::getCurrentAC_Alfafilter(uint16_t frequency) {
	// This function samples one cycle of the ac current when called and return the read current value after applying an alfa filer

	uint32_t period = 1000000 / frequency; // 1 second / 50Hz = 20ms = 1 cycle
	uint32_t t_start = micros();


	while (micros() - t_start < period) {
    	adcValue = getVPP();
		mV = (adcValue / ADC_SCALE) * (VREF*1000); // mV  = adcValue * 4.8828f;              //
		eCurrent = ((mV - ((VREF*1000)/2)) / sensitivity);  // 2500 = voltageRef/2, ACS712 5A=1.85f, 20A=1.0f, 30A=0.66f
		//amp = eCurrent / 141.42f;           // eCurrent/(sqrt(2); // sqrt(2) = 1.4142

		// Apply alpha filter
		currentValue = 0.1f * (eCurrent / 141.42f) + 0.9f * previousValue; // alpha = 0.1f, 0.9f = (1-alpha)
	}
    // Manual fixup of readings
    if (currentValue<=1.5)
      currentValue = currentValue * 0.93;
    if (currentValue>1.5 && currentValue<=2.5)
      currentValue = currentValue * 0.94;
    else if (currentValue>2.5 && currentValue<=8.0)
      currentValue = currentValue * 0.97;
    else if (currentValue>8.0 && currentValue<=10.0)
      currentValue = currentValue * 0.98;
    else
      currentValue = currentValue * 0.985;

    previousValue = currentValue;	
	return currentValue;
}


int ACS712::getVPP()
{
  int maxValue = 0;

  int c=250;
  while(c-->0) {
       int readValue = analogRead(pin);

       if (readValue > maxValue)
           maxValue = readValue;
   }

   return maxValue;
 }



 void ACS712::watch() {
// this fuction contineousely reads adc and it should be called within the main loop function

    adcValue = getVPP();
    mV = (adcValue / ADC_SCALE) * (VREF*1000); //adcValue * 4.8828f;              // (adcValue / 1024.0f) * 5000.0f; // mV
    eCurrent = ((mV - ((VREF*1000)/2)) / sensitivity); // eCurrent = ((mV - 2500.0f) / 0.66f);  // 2500 = voltageRef/2, ACS712 5A=1.85f, 20A=1.0f, 30A=0.66f
    //amp = eCurrent / 141.42f;           // eCurrent/(sqrt(2); // sqrt(2) = 1.4142

    // Apply alpha filter
    currentValue = 0.1f * (eCurrent / 141.42f) + 0.9f * previousValue; // alpha = 0.1f, 0.9f = (1-alpha)

    // Manual fixup of readings
    if (currentValue<=1.5)
      currentValue = currentValue * 0.93;
    if (currentValue>1.5 && currentValue<=2.5)
      currentValue = currentValue * 0.94;
    else if (currentValue>2.5 && currentValue<=8.0)
      currentValue = currentValue * 0.97;
    else if (currentValue>8.0 && currentValue<=10.0)
      currentValue = currentValue * 0.98;
    else
      currentValue = currentValue * 0.985;

    previousValue = currentValue;

    // Last send
    long diff = millis() - lastSendMillis;

    // If 5000ms have gone by then send packet
    if (abs(diff)> 4999) { // every five seconds

        // Syntax: dtostrf(floatVar, minStringWidthIncDecimalPoint, numVarsAfterDecimal, charBuf)
        //dtostrf(currentValue, 2, 2, &result_frame[0]);
        //result_frame[16] = '\0';
        lastSendMillis = millis();
		ACS_frame_result = currentValue;
    } 

//    delay(250);
} 