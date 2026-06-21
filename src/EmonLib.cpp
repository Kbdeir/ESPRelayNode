/*
  Emon.cpp - Library for openenergymonitor
  Created by Trystan Lea, April 27 2010
  GNU GPL
  modified to use up to 12 bits ADC resolution (ex. Arduino Due)
  by boredman@boredomprojects.net 26.12.2013
  Low Pass filter for offset removal replaces HP filter 1/1/2015 - RW
*/

// Proboscide99 10/08/2016 - Added ADMUX settings for ATmega1284 e 1284P (644 / 644P also, but not tested) in readVcc function

//#include "WProgram.h" un-comment for use on older versions of Arduino IDE
#include "EmonLib.h"
#ifdef ESP32
#include "RemoteDebug.h"
extern   RemoteDebug Debug;
#endif
//#include "OneButton.h"

//extern OneButton MYbutton;;

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#ifdef _ADS1X15_CURRENT_
#ifndef _ADS_ASYNC_
  #include <Wire.h>
  #include <Adafruit_ADS1X15.h>
  #ifdef _ADS1015_
    extern Adafruit_ADS1015 ads;
  #else
    extern Adafruit_ADS1115 ads;
  #endif
#else
  #include "ADS1X15.h"
  #ifdef _ADS1015_
    extern ADS1015 ADS1;
  #else
    extern ADS1115 ADS1;
  #endif
#endif
#endif

  int pSupplyVoltage;
  int SupplyVoltage ;
  int SupplyVoltageADS1115 = 6144;
  int offsetVADS = 0;
  int offsetIADS = 0;
  unsigned long offsetI = 8710; 
  unsigned long offsetV = 8710; 
  int16_t ADCCOUNTS = ADC_COUNTS;
  double V_RATIO , I_RATIO, I_V_RATIO;


void EnergyMonitor::setupparams() {

  #if defined emonTxV3
   SupplyVoltage=3300;
  #else
   SupplyVoltage = 3260;//3300; //readVcc();
  #endif

  #ifdef _ADS1X15_VOLTAGE_
    V_RATIO = VCAL *((SupplyVoltageADS1115/1000.0) / (ADC_COUNTS_ADS1X15)); //switch to ADS
    ADCCOUNTS = ADC_COUNTS_ADS1X15;
  #else
    V_RATIO = VCAL *((SupplyVoltage/1000.0) / (ADC_COUNTS)); 
    ADCCOUNTS = ADC_COUNTS;
  #endif  

  #ifdef _ADS1X15_CURRENT_
    I_RATIO = ICAL *((SupplyVoltageADS1115/1000.0) / (ADC_COUNTS_ADS1X15));
  #else
    I_RATIO = ICAL *((SupplyVoltage/1000.0) / (ADC_COUNTS));
  #endif

  I_V_RATIO = V_RATIO * I_RATIO;

  #ifdef _ADS1X15_CURRENT_
    #ifndef _ADS_ASYNC_
      #ifdef _ADS1015_
        if (ads.getGain() == ADS1X15_REG_CONFIG_PGA_6_144V) {SupplyVoltageADS1115 = 6144; offsetVADS = 544;  offsetIADS = 544; offsetI = 544; }
        if (ads.getGain() == ADS1X15_REG_CONFIG_PGA_4_096V) {SupplyVoltageADS1115 = 4096; offsetVADS = 817; offsetIADS = 817;offsetI = 817;}
        #ifdef _ADS1X15_VOLTAGE_
          if (ads.getGain() == ADS1X15_REG_CONFIG_PGA_6_144V) {  offsetV = 544; }
          if (ads.getGain() == ADS1X15_REG_CONFIG_PGA_4_096V) {  offsetV = 817;}
            ADCCOUNTS = ADC_COUNTS_ADS1X15;
        #endif
      #else
        if (ads.getGain() == ADS1X15_REG_CONFIG_PGA_6_144V) {SupplyVoltageADS1115 = 6144; offsetVADS = 8710;  offsetIADS = 8710; offsetI = 8710; }
        if (ads.getGain() == ADS1X15_REG_CONFIG_PGA_4_096V) {SupplyVoltageADS1115 = 4096; offsetVADS = 13080; offsetIADS = 13080;offsetI = 13080;}
        #ifdef _ADS1X15_VOLTAGE_
          if (ads.getGain() == ADS1X15_REG_CONFIG_PGA_6_144V) {  offsetV = 8710; }
          if (ads.getGain() == ADS1X15_REG_CONFIG_PGA_4_096V) {  offsetV = 13080;}
            ADCCOUNTS = ADC_COUNTS_ADS1X15;
        #endif
      #endif
    #else

      #ifdef _ADS1015_
        SupplyVoltageADS1115 = 6144; offsetVADS = 544;  offsetIADS = 544; offsetI = 544;
        #ifdef _ADS1X15_VOLTAGE_
          offsetV = 544;
        #endif
      #else
        SupplyVoltageADS1115 = 6144; offsetVADS = 8718;  offsetIADS = 8718; offsetI = 8718;
        #ifdef _ADS1X15_VOLTAGE_
          offsetV = 8718;
        #endif
        // SupplyVoltageADS1115 = 4096; offsetVADS = 13080; offsetIADS = 13080;offsetI = 13080;
      #endif
      ADCCOUNTS = ADC_COUNTS_ADS1X15;
    #endif
  #else
  offsetI = 1820; offsetV = 1820;
  //  offsetI = ADC_COUNTS>>1; ;   offsetV = ADC_COUNTS>>1;;
  #endif
}


