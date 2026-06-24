#ifdef ESP32
#include "Arduino.h"
#include "EmonLib.h"    
#include "CT_ProcessPower.h"
#include "defines.h"

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <AsyncMqttClient.h>
#include <Adafruit_ADS1X15.h>

#include <Chronos.h>

#ifdef ESP32
 #include <WiFi.h>
 #include <esp_wifi.h>
#else
  #include <ESP8266WiFi.h>
#endif

#ifdef ESP32
  #include "esp_adc_cal.h"
#endif

#ifdef DEBUG_ENABLED
  #include "RemoteDebug.h"
  extern   RemoteDebug Debug;
#endif

extern DisplayActions CURRENT_Display_Action;
extern void saveCTReadings(float KWh,  float MTD_KWh, float YTD_KWh);
extern   Adafruit_ADS1115 ads;


CTPROCESSOR::CTPROCESSOR(uint8_t _CTPin, double _ICAL, uint8_t _inPinV, double _VCAL, double _PHASECAL,fnptr_a pThresholdHigh_callback, fnptr_a pThresholdLow_callback )
{
  CTPin = _CTPin;
  ICAL = _ICAL;
  inPinV = _inPinV;
  VCAL = _VCAL;
  PHASECAL = _PHASECAL;
  fThresholdHigh_callback = pThresholdHigh_callback;
  fThresholdLow_callback = pThresholdLow_callback;

/*
  CTPin = 35;
  ICAL = 30;
  inPinV = 34;
  VCAL = 382/2;
  PHASECAL = 1.7;
  */

  Serial.printf ("[CT     ]  CTPIN %f , ICAL %f, inPINV %f, VCAL %f, PHASECAL %f", CTPin,ICAL,inPinV, VCAL, PHASECAL);

  analogReadResolution(12);
  adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11); 
  adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11);   
  analogReadResolution(12);

  emon1.voltage(inPinV, VCAL , PHASECAL);     // 382 Voltage: input pin, calibration, phase_shift ** voltage constant = alternating mains voltage ÷ alternating voltage at ADC input pin (alternating voltage at ADC input pin = voltage at the middle point of the resistor voltage divider)
  emon1.current(CTPin , ICAL);                // Current: input pin, calibration.  *** the current constant is the value of current you want to read when 1 V is produced at the analogue input  

  ThresholdCossHighTimer = xTimerCreate("ThresholdCossHighTimer", pdMS_TO_TICKS(1000), pdTRUE, (void*)  0, reinterpret_cast<TimerCallbackFunction_t>(fThresholdHigh_callback));
  ThresholdCossLowTimer  = xTimerCreate("ThresholdCossLowTimer",  pdMS_TO_TICKS(1000), pdTRUE, (void*)  0, reinterpret_cast<TimerCallbackFunction_t>(fThresholdLow_callback));
  xTimerStop(ThresholdCossHighTimer,0);
  xTimerStop(ThresholdCossLowTimer,0);

}


CTPROCESSOR::~CTPROCESSOR(){
   //   delete emon1 ;
    }


void CTPROCESSOR::ThresholdCossHighTimerStart() {
  #ifdef ESP32
      xTimerStart(this->ThresholdCossHighTimer,0);
  #else
    	// return false;
  #endif    
}

void CTPROCESSOR::ThresholdCossHighTimerStop() {
  #ifdef ESP32
      xTimerStop(this->ThresholdCossHighTimer,0);
      ThresholdCossHighTimerCounter = 0; 
      // return true;
   #else

  #endif    

}

bool CTPROCESSOR::ThresholdCossHighTimerActive() {
  #ifdef ESP32
     if( xTimerIsTimerActive( this->ThresholdCossHighTimer ) != pdFALSE )    {
         return true;
     } else {
        return false;
     }
  #endif   
}


void CTPROCESSOR::ThresholdCossLowTimerStart() {
  #ifdef ESP32
      xTimerStart(this->ThresholdCossLowTimer,0);
      // return true;
  #else
    	return false;
  #endif    
}

void CTPROCESSOR::ThresholdCossLowTimerStop() {
  #ifdef ESP32
      xTimerStop(this->ThresholdCossLowTimer,0);
      ThresholdCossLowTimerCounter = 0; 
      // return true;
   #else
    	return false;
  #endif    
}

