#include "Arduino.h"
#include "EmonLib.h"    
#include "CT_ProcessPower.h"
#include "defines.h"

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <AsyncMqttClient.h>

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

extern bool saveCTReadings(float KWh,  float MTD_KWh, float YTD_KWh);


CTPROCESSOR::CTPROCESSOR(uint8_t _CTPin, double _ICAL, uint8_t _inPinV, double _VCAL, double _PHASECAL)
{
  CTPin = _CTPin;
  ICAL = _ICAL;
  inPinV = _inPinV;
  VCAL = _VCAL;
  PHASECAL = _PHASECAL;

/*
  CTPin = 35;
  ICAL = 30;
  inPinV = 34;
  VCAL = 382/2;
  PHASECAL = 1.7;
  */

  Serial.printf (">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>   CTPIN %f , ICAL %f, inPINV %f, VCAL %f, PHASECAL %f", CTPin,ICAL,inPinV, VCAL, PHASECAL);

  analogReadResolution(12);
  adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11); 
  adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11);   

  emon1.voltage(inPinV, VCAL , PHASECAL);    // 382 Voltage: input pin, calibration, phase_shift ** voltage constant = alternating mains voltage ÷ alternating voltage at ADC input pin (alternating voltage at ADC input pin = voltage at the middle point of the resistor voltage divider)
  emon1.current(CTPin , ICAL);                // Current: input pin, calibration.  *** the current constant is the value of current you want to read when 1 V is produced at the analogue input  
  
  // emon1.voltage(34, VCAL , 1.7);    // 382 Voltage: input pin, calibration, phase_shift ** voltage constant = alternating mains voltage ÷ alternating voltage at ADC input pin (alternating voltage at ADC input pin = voltage at the middle point of the resistor voltage divider)
  // emon1.current(35, 30);                // Current: input pin, calibration.  *** the current constant is the value of current you want to read when 1 V is produced at the analogue input  
    
}