int16_t EnergyMonitor::analogread_volts(unsigned int pin) {
  int16_t res = 0;
  
  #ifdef _ADS1X15_VOLTAGE_ 
    #ifdef _ADS_ASYNC_
      res = ADS1.readADC(pin); 
    #endif
    #ifndef _ADS_ASYNC_
      res=  ads.readADC_SingleEnded(pin) ;
    #endif  
  #endif 
  
  #ifndef _ADS1X15_VOLTAGE_
    res = analogRead(inPinV);
  #endif  

  return res;
}

int16_t EnergyMonitor::analogread_amps(unsigned int pin) {
  int16_t res;
  #ifdef _ADS1X15_CURRENT_ 
    #ifdef _ADS_ASYNC_
      res = ADS1.readADC(pin); 
    #endif
    #ifndef _ADS_ASYNC_
      res=  ads.readADC_SingleEnded(pin) ;
    #endif  
  #endif 
  
  #ifndef _ADS1X15_CURRENT_
   // res = analogRead(inPinI); 
  #endif  

  return res;
}

void EnergyMonitor::primeOffsets()
{
  const unsigned long startMs = millis();
  const unsigned long primeMs = 220;
  double sumVPrime = 0.0;
  double sumIPrime = 0.0;
  unsigned int samples = 0;

  while ((millis() - startMs) < primeMs) {
    sumVPrime += analogread_volts(0);
    #ifdef _ADS1X15_CURRENT_
      sumIPrime += analogread_amps(1);
    #else
      sumIPrime += analogRead(inPinI);
    #endif
    samples++;
  }

  if (samples > 0) {
    offsetV = sumVPrime / samples;
    offsetI = sumIPrime / samples;
  }

  lastFilteredV = 0.0;
  filteredV = 0.0;
  filteredI = 0.0;
  offsetsPrimed = true;
}





//--------------------------------------------------------------------------------------
// Sets the pins to be used for voltage and current sensors
//--------------------------------------------------------------------------------------
void EnergyMonitor::voltage(unsigned int _inPinV, double _VCAL, double _PHASECAL)
{
  inPinV = _inPinV;
  VCAL = _VCAL;
  PHASECAL = _PHASECAL;
  offsetV = ADC_COUNTS>>1;
  offsetsPrimed = false;
  setupparams();
}

void EnergyMonitor::current(unsigned int _inPinI, double _ICAL)
{
  inPinI = _inPinI;
  ICAL = _ICAL;
  offsetI = ADC_COUNTS>>1; 
  offsetsPrimed = false;
  setupparams();
}

//--------------------------------------------------------------------------------------
// Sets the pins to be used for voltage and current sensors based on emontx pin map
//--------------------------------------------------------------------------------------
void EnergyMonitor::voltageTX(double _VCAL, double _PHASECAL)
{
  inPinV = 2;
  VCAL = _VCAL;
  PHASECAL = _PHASECAL;
  offsetV = ADC_COUNTS>>1;
}