bool CTPROCESSOR::ThresholdCossLowTimerActive() {
  #ifdef ESP32
     if( xTimerIsTimerActive( this->ThresholdCossLowTimer ) != pdFALSE )    {
         return true;
     } else {
        return false;
     }
  #endif   
}


  void CTPROCESSOR::readPower(float adjustment, uint8_t savethreshold) {
      // amps = emon1.calcIrms(20) + adjustment; // Calculate Irms only (150 is optimal)
      // amps = emon1.calcIrms(5000) + adjustment; // Calculate Irms only (150 is optimal)      
      // if (amps < 0) amps = 0;

      #ifdef _ADS1X15_CURRENT_
      emon1.calcVI(50,2000);//,2); //emon1.calcVI(100,2000);//,2);    // Calculate all. No.of wavelengths, time-out// emon1.calcVI(20,5000); 
      
      #else
      // amps = emon1.calcIrms(20) + adjustment; // Calculate Irms only (150 is optimal)
      emon1.calcVI(50,2000);
      #endif

      amps = emon1.Irms;
      
      realPower       = emon1.realPower;        // if you get a negative powerfactor then invert the direction of the CT around the wire!
      apparentPower   = emon1.apparentPower;    //extract Apparent Power into variable
      apparentPower_2 = amps * emon1.Vrms;
      supplyVoltage   = emon1.Vrms;             //extract Vrms into Variable
      Irms            = amps ; // emon1.Irms;   //extract Irms into Variable      
      powerFactor     = emon1.powerFactor;      //extract Power Factor into Variable
      float powerFactor2 = realPower / apparentPower_2;

      Instantaneous_Wh    = (amps*(millis()-lwhtime)*abs(powerFactor2)*supplyVoltage)/3600000.0; // / 3600 seconds /1000 because it is Kilo watts
      lwhtime = millis();  
      CTSaveThreshold += Instantaneous_Wh; // from Kwh to wh
      
      PreviousWh = wh;
      wh += Instantaneous_Wh;
      MTD_Wh += Instantaneous_Wh;
      YTD_Wh += Instantaneous_Wh;      

      #ifdef DEBUG_ENABLED
      //  debugV (" Vrms= %f, Irms= %f, apparentPower= %f, realPower= %f, powerFactor= %f",emon1.Vrms, emon1.Irms, apparentPower, emon1.realPower ,emon1.powerFactor);   
      //  debugV (" Vrms= %f, Adjusted Amps= %f, ApparentPower adjusted amps (V*Aa)= %f, powerFactor2= %f",emon1.Vrms,amps, apparentPower_2,powerFactor2);   
      //  Serial.printf ("\nVrms= %f, Irms= %f, offsetI= %f, apparentPower= %f, realPower= %f, powerFactor= %f",emon1.Vrms, emon1.Irms, emon1.offsetI, apparentPower, emon1.realPower ,emon1.powerFactor);   
      #endif        

      jsonPost = jsonPost_template; 
      jsonPost.replace("[SOURCE]","Cnt:" + MyConfParam.v_PhyLoc);
      jsonPost.replace("[POWER]",String(apparentPower_2));     
      jsonPost.replace("[R_POWER]",String(emon1.realPower));             
      jsonPost.replace("[VOLTAGE]",String(supplyVoltage));
      jsonPost.replace("[AMPS]",String(Irms));      
      jsonPost.replace("[WH]",String(wh/1000));
      jsonPost.replace("[MTD_WH]",String(MTD_Wh/1000));
      jsonPost.replace("[YTD_WH]",String(YTD_Wh/1000));   
      jsonPost.replace("[PF]",String(emon1.powerFactor));
      jsonPost.replace("[PFC]",String(powerFactor2));      
      jsonPost.replace("[IP]",WiFi.localIP().toString());  
      jsonPost.replace("[R1]",digitalRead(RelayPin) == HIGH ? ON : OFF);  
      #ifdef ESP32_2RBoard
      jsonPost.replace("[R2]",digitalRead(Relay1Pin) == HIGH ? ON : OFF);   
      #endif


      if (this->amps > MyConfParam.v_CT_MaxAllowed_current) {
        //Serial.printf (" Amps= %f Irms= %f ", amps, emon1.Irms );   
        if (fThresholdHigh_callback) {
            if (!this->ThresholdCossHighTimerActive()) {
              this->ThresholdCossLowTimerStop();
              this->ThresholdCossHighTimerStart();
            } else{
              ThresholdCossHighTimerCounter++;
            }
        };
      }
      
      if (this->amps < MyConfParam.v_CT_MaxAllowed_current - 0.5) {
        //Serial.printf (" Amps= %f Irms= %f ", amps, emon1.Irms ); 
        if (fThresholdLow_callback) {
          this->ThresholdCossHighTimerStop();
          if(!this->ThresholdCossLowTimerActive()) {

            this->ThresholdCossLowTimerStart();
          } else {
          this->ThresholdCossLowTimerCounter++;
          }
        };
      }

      // daily, reset at midnight
      if ((Chronos::DateTime::now().hour() == 1) && 
          (Chronos::DateTime::now().minute() == 1) && (Chronos::DateTime::now().second() == 1)) wh = 0;         

      // Monthly, reset at midnight, each month
      if ((Chronos::DateTime::now().day() == 1) && (Chronos::DateTime::now().hour() == 1) && 
          (Chronos::DateTime::now().minute() == 1) && (Chronos::DateTime::now().second() == 1)) MTD_Wh = 0; 

      // Yearly, reset at midnight every 1 January
      if ((Chronos::DateTime::now().day() == 1) && (Chronos::DateTime::now().hour() == 1) && 
          (Chronos::DateTime::now().minute() == 1) && (Chronos::DateTime::now().second() == 1) && 
          (Chronos::DateTime::now().month() == 1) ) YTD_Wh = 0;                

      Stabilized ++;
      //  Serial.printf ("[CT     ] CT Values wh= %f | PreviousWh wh= %f | DIFF= %f \n", wh, PreviousWh, wh-PreviousWh );       
      if (Stabilized > 5) {
        if (CTSaveThreshold > savethreshold) { 
          CTSaveThreshold = 0;
          saveCTReadings(wh, MTD_Wh, YTD_Wh);
          Serial.printf ("\n[CT **  ] Saved CT Values %f Wh | %f MTD_Wh | %f YTD_Wh \n",wh, MTD_Wh, YTD_Wh ); 
        }
      }      
  }





  void CTPROCESSOR::readVoltage() {
     supplyVoltage = emon1.ReadVoltage(20,500,0); 
  }


  void CTPROCESSOR::DisplayPower(Adafruit_SSD1306 &display, AsyncMqttClient &mqttClient, uint8_t sco){
     #ifdef OLED_1306
        display.setRotation(sco); 
        display.cp437(true); 
        display.clearDisplay();
        display.setTextSize(1);
        #define StartRow 22 
        #define rect_height 30
        #define rect_width 60

        display.drawRect(0,StartRow,rect_width,rect_height, WHITE);
        display.drawRect(rect_width + 2 ,StartRow,rect_width,rect_height, WHITE);   
       
        // display.println("Power Readings");
        display.setCursor(1,1);    
        display.print(String(apparentPower_2) + " w");
        display.setCursor(60,1);        
        display.print(String(wh/1000) + " KWh");        

        display.setCursor(1,12);        
        display.print(String(MTD_Wh/1000) + " MTD");                    

        display.setCursor(60,12);        
        display.print(String(YTD_Wh/1000) + " YTD");        
        
        if (CURRENT_Display_Action == ACTION_DISPALY_1) {
        display.setCursor(8, StartRow + 2);
        display.println(F("Current"));
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(10,StartRow + 16);       
        display.setTextSize(1.5); 
        display.print(String(Irms));
        display.setTextSize(1);        
        display.print(" A");

        display.setCursor(rect_width + 10 ,StartRow + 2 );    
        display.println(F("Voltage"));
        display.setCursor(rect_width + 8,StartRow + 16 );   
        display.setTextSize(1.5);                  
        display.print(String(supplyVoltage));
        display.setTextSize(1);              
        display.setTextColor(WHITE);
        display.println(" V");   
        }

        if (CURRENT_Display_Action == ACTION_DISPALY_2) {
        display.setCursor(8, StartRow + 2);
        display.println(F("P Factor"));
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(10,StartRow + 16);       
        display.setTextSize(1.5); 
        display.print(String(powerFactor));
        display.setTextSize(1);        
 

        display.setCursor(rect_width + 10 ,StartRow + 2 );    
        display.println(F("R Power"));
        display.setCursor(rect_width + 8,StartRow + 16 );   
        display.setTextSize(1.5);                  
        display.print(String(realPower));
        display.setTextSize(1);              
        display.setTextColor(WHITE);
  
        }
        display.display();

      #endif    
  }




#endif