CTPROCESSOR::~CTPROCESSOR(){
   //   delete emon1 ;
    }


  int CTPROCESSOR::readPower(float adjustment){
      amps = emon1.calcIrms(150) + adjustment; // Calculate Irms only (150 is optimal)
      if (amps < 0) amps = 0;
      Serial.print (F("[CT     ] Amps "));
      Serial.print (amps);
      Serial.print (F(" | offsetI  "));
      Serial.println (emon1.offsetI);
      dtostrf(amps, 6, 2, resx); // Leave room for too large numbers!   

      emon1.calcVI(20,500);                    // Calculate all. No.of wavelengths, time-out// emon1.calcVI(20,5000); 
      Serial.print(F("[CT     ]"));
      emon1.serialprint();            
      
      realPower       = emon1.realPower;        //extract Real Power into variable
      apparentPower   = emon1.apparentPower;    //extract Apparent Power into variable
      apparentPower_2 = amps * emon1.Vrms;
      supplyVoltage   = emon1.Vrms;             //extract Vrms into Variable
      Irms            = amps ; // emon1.Irms;   //extract Irms into Variable      
      powerFActor     = emon1.powerFactor;      //extract Power Factor into Variable


      Instantaneous_Wh = (amps*(millis()-lwhtime)*abs(emon1.powerFactor)*supplyVoltage)/3600000.0; 
      lwhtime = millis();  
      CTSaveThreshold += Instantaneous_Wh;
      PreviousWh = wh;
      wh += Instantaneous_Wh;
      MTD_Wh += Instantaneous_Wh;
      YTD_Wh += Instantaneous_Wh;      

      Stabilized ++;
      Serial.printf ("[CT     ] CT Values wh= %f | PreviousWh wh= %f | DIFF= %f \n", wh, PreviousWh, wh-PreviousWh );       
      if (Stabilized > 5) {
        if (CTSaveThreshold > CTSaveThreshold_value) {
          CTSaveThreshold = 0;
          saveCTReadings(wh, MTD_Wh, YTD_Wh);
          Serial.printf ("[CT **  ] Saved CT Values %f Wh | %f MTD_Wh | %f YTD_Wh \n",wh, MTD_Wh, YTD_Wh ); 
        }
      }      
  }

  int CTPROCESSOR::DisplayPower(Adafruit_SSD1306 &display, AsyncMqttClient &mqttClient){
     #ifdef OLED_1306
        display.clearDisplay();
        display.setTextSize(1);
        #define StartRow 22 
        #define rect_height 30
        #define rect_width 60

        display.drawRect(0,StartRow,rect_width,rect_height, WHITE);
        display.drawRect(rect_width + 2 ,StartRow,rect_width,rect_height, WHITE);   

        display.setCursor(1,1);
        // display.println("Power Readings");
        display.print(String(apparentPower_2) + " w");
        display.setCursor(60,1);        
        display.print(String(wh/1000) + " KWh");        

        display.setCursor(1,12);        
        display.print(String(MTD_Wh/1000) + " MTD");      

                
        if ((Chronos::DateTime::now().day() == 1) && (Chronos::DateTime::now().hour() == 1) && 
            (Chronos::DateTime::now().minute() == 1) && (Chronos::DateTime::now().second() == 1)) MTD_Wh = 0; 

        display.setCursor(60,12);        
        display.print(String(YTD_Wh/1000) + " YTD");       
        if ((Chronos::DateTime::now().day() == 1) && (Chronos::DateTime::now().hour() == 1) && 
            (Chronos::DateTime::now().minute() == 1) && (Chronos::DateTime::now().second() == 1) && 
            (Chronos::DateTime::now().month() == 1) ) YTD_Wh = 0;            
      
        
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


        display.setCursor(1,54);  // col,row      
        display.setTextColor(BLACK,WHITE);
        display.print(WiFi.localIP().toString());
        display.setTextColor(WHITE);
        display.setCursor(120,54);  // col,row    
        if ((WiFi.status() == WL_CONNECTED)) display.print("*");   
        if ((WiFi.status() != WL_CONNECTED)) display.print("x");  
        display.setCursor(110,54);  // col,row   
        if (mqttClient.connected()) display.print("M");   
        if (!mqttClient.connected()) display.print("m");    
          
            
      #endif    
  }










    #ifdef _emonlibx_  
      amps = emon1.calcIrms(150); // Calculate Irms only (150 is optimal)
      Serial.print (F("[CT     ] Amps "));
      Serial.print (amps);
      Serial.print (F(" | offsetI  "));
      Serial.println (emon1.offsetI);
      char resx[8];
      dtostrf(amps, 6, 2, resx); // Leave room for too large numbers!   
      if (mqttClient.connected()) {
        mqttClient.publish((MyConfParam.v_CurrentTransformerTopic).c_str(), QOS2, RETAINED, resx);
        }  
        #ifdef AppleHK
          hap_val_t new_val;
          new_val.f = amps;
          hap_char_update_val(hcx_temp, &new_val);
        #endif        
     
      /*
      Serial.print ("Max Allowed Current: ");
      Serial.print (MyConfParam.v_CT_MaxAllowed_current);
      Serial.print (" | off time: ");
      Serial.print (time_off);
      Serial.print (" | on time: ");
      Serial.println (time_on);            
      */
     
      if (amps > MyConfParam.v_CT_MaxAllowed_current) {
        if (RelayCToff.enabled == false) {
            Serial.println(F("[CT     ] timer off runing - High consumption detected"));
            RelayCToff.start(); 
            // RelayCTon.stop();
        }
      }
      
      emon1.calcVI(20,500);                    // Calculate all. No.of wavelengths, time-out// emon1.calcVI(20,5000); 
      Serial.print(F("[CT     ]"));
      emon1.serialprint();            
      realPower       = emon1.realPower;        //extract Real Power into variable
      apparentPower   = emon1.apparentPower;    //extract Apparent Power into variable
      apparentPower_2 = amps * emon1.Vrms;
      supplyVoltage   = emon1.Vrms;             //extract Vrms into Variable
      Irms            = amps ; // emon1.Irms;             //extract Irms into Variable      
      powerFActor     = emon1.powerFactor;      //extract Power Factor into Variable

      // wh += (amps*(millis()-lwhtime)*POWER_FACTOR*supplyVoltage)/3600000.0; //3600000 convert to float

      Instantaneous_Wh = (amps*(millis()-lwhtime)*abs(emon1.powerFactor)*supplyVoltage)/3600000.0; 
      lwhtime = millis();  
      CTSaveThreshold += Instantaneous_Wh;
      PreviousWh = wh;
      wh += Instantaneous_Wh;
      MTD_Wh += Instantaneous_Wh;
      YTD_Wh += Instantaneous_Wh;

      Stabilized ++;
      Serial.printf ("[CT     ] CT Values wh= %f | PreviousWh wh= %f | DIFF= %f \n", wh, PreviousWh, wh-PreviousWh );       
      if (Stabilized > 5) {
        if (CTSaveThreshold > CTSaveThreshold_value) {
          CTSaveThreshold = 0;
          saveCTReadings(wh, MTD_Wh, YTD_Wh);
          Serial.printf ("[CT     ] Saved CT Values %f Wh | %f MTD_Wh | %f YTD_Wh \n",wh, MTD_Wh, YTD_Wh ); 
        }
      }
 
      #ifdef OLED_1306
        display.clearDisplay();
        display.setTextSize(1);
        #define StartRow 22 
        #define rect_height 30
        #define rect_width 60

        display.drawRect(0,StartRow,rect_width,rect_height, WHITE);
        display.drawRect(rect_width + 2 ,StartRow,rect_width,rect_height, WHITE);   

        display.setCursor(1,1);
        // display.println("Power Readings");
        display.print(String(apparentPower_2) + " w");
        display.setCursor(60,1);        
        display.print(String(wh/1000) + " KWh");        

        display.setCursor(1,12);        
        display.print(String(MTD_Wh/1000) + " MTD");      
        if ((Chronos::DateTime::now().day() == 1) && (Chronos::DateTime::now().hour() == 1) && 
            (Chronos::DateTime::now().minute() == 1) && (Chronos::DateTime::now().second() == 1)) MTD_Wh = 0; 

        display.setCursor(60,12);        
        display.print(String(YTD_Wh/1000) + " YTD");       
        if ((Chronos::DateTime::now().day() == 1) && (Chronos::DateTime::now().hour() == 1) && 
            (Chronos::DateTime::now().minute() == 1) && (Chronos::DateTime::now().second() == 1) && 
            (Chronos::DateTime::now().month() == 1) ) YTD_Wh = 0;                          

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

        display.setCursor(1,54);  // col,row      
        display.setTextColor(BLACK,WHITE);
        display.print(WiFi.localIP().toString());
        display.setTextColor(WHITE);
        display.setCursor(120,54);  // col,row    
        if ((WiFi.status() == WL_CONNECTED)) display.print("*");   
        if ((WiFi.status() != WL_CONNECTED)) display.print("x");  
        display.setCursor(110,54);  // col,row   
        if (mqttClient.connected()) display.print("M");   
        if (!mqttClient.connected()) display.print("m");          
      #endif
    #endif