void EnergyMonitor::currentTX(unsigned int _channel, double _ICAL)
{
  if (_channel == 1) inPinI = 3;
  if (_channel == 2) inPinI = 0;
  if (_channel == 3) inPinI = 1;
  ICAL = _ICAL;
  offsetI = ADC_COUNTS>>1;
}

//--------------------------------------------------------------------------------------
// emon_calc procedure
// Calculates realPower,apparentPower,powerFactor,Vrms,Irms,kWh increment
// From a sample window of the mains AC voltage and current.
// The Sample window length is defined by the number of half wavelengths or crossings we choose to measure.
//--------------------------------------------------------------------------------------

/**/
void EnergyMonitor::calcVI(unsigned int crossings, unsigned int timeout)
{
  /*
  #if defined emonTxV3
  int SupplyVoltage=3300;
  #else
  int SupplyVoltage = readVcc();
  #endif
  */

  //double V_RATIO , I_RATIO;

  // setupparams();

  unsigned int crossCount = 0;                             //Used to measure number of times threshold is crossed.
  unsigned int numberOfSamples = 0;                        //This is now incremented

  if (!offsetsPrimed) {
    primeOffsets();
  }

  //-------------------------------------------------------------------------------------------------------------------------
  // 1) Waits for the waveform to be close to 'zero' (mid-scale adc) part in sin curve.
  //-------------------------------------------------------------------------------------------------------------------------
  unsigned long start = millis();    //millis()-start makes sure it doesnt get stuck in the loop if there is an error.
  // Serial.printf("\nStart of Zero Cross identification @ %u",start);
  while(1)                                   //the while loop...
  {
    startV = analogread_volts(0);


      double zeroCrossTolerance = ADCCOUNTS * 0.02;
      if (zeroCrossTolerance < 20.0) zeroCrossTolerance = 20.0;
      if ((startV < (offsetV + zeroCrossTolerance)) &&
          (startV > (offsetV - zeroCrossTolerance))) {

      //  Serial.printf("\n Zero Cross detected ----- %u", startV);
      //  Serial.printf("\n>>>>End of Zero Cross identification took @ %u ms",millis() - start);
      break;  //check its within range
    }
    if ((millis()-start)>timeout) break;
  }

  //-------------------------------------------------------------------------------------------------------------------------
  // 2) Main measurement loop
  //-------------------------------------------------------------------------------------------------------------------------
  start = millis();

  while ((crossCount < crossings) && ((millis()-start)<timeout))
  {
    numberOfSamples++;                       //Count number of times looped.
    lastFilteredV = filteredV;               //Used for delay/phase compensation

    //-----------------------------------------------------------------------------
    // A) Read in raw voltage and current samples
    //-----------------------------------------------------------------------------

    // Keep voltage and current reads close together. Averaging multiple current
    // reads here delays current relative to voltage and corrupts power factor.
    #ifndef _ADS1X15_CURRENT_
      sampleV = analogRead(inPinV);
      sampleI = analogRead(inPinI);
      offsetI = offsetI + ((sampleI-offsetI)/1024);
    #endif
    

    #ifdef _ADS1X15_CURRENT_
      //t = millis();
      sampleV = analogread_volts(0);  //voltage
      //Serial.printf("\n>>>>>Voltage sample time took @ %u ms",millis() - t);
      sampleI = analogread_amps(1);   //current

      offsetI = offsetI + ((sampleI-offsetI)/1024);
    #endif

    //-----------------------------------------------------------------------------
    // B) Apply digital low pass filters to extract the 2.5 V or 1.65 V dc offset,
    //     then subtract this - signal is now centred on 0 counts.
    //-----------------------------------------------------------------------------

    offsetV = offsetV + ((sampleV-offsetV)/1024);
    filteredV = sampleV - offsetV; 


    //offsetI = offsetI + ((sampleI-offsetI)/ADC_COUNTS);
    filteredI = sampleI - offsetI; 


    //-----------------------------------------------------------------------------
    // C) Root-mean-square method voltage
    //-----------------------------------------------------------------------------
    sqV= filteredV * filteredV;                 //1) square voltage values
    sumV += sqV;                                //2) sum

    //-----------------------------------------------------------------------------
    // D) Root-mean-square method current
    //-----------------------------------------------------------------------------
    sqI = filteredI * filteredI;                //1) square current values
    sumI += sqI;                                //2) sum

    //-----------------------------------------------------------------------------
    // E) Phase calibration
    //-----------------------------------------------------------------------------
    phaseShiftedV = lastFilteredV + PHASECAL * (filteredV - lastFilteredV);

    //-----------------------------------------------------------------------------
    // F) Instantaneous power calc
    //-----------------------------------------------------------------------------
    instP = phaseShiftedV * filteredI;          //Instantaneous Power
    sumP +=instP;                               //Sum

    //-----------------------------------------------------------------------------
    // G) Find the number of times the voltage has crossed the initial voltage
    //    - every 2 crosses we will have sampled 1 wavelength
    //    - so this method allows us to sample an integer number of half wavelengths which increases accuracy
    //-----------------------------------------------------------------------------
    lastVCross = checkVCross;
    if (sampleV > startV) checkVCross = true;
                     else checkVCross = false;
    if (numberOfSamples==1) lastVCross = checkVCross;

    if (lastVCross != checkVCross) crossCount++;
  }

  //-------------------------------------------------------------------------------------------------------------------------
  // 3) Post loop calculations
  //-------------------------------------------------------------------------------------------------------------------------
  //Calculation of the root of the mean of the voltage and current squared (rms)
  //Calibration coefficients applied.

  /*
  #ifdef _ADS1X15_VOLTAGE_
    V_RATIO = VCAL *((SupplyVoltageADS1115/1000.0) / (ADC_COUNTS_ADS1X15)); //switch to ADS
  #else
    V_RATIO = VCAL *((SupplyVoltage/1000.0) / (ADC_COUNTS)); 
  #endif  
  */
  Vrms = V_RATIO * sqrt(sumV / numberOfSamples);


  /*
  #ifdef _ADS1X15_CURRENT_
     I_RATIO = ICAL *((SupplyVoltageADS1115/1000.0) / (ADC_COUNTS_ADS1X15));
  #else
     I_RATIO = ICAL *((SupplyVoltage/1000.0) / (ADC_COUNTS));
  #endif
  */
  Irms = I_RATIO * sqrt(sumI / numberOfSamples);


  // Calculation power values
  //  Serial.printf ("\n[emon   ] Irms %f, V_RATIO %f, I-RATIO %f, SUMP %f, numberOfSamples %u, crossCount %u, ADC_COUNTS %u, offsetV %f, offsetI %f", Irms, V_RATIO , I_RATIO , sumP , numberOfSamples, crossCount, ADC_COUNTS, offsetV, offsetI);
  // debugV ("\nIrms %f, V_RATIO %f, I-RATIO %f, I_V_RATIO %f, SUMP %f, numberOfSamples %u, crossCount %u, ADC_COUNTS %u, offsetV %f, offsetI %f", Irms, V_RATIO , I_RATIO , I_V_RATIO, sumP , numberOfSamples, crossCount, ADC_COUNTS, offsetV, offsetI);   
  
  realPower = I_V_RATIO * ((sumP / numberOfSamples));
  apparentPower = Vrms * Irms;
  powerFactor = (apparentPower != 0.0) ? (realPower / apparentPower) : 0.0;

  //Reset accumulators
  sumV = 0;
  sumI = 0;
  sumP = 0;
  //  Serial.printf("\n>>>>Total function time took @ %u ms",millis() - totalTime);
}




