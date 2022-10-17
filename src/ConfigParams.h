#pragma once
/*the correct pin mapping is the following [1][2] (NodeMCU on the left and ESP8266 on the right):
    D0 = GPIO16;
    D1 = GPIO5;
    D2 = GPIO4;
    D3 = GPIO0;
    D4 = GPIO2;
    D5 = GPIO14;
    D6 = GPIO12;
    D7 = GPIO13;
    D8 = GPIO15;
    D9 = GPIO3;
    D10 = GPIO1;
    LED_BUILTIN = GPIO16 (auxiliary constant for the board LED, not a board pin);
*/

#define softwareVersion  " - SW 1.1"

#include <defines.h>

#ifndef _CONFIGS_H__
#define _CONFIGS_H__

#include <Arduino.h>
#include <Scheduletimer.h>
#include <IPAddress.h>

 #ifdef HWESP32
    #define led                 02 // led on IO2  
    #define RelayPin            25 // relay on IO25
    #define HomeKitt_PIN_SWITCH 25 // Homekit is on (Relay0 = RelayPin) pin only

    // #ifndef _emonlib_
    #if not defined _emonlib_ && not defined _pressureSensor_
      #define InputPin01  33 // Input1 on IO33
    #endif    
    #define InputPin02  16 // Input2 on IO16    
    #define InputPin03  17 // Input3 on IO17
    #define InputPin04  32 // Input4 on IO32  // SET TO 21 FOR FIRST VERSION OF ESP32 BOARD, CHANGED TO 32 TO AVOID USING SDL/SDC
    #define InputPin05  26 // Input5 on IO26   
    #define InputPin06  27 // Input6 on IO27  

    #define ConfigInputPin      04 // configuration pin on IO4
     #if not defined _emonlib_ && not defined _pressureSensor_
    #define TempSensorPin       InputPin01 // 33
    #endif
    #define SecondTempSensorPin InputPin02 // 16    
    #define TRIG_PIN InputPin03
    #define ECHO_PIN InputPin04

    #ifdef ESP32_2RBoard
      #define Relay1Pin 18    
      #define InputPin01  33 // Input1 on IO33       
    #endif        
#endif


 #ifdef HWver03
    #define led         16
    #define InputPin02  02 
    #define SwitchButtonPin2 12 // revert to 12 when done testing  
  #endif

  #ifdef HWver02
    #define led         02 
    #define SwitchButtonPin2 13 // revert to 12 when done testing
 #endif

 #if defined (HWver02)  || defined (HWver03)
    #define RelayPin 05
    #define HomeKitt_PIN_SWITCH 05
    //#define Relay2Pin 16
    #define InputPin12 12
    #define ConfigInputPin 13 // was 13. should return to 13
    #define InputPin14 14

    #define TempSensorPin 14
    #define SecondTempSensorPin 12
    #define TRIG_PIN 14
    #define ECHO_PIN 12    
 #endif


 #ifdef HWver03_4R
    #define led       05  
    #define RelayPin  16
    #define HomeKitt_PIN_SWITCH 16 // Homekit is on (Relay0 = RelayPin) pin only
    #define Relay1Pin 14    
    #define Relay2Pin 12        
    #define Relay3Pin 13 
    #define InputPin02  02 
    #define InputPin14  00
    #define InputPin13  04    
    #define ConfigInputPin 15 
    #define TempSensorPin   2
    #define SecondTempSensorPin 4    
 #endif


#define RETAINED true
#define NOT_RETAINED false
#define QOS2  2
#define ON "on"
#define OFF "off"
#define TOG "tog"
#define TOG_MODE 0
#define BTN_MODE 1

const uint16_t MaxWifiTrials = 500;


typedef struct IPAdr {
  uint8_t bytes[4];
}IPAdr;

typedef struct Trelayconf {
  uint8_t v_relaynb;
  String v_PhyLoc ;
  String v_PUB_TOPIC1 ;
  String v_STATE_PUB_TOPIC ;
  String v_LWILL_TOPIC ;
  String v_SUB_TOPIC1 ;
  String v_ttl_PUB_TOPIC ;      
  String v_CURR_TTL_PUB_TOPIC; 
  String v_i_ttl_PUB_TOPIC;    
  String v_TemperatureValue;     
  String v_AlexaName;     
  uint32_t v_ttl ;
  uint32_t v_tta ;
  uint16_t v_ACS_elasticity ;  
  String v_ACS_Sensor_Model;
  boolean v_ACS_Active ;
  uint8_t v_Max_Current ;
  String v_ACS_AMPS;

  uint8_t v_IN0_INPUTMODE;
  uint8_t v_IN1_INPUTMODE;
  uint8_t v_IN2_INPUTMODE;
} Trelayconf;


