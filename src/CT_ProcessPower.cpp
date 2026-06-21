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
  #include "esp_task_wdt.h"
#endif

#ifdef DEBUG_ENABLED
  #include "RemoteDebug.h"
  extern   RemoteDebug Debug;
#endif

extern DisplayActions CURRENT_Display_Action;
extern void saveCTReadings(double KWh, double MTD_KWh, double YTD_KWh);
extern   Adafruit_ADS1115 ads;


// Wrappers with the correct TimerCallbackFunction_t signature (void(*)(TimerHandle_t)).
// The CTPROCESSOR instance is stored as the timer ID so it is recoverable here.
static void CT_ThresholdHighCallback(TimerHandle_t xTimer) {
    CTPROCESSOR* ct = static_cast<CTPROCESSOR*>(pvTimerGetTimerID(xTimer));
    if (ct && ct->fThresholdHigh_callback) {
        ct->fThresholdHigh_callback(ct);
    }
}

static void CT_ThresholdLowCallback(TimerHandle_t xTimer) {
    CTPROCESSOR* ct = static_cast<CTPROCESSOR*>(pvTimerGetTimerID(xTimer));
    if (ct && ct->fThresholdLow_callback) {
        ct->fThresholdLow_callback(ct);
    }
}

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

  emon1.voltage(inPinV, VCAL , PHASECAL);     // 382 Voltage: input pin, calibration, phase_shift ** voltage constant = alternating mains voltage ÷ alternating voltage at ADC input pin (alternating voltage at ADC input pin = voltage at the middle point of the resistor voltage divider)
  emon1.current(CTPin , ICAL);                // Current: input pin, calibration.  *** the current constant is the value of current you want to read when 1 V is produced at the analogue input  

  ThresholdCossHighTimer = xTimerCreate("ThresholdCossHighTimer", pdMS_TO_TICKS(1000), pdTRUE, (void*) this, CT_ThresholdHighCallback);
  ThresholdCossLowTimer  = xTimerCreate("ThresholdCossLowTimer",  pdMS_TO_TICKS(1000), pdTRUE, (void*) this, CT_ThresholdLowCallback);
  xTimerStop(ThresholdCossHighTimer,0);
  xTimerStop(ThresholdCossLowTimer,0);
  jsonPostMutex = xSemaphoreCreateMutex();

}


CTPROCESSOR::~CTPROCESSOR(){
   //   delete emon1 ;
    }


void CTPROCESSOR::ThresholdCossHighTimerStart() {
  xTimerStart(ThresholdCossHighTimer, 0);
}

void CTPROCESSOR::ThresholdCossHighTimerStop() {
  xTimerStop(ThresholdCossHighTimer, 0);
  ThresholdCossHighTimerCounter = 0;
}

bool CTPROCESSOR::ThresholdCossHighTimerActive() {
  return xTimerIsTimerActive(ThresholdCossHighTimer) != pdFALSE;
}

void CTPROCESSOR::ThresholdCossLowTimerStart() {
  xTimerStart(ThresholdCossLowTimer, 0);
}

void CTPROCESSOR::ThresholdCossLowTimerStop() {
  xTimerStop(ThresholdCossLowTimer, 0);
  ThresholdCossLowTimerCounter = 0;
}