/*
void EnergyMonitor::calcVI(unsigned int crossings, unsigned int timeout, uint8_t readtype )
{
  #if defined emonTxV3
  int SupplyVoltage=3300;
  #else
  int SupplyVoltage = readVcc();
  #endif

  unsigned int crossCount = 0;                             //Used to measure number of times threshold is crossed.
  unsigned int numberOfSamples = 0;                        //This is now incremented

  //-------------------------------------------------------------------------------------------------------------------------
  // 1) Waits for the waveform to be close to 'zero' (mid-scale adc) part in sin curve.
  //-------------------------------------------------------------------------------------------------------------------------
  unsigned long start = millis();    //millis()-start makes sure it doesnt get stuck in the loop if there is an error.

  while(1)                                   //the while loop...
  {
    startV = analogRead(inPinV);                    //using the voltage waveform
    if ((startV < (ADC_COUNTS*0.55)) && (startV > (ADC_COUNTS*0.45))) break;  //check its within range
    if ((millis()-start)>timeout) break;
  }

  //-------------------------------------------------------------------------------------------------------------------------
  // 2) Main measurement loop
  //-------------------------------------------------------------------------------------------------------------------------
  start = millis();

  while ((crossCount < crossings) && ((millis()-start)<timeout))
  {
    numberOfSamples++;                       //Count number of times looped.
    lastFilteredV = filteredV;               //Used for delay/phase compensation

    //-----------------------------------------------------------------------------
    // A) Read in raw voltage and current samples
    //-----------------------------------------------------------------------------
    sampleV = analogRead(inPinV);                 //Read in raw voltage signal
    if (readtype > 1) {
    sampleI = analogRead(inPinI);                 //Read in raw current signal
    }

    //-----------------------------------------------------------------------------
    // B) Apply digital low pass filters to extract the 2.5 V or 1.65 V dc offset,
    //     then subtract this - signal is now centred on 0 counts.
    //-----------------------------------------------------------------------------
    offsetV = offsetV + ((sampleV-offsetV)/ADC_COUNTS);
    filteredV = sampleV - offsetV;
    if (readtype > 1) {
    offsetI = offsetI + ((sampleI-offsetI)/ADC_COUNTS);
    filteredI = sampleI - offsetI;
    }

    //-----------------------------------------------------------------------------
    // C) Root-mean-square method voltage
    //-----------------------------------------------------------------------------
    sqV= filteredV * filteredV;                 //1) square voltage values
    sumV += sqV;                                //2) sum

    //-----------------------------------------------------------------------------
    // D) Root-mean-square method current
    //-----------------------------------------------------------------------------
    if (readtype > 1) {
    sqI = filteredI * filteredI;                //1) square current values
    sumI += sqI;                                //2) sum
    }

    //-----------------------------------------------------------------------------
    // E) Phase calibration
    //-----------------------------------------------------------------------------
    phaseShiftedV = lastFilteredV + PHASECAL * (filteredV - lastFilteredV);

    //-----------------------------------------------------------------------------
    // F) Instantaneous power calc
    //-----------------------------------------------------------------------------
    if (readtype > 1) {
    instP = phaseShiftedV * filteredI;          //Instantaneous Power
    sumP +=instP;                               //Sum
    }

    //-----------------------------------------------------------------------------
    // G) Find the number of times the voltage has crossed the initial voltage
    //    - every 2 crosses we will have sampled 1 wavelength
    //    - so this method allows us to sample an integer number of half wavelengths which increases accuracy
    //-----------------------------------------------------------------------------
    lastVCross = checkVCross;
    if (sampleV > startV) checkVCross = true;
                     else checkVCross = false;
    if (numberOfSamples==1) lastVCross = checkVCross;

    if (lastVCross != checkVCross) crossCount++;
  }

  //-------------------------------------------------------------------------------------------------------------------------
  // 3) Post loop calculations
  //-------------------------------------------------------------------------------------------------------------------------
  //Calculation of the root of the mean of the voltage and current squared (rms)
  //Calibration coefficients applied.

  double V_RATIO = VCAL *((SupplyVoltage/1000.0) / (ADC_COUNTS));
  Vrms = V_RATIO * sqrt(sumV / numberOfSamples);

if (readtype > 1) {
  double I_RATIO = ICAL *((SupplyVoltage/1000.0) / (ADC_COUNTS));
  Irms = I_RATIO * sqrt(sumI / numberOfSamples);

  //Calculation power values
  realPower = V_RATIO * I_RATIO * sumP / numberOfSamples;
  apparentPower = Vrms * Irms;
  powerFactor=realPower / apparentPower;
}

  //Reset accumulators
  sumV = 0;
  sumI = 0;
  sumP = 0;
//--------------------------------------------------------------------------------------
}
*/