typedef struct TConfigParams {
  String v_ssid;
  String v_pass;
  String v_PhyLoc ;

  IPAddress v_timeserver ;
  signed char v_ntptz ;
  uint8_t v_MQTT_Active ;
  // IPAddress v_MQTT_BROKER ;
  String v_MQTT_BROKER ;
  uint16_t v_MQTT_B_PRT ;

  String v_mqttUser;
  String v_mqttPass;

  IPAddress v_Pingserver ;

  IPAddress v_FRM_IP ;
  uint16_t v_FRM_PRT ;
  boolean v_Update_now ;

#ifndef HWESP32
  String v_TOGGLE_BTN_PUB_TOPIC ;
  String v_InputPin12_STATE_PUB_TOPIC ;
  String v_InputPin14_STATE_PUB_TOPIC ;
  uint8_t v_IN0_INPUTMODE;
  uint8_t v_IN1_INPUTMODE;
  uint8_t v_IN2_INPUTMODE;
//zzzzzzzzzz
  String v_InputPin03_STATE_PUB_TOPIC ;
  String v_InputPin04_STATE_PUB_TOPIC ;
  String v_InputPin05_STATE_PUB_TOPIC ;
  String v_InputPin06_STATE_PUB_TOPIC ;  
  uint8_t v_IN3_INPUTMODE;
  uint8_t v_IN4_INPUTMODE;
  uint8_t v_IN5_INPUTMODE;  
  uint8_t v_IN6_INPUTMODE;  
#endif  

#ifdef HWESP32
  String v_TOGGLE_BTN_PUB_TOPIC ;
  String v_InputPin12_STATE_PUB_TOPIC ;
  String v_InputPin14_STATE_PUB_TOPIC ;

  String v_InputPin01_STATE_PUB_TOPIC ;
  String v_InputPin02_STATE_PUB_TOPIC ;
  String v_InputPin03_STATE_PUB_TOPIC ;
  String v_InputPin04_STATE_PUB_TOPIC ;
  String v_InputPin05_STATE_PUB_TOPIC ;
  String v_InputPin06_STATE_PUB_TOPIC ;

  uint8_t v_IN0_INPUTMODE;
  uint8_t v_IN1_INPUTMODE;
  uint8_t v_IN2_INPUTMODE;
  uint8_t v_IN3_INPUTMODE;
  uint8_t v_IN4_INPUTMODE;
  uint8_t v_IN5_INPUTMODE;  
  uint8_t v_IN6_INPUTMODE;
#endif  

  String v_Sonar_distance ;
  uint16_t v_Sonar_distance_max;
  uint16_t v_Reboot_on_WIFI_Disconnection;  

  uint8_t v_CurrentTransformer_max_current ;
  uint16_t v_calibration;
  String v_CurrentTransformerTopic ;
  uint16_t v_ToleranceOffTime;
  uint16_t v_ToleranceOnTime;    
  uint16_t v_CT_MaxAllowed_current;      
  float v_CT_adjustment;
  uint8_t v_CT_saveThreshold;  
  uint8_t v_Screen_orientation;  
  
  uint16_t maSensor_L_Calibration, maSensor_HighCalibration, maSensor_max_range;  
  String maSTopic ;

} TConfigParams;

typedef struct TIRMap {
int8_t I1;
int8_t I2;
int8_t I3;
int8_t I4;
int8_t I5;
int8_t I6;
int8_t I7;
int8_t I8;
int8_t I9;
int8_t I10;

int8_t R1;
int8_t R2;
int8_t R3;
int8_t R4;
int8_t R5;
int8_t R6;
int8_t R7;
int8_t R8;
int8_t R9;
int8_t R10;
} TIRMap;

  extern TConfigParams MyConfParam;
  extern TIRMap myIRMap;
  extern String  MAC;
  String CID();
  void relayon(void* obj);

  #include <RelayClass.h>



#endif //CONFIGPARAMS_H

/*
GPIO Pin 	I/O Index Number
GPIO0 	 3
GPIO1 	 10
GPIO2 	 4
GPIO3 	 9
GPIO4 	 2
GPIO5 	 1
GPIO6 	 N/A
GPIO7 	 N/A
GPIO8 	 N/A
GPIO9 	 11
GPIO10 	 12
GPIO11 	 N/A
GPIO12 	 6
GPIO13 	 7
GPIO14 	 5
GPIO15 	 8
GPIO16 	 0
*/