bool CTPROCESSOR::ThresholdCossLowTimerActive() {
  return xTimerIsTimerActive(ThresholdCossLowTimer) != pdFALSE;
}


  void CTPROCESSOR::readPower(float adjustment, uint16_t savethreshold) {
      // amps = emon1.calcIrms(20) + adjustment; // Calculate Irms only (150 is optimal)
      // amps = emon1.calcIrms(5000) + adjustment; // Calculate Irms only (150 is optimal)      
      // if (amps < 0) amps = 0;

      #ifdef _ADS1X15_CURRENT_
      #ifdef ESP32
      esp_task_wdt_reset();
      #endif
      emon1.calcVI(20,1000);//,2); //emon1.calcVI(100,2000);//,2);    // Calculate all. No.of wavelengths, time-out// emon1.calcVI(20,5000);
      #ifdef ESP32
      esp_task_wdt_reset();
      #endif

      #else
      // amps = emon1.calcIrms(20) + adjustment; // Calculate Irms only (150 is optimal)
      #ifdef ESP32
      esp_task_wdt_reset();
      #endif
      emon1.calcVI(20,1000);//,2); //emon1.calcVI(100,2000);//,2);    // Calculate all. No.of wavelengths, time-out// emon1.calcVI(20,5000);
      #ifdef ESP32
      esp_task_wdt_reset();
      #endif
      #endif

      amps = emon1.Irms;

      realPower       = emon1.realPower;
      apparentPower   = emon1.apparentPower;
      apparentPower_2 = amps * emon1.Vrms;
      supplyVoltage   = emon1.Vrms;
      Irms            = amps;
      powerFactor     = emon1.powerFactor;
      float powerFactor2 = 0.0f;
      Stabilized++;
      const bool ctWarmingUp = (Stabilized <= WarmupReadings);

      // Only accumulate energy above the noise floor to prevent phantom Wh from
      // sensor noise when no load is connected.
      unsigned long nowMs = millis();
      unsigned long elapsedMs = nowMs - lwhtime;
      const bool zeroLoadCurrentNoise = amps <= MyConfParam.v_CT_ZeroLoadCurrentMax;
      const bool zeroLoadPowerNoise = realPower >= MyConfParam.v_CT_ZeroLoadPowerMin &&
                                      realPower <= MyConfParam.v_CT_ZeroLoadPowerMax;
      // Current below threshold (truly no load) OR power in ADC noise band.
      // The power condition catches high-current ADC noise (negative real power from
      // phase coupling) even when current exceeds the current threshold.
      const bool zeroLoadNoise = zeroLoadCurrentNoise || zeroLoadPowerNoise;
      if (zeroLoadNoise) {
        amps = 0.0;
        realPower = 0.0;
        apparentPower = 0.0;
        apparentPower_2 = 0.0;
        Irms = 0.0;
        powerFactor = 0.0;
        powerFactor2 = 0.0f;
      }
      if (lwhtime == 0 || ctWarmingUp) {
        Instantaneous_Wh = 0.0f;
      } else if (amps > MyConfParam.v_CT_ZeroLoadCurrentMax) {
        // Only count positive real power; negative values are phase-error noise.
        Instantaneous_Wh = (realPower > 0.0f ? realPower * elapsedMs / 3600000.0f : 0.0f);
      } else {
        Instantaneous_Wh = 0.0f;
      }
      lwhtime = nowMs;
      CTSaveThreshold += Instantaneous_Wh;

      PreviousWh = wh;
      wh += Instantaneous_Wh;
      MTD_Wh += Instantaneous_Wh;
      YTD_Wh += Instantaneous_Wh;      

      if (ctWarmingUp) {
        amps = 0.0;
        realPower = 0.0;
        apparentPower = 0.0;
        apparentPower_2 = 0.0;
        supplyVoltage = 0.0;
        Irms = 0.0;
        powerFactor = 0.0;
        powerFactor2 = 0.0f;
      } else {
        const float voltageSmoothingAlpha = 0.20f;
        if (!voltageSmoothingPrimed) {
          smoothedSupplyVoltage = supplyVoltage;
          voltageSmoothingPrimed = true;
        } else {
          smoothedSupplyVoltage += voltageSmoothingAlpha * (supplyVoltage - smoothedSupplyVoltage);
        }
        supplyVoltage = smoothedSupplyVoltage;
        apparentPower_2 = amps * supplyVoltage;
        powerFactor2 = (apparentPower_2 != 0.0) ? (realPower / apparentPower_2) : 0.0f;
      }

      #ifdef DEBUG_ENABLED
      //  debugV (" Vrms= %f, Irms= %f, apparentPower= %f, realPower= %f, powerFactor= %f",emon1.Vrms, emon1.Irms, apparentPower, emon1.realPower ,emon1.powerFactor);   
      //  debugV (" Vrms= %f, Adjusted Amps= %f, ApparentPower adjusted amps (V*Aa)= %f, powerFactor2= %f",emon1.Vrms,amps, apparentPower_2,powerFactor2);   
      //  Serial.printf ("\nVrms= %f, Irms= %f, offsetI= %f, apparentPower= %f, realPower= %f, powerFactor= %f",emon1.Vrms, emon1.Irms, emon1.offsetI, apparentPower, emon1.realPower ,emon1.powerFactor);   
      #endif        

      // Guard jsonPost against concurrent read from tskfn_EmonPublisher (Core 1).
      // Skip this update if the reader is still busy rather than racing on the buffer.
      if (jsonPostMutex &&
          xSemaphoreTake(jsonPostMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        static char jsonBuf[512];
        snprintf(jsonBuf, sizeof(jsonBuf),
          "json:{\"msg\":{\"source\":\"Cnt:%s\",\"data\":[{"
          "\"voltage\":\"%.0f\",\"amps\":\"%.1f\",\"power\":\"%.0f\",\"r_power\":\"%.0f\","
          "\"wh\":\"%.2f\",\"MTD_wh\":\"%.2f\",\"YTD_wh\":\"%.2f\","
          "\"PF\":\"%.2f\",\"Volts2\":\"[VOLTS2]\",\"Volts3\":\"[VOLTS3]\",\"HST_Amps\":\"[HST_AMPS]\",\"DCAmps\":\"[DCAMPS]\","
          "\"PFC\":\"%.2f\",\"IP\":\"%s\",\"R1\":\"%s\",\"R2\":\"%s\"}]}}",
          MyConfParam.v_PhyLoc.c_str(),
          supplyVoltage, Irms, realPower, realPower,
          wh / 1000.0f, MTD_Wh / 1000.0f, YTD_Wh / 1000.0f,
          powerFactor, powerFactor2,
          WiFi.localIP().toString().c_str(),
          digitalRead(RelayPin) == HIGH ? ON : OFF,
  #ifdef ESP32_2RBoard
          digitalRead(Relay1Pin) == HIGH ? ON : OFF
  #else
          ""
  #endif
        );
        strncpy(jsonPost, jsonBuf, sizeof(jsonPost) - 1);
        jsonPost[sizeof(jsonPost) - 1] = '\0';
        xSemaphoreGive(jsonPostMutex);
      }


      if (!ctWarmingUp && this->amps > MyConfParam.v_CT_MaxAllowed_current) {
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
      
      if (!ctWarmingUp && this->amps < MyConfParam.v_CT_MaxAllowed_current - 1.5) {
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

      {
        Chronos::DateTime _now = Chronos::DateTime::now();
        int _nd = _now.day(), _nm = _now.month(), _ny = _now.year(), _nh = _now.hour();

        // Only anchor the reset timestamps once NTP has given us a real date.
        // Before NTP syncs, Chronos returns the Unix epoch (year=1970, month=1,
        // day=1).  Anchoring on that value means the month and year comparisons
        // would diverge from the real date and trigger a false MTD/YTD reset at
        // the first 1 am after NTP sync — zeroing accumulators that should not reset.
        const bool validTime = (_ny >= 2024);
        if (validTime) {
          if (_last_reset_day   == -1) _last_reset_day   = _nd;
          if (_last_reset_month == -1) _last_reset_month = _nm;
          if (_last_reset_year  == -1) _last_reset_year  = _ny;
        }

        // Midnight checkpoint (hour 0): persist the full day's values before the
        // 1 AM reset window.  With zero load overnight the threshold-save never
        // fires, so without this checkpoint a reboot between midnight and 1 AM
        // loads stale flash values and the day's energy is lost.  Fires once per
        // calendar day; _last_midnight_save_day resets to -1 on every boot so the
        // save re-runs even after a reboot during hour 0.
        static int _last_midnight_save_day = -1;
        if (validTime && _nh == 0 && _nd != _last_midnight_save_day) {
          _last_midnight_save_day = _nd;
          saveCTReadings(wh, MTD_Wh, YTD_Wh, _last_reset_day, _last_reset_month, _last_reset_year);
          Serial.printf("\n[CT **  ] Midnight checkpoint (day=%d) wh=%.1f MTD=%.1f YTD=%.1f\n",
                        _nd, wh, MTD_Wh, YTD_Wh);
        }

        // Only apply resets once the anchor has been set from a valid time.
        bool resetFired = false;
        if (_last_reset_day   != -1 && _nd != _last_reset_day   && _nh == 1) { wh = 0;      _last_reset_day   = _nd; resetFired = true; Serial.printf("\n[CT **  ] Daily reset fired (day=%d)\n", _nd); }
        if (_last_reset_month != -1 && _nm != _last_reset_month && _nh == 1) { MTD_Wh = 0;  _last_reset_month = _nm; resetFired = true; Serial.printf("\n[CT **  ] MTD reset fired (month=%d)\n", _nm); }
        if (_last_reset_year  != -1 && _ny != _last_reset_year  && _nh == 1) { YTD_Wh = 0;  _last_reset_year  = _ny; resetFired = true; Serial.printf("\n[CT **  ] YTD reset fired (year=%d)\n", _ny); }
        // Persist immediately so a crash between 1am and the next threshold-save
        // cannot leave stale pre-reset values in flash.
        if (resetFired) saveCTReadings(wh, MTD_Wh, YTD_Wh, _last_reset_day, _last_reset_month, _last_reset_year);
      }

      if (!ctWarmingUp) {
        if (CTSaveThreshold > savethreshold) {
          // Save when the configured Wh threshold is crossed.
          // A 60-second minimum interval guards against edge-case write bursts;
          // LittleFS wear-leveling makes this the only protection needed.
          static unsigned long lastCTSaveMs = 0;
          static bool firstCTSave = true;
          unsigned long now = millis();
          if (firstCTSave || (now - lastCTSaveMs >= 60000UL)) {
            firstCTSave  = false;
            lastCTSaveMs = now;
            CTSaveThreshold = 0;
            saveCTReadings(wh, MTD_Wh, YTD_Wh, _last_reset_day, _last_reset_month, _last_reset_year);
            Serial.printf("\n[CT **  ] Saved CT Values %f Wh | %f MTD_Wh | %f YTD_Wh\n", wh, MTD_Wh, YTD_Wh);
          }
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
        display.setTextSize(1);
        #define StartRow 22 
        #define rect_height 30
        #define rect_width 60

        display.drawRect(0,StartRow,rect_width,rect_height, WHITE);
        display.drawRect(rect_width + 2 ,StartRow,rect_width,rect_height, WHITE);   
       
        // display.println("Power Readings");
        display.setCursor(1,1);    
        display.print(String(realPower, 0) + " w");
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
        display.setTextSize(1);
        display.print(String(Irms, 1));
        display.setTextSize(1);        
        display.print(" A");

        display.setCursor(rect_width + 10 ,StartRow + 2 );    
        display.println(F("Voltage"));
        display.setCursor(rect_width + 8,StartRow + 16 );   
        display.setTextSize(1);                 
        display.print(String(supplyVoltage, 0));
        display.setTextSize(1);              
        display.setTextColor(WHITE);
        display.println(" V");   
        }

        if (CURRENT_Display_Action == ACTION_DISPALY_2) {
        display.setCursor(8, StartRow + 2);
        display.println(F("P Factor"));
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(10,StartRow + 16);       
        display.setTextSize(1);
        display.print(String(powerFactor));
        display.setTextSize(1);        
 

        display.setCursor(rect_width + 10 ,StartRow + 2 );    
        display.println(F("R Power"));
        display.setCursor(rect_width + 8,StartRow + 16 );   
        display.setTextSize(1);                 
        display.print(String(realPower, 0));
        display.setTextSize(1);              
        display.setTextColor(WHITE);
  
        }

      #endif
  }




#endif