double EnergyMonitor::ReadVoltage(unsigned int crossings, unsigned int timeout, uint8_t readtype )
{
    #if defined emonTxV3
   SupplyVoltage=3300;
  #else
   SupplyVoltage = readVcc();
  #endif

  double V_RATIO , I_RATIO;

  setupparams();

  unsigned int crossCount = 0;                             //Used to measure number of times threshold is crossed.
  unsigned int numberOfSamples = 0;                        //This is now incremented

  //-------------------------------------------------------------------------------------------------------------------------
  // 1) Waits for the waveform to be close to 'zero' (mid-scale adc) part in sin curve.
  //-------------------------------------------------------------------------------------------------------------------------
  unsigned long start = millis();    //millis()-start makes sure it doesnt get stuck in the loop if there is an error.

  while(1)                                   //the while loop...
  {
    startV = analogread_volts(0);      
    if ((startV < (ADCCOUNTS*0.55)) && (startV > (ADCCOUNTS*0.45))) { 
      break;  //check its within range
    }
    if ((millis()-start)>timeout) break;
  }

  //-------------------------------------------------------------------------------------------------------------------------
  // 2) Main measurement loop
  //-------------------------------------------------------------------------------------------------------------------------
  start = millis();

  while ((crossCount < crossings) && ((millis()-start)<timeout))
  {
    numberOfSamples++;                       //Count number of times looped.
    lastFilteredV = filteredV;               //Used for delay/phase compensation

    //-----------------------------------------------------------------------------
    // A) Read in raw voltage and current samples
    //-----------------------------------------------------------------------------

     int numsamples = 10;
     double sumxi = 0;
     double sumxv = 0;

     #ifndef _ADS1X15_VOLTAGE_ 
      for (unsigned int nn = 0; nn < numsamples; nn++)
        {  
              sampleV = analogRead(inPinV);        //Read in raw voltage signal
              sumxv = sumxv + sampleV*sampleV;     
              offsetV = (offsetV + (sampleV-offsetV)/(ADC_COUNTS));
        }  
     
    sampleV =sqrt(sumxv/numsamples);  
    #endif

    #ifdef _ADS1X15_VOLTAGE_
        sampleV = analogread_volts(0);  //voltage
    #endif    

    //-----------------------------------------------------------------------------
    // B) Apply digital low pass filters to extract the 2.5 V or 1.65 V dc offset,
    //     then subtract this - signal is now centred on 0 counts.
    //-----------------------------------------------------------------------------
    offsetV = floor(offsetV + ((sampleV-offsetV)/1024));
    filteredV = sampleV - offsetV; 


    //-----------------------------------------------------------------------------
    // C) Root-mean-square method voltage
    //-----------------------------------------------------------------------------
    sqV= filteredV * filteredV;                 //1) square voltage values
    sumV += sqV;                                //2) sum

    //-----------------------------------------------------------------------------
    // G) Find the number of times the voltage has crossed the initial voltage
    //    - every 2 crosses we will have sampled 1 wavelength
    //    - so this method allows us to sample an integer number of half wavelengths which increases accuracy
    //-----------------------------------------------------------------------------
    lastVCross = checkVCross;
    if (sampleV > startV) checkVCross = true;
                     else checkVCross = false;
    if (numberOfSamples==1) lastVCross = checkVCross;

    if (lastVCross != checkVCross) crossCount++;
  }

  //-------------------------------------------------------------------------------------------------------------------------
  // 3) Post loop calculations
  //-------------------------------------------------------------------------------------------------------------------------
  //Calculation of the root of the mean of the voltage 
  //Calibration coefficients applied.

  #ifdef _ADS1X15_VOLTAGE_
    V_RATIO = VCAL *((SupplyVoltageADS1115/1000.0) / (ADC_COUNTS_ADS1X15)); //switch to ADS
  #else
    V_RATIO = VCAL *((SupplyVoltage/1000.0) / (ADC_COUNTS)); 
  #endif  
  Vrms = V_RATIO * sqrt(sumV / numberOfSamples);

  //Reset accumulators
  sumV = 0;
  return Vrms;
}




double EnergyMonitor::calcIrms(unsigned int Number_of_Samples)
{
  double sumxi = 0;
  int16_t numsamples = 50; // 75 is optimal

  /*
  #if defined emonTxV3
    int SupplyVoltage=3300;
  #else
    int SupplyVoltage = readVcc();
  #endif
  
  setupparams();
  */

  for (unsigned int n = 0; n < Number_of_Samples; n++)
  {
      #ifndef _ADS1X15_CURRENT_
      sumxi = 0;
      for (unsigned int nn = 0; nn < numsamples; nn++)
        {  
          sampleI = analogRead(inPinI);
          sumxi = sumxi + sampleI*sampleI;
          offsetI = (offsetI + (sampleI-offsetI)/(ADC_COUNTS));
        }  
          sampleI =sqrt(sumxi/numsamples);
    #endif

    #ifdef _ADS1X15_CURRENT_
    sampleI = analogread_amps (1);   //current
    #endif 

    // Digital low pass filter extracts the 2.5 V or 1.65 V dc offset,
    // then subtract this - signal is now centered on 0 counts.
    // - offsetI = floor(offsetI + ((sampleI-offsetI)/1024));
    filteredI = sampleI - offsetI; 

    // Root-mean-square method current
    // 1) square current values
    sqI = filteredI * filteredI;
    // 2) sum
    sumI += sqI;
  }

  #ifndef _ADS1X15_CURRENT_
    double I_RATIO = ICAL *((SupplyVoltage/1000.0) / (ADC_COUNTS));
  #else
    double I_RATIO = ICAL *((SupplyVoltageADS1115/1000.0) / (ADC_COUNTS_ADS1X15));
  #endif

  Irms = I_RATIO * sqrt(sumI / Number_of_Samples);

  //Reset accumulators
  sumI = 0;
  //--------------------------------------------------------------------------------------

  return Irms;
}


void EnergyMonitor::serialprint()
{
  /*
  Serial.print(realPower);
  Serial.print(' ');
  Serial.print(apparentPower);
  Serial.print(' ');
  Serial.print(Vrms);
  Serial.print(' ');
  Serial.print(Irms);
  Serial.print(' ');
  Serial.print(powerFactor);
  Serial.println(' ');
  */

  Serial.printf ("\n realPower= %f, apparentPower= %f, Vrms= %f, Irms= %f, powerFactor= %f",realPower,apparentPower,Vrms,Irms,powerFactor);
  // delay(100);
}

//thanks to http://hacking.majenko.co.uk/making-accurate-adc-readings-on-arduino
//and Jérôme who alerted us to http://provideyourown.com/2012/secret-arduino-voltmeter-measure-battery-voltage/

long EnergyMonitor::readVcc() {
  long result;

  //not used on emonTx V3 - as Vcc is always 3.3V - eliminates bandgap error and need for calibration http://harizanov.com/2013/09/thoughts-on-avr-adc-accuracy/

  #if defined(__AVR_ATmega168__) || defined(__AVR_ATmega328__) || defined (__AVR_ATmega328P__)
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined(__AVR_ATmega644__) || defined(__AVR_ATmega644P__) || defined(__AVR_ATmega1284__) || defined(__AVR_ATmega1284P__)
  ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__) || defined(__AVR_AT90USB1286__)
  ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  ADCSRB &= ~_BV(MUX5);   // Without this the function always returns -1 on the ATmega2560 http://openenergymonitor.org/emon/node/2253#comment-11432
  #elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
  ADMUX = _BV(MUX5) | _BV(MUX0);
  #elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
  ADMUX = _BV(MUX3) | _BV(MUX2);

  #endif


  #if defined(__AVR__)
  delay(2);                                        // Wait for Vref to settle
  ADCSRA |= _BV(ADSC);                             // Convert
  while (bit_is_set(ADCSRA,ADSC));
  result = ADCL;
  result |= ADCH<<8;
  result = READVCC_CALIBRATION_CONST / result;  //1100mV*1024 ADC steps http://openenergymonitor.org/emon/node/1186
  return result;
  #elif defined(__arm__)
  return (3300);                                  //Arduino Due
  #else
  return (3300);                                  //Guess that other un-supported architectures will be running a 3.3V!
  #endif
}

