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

#ifndef _CONFIGS_H__
#define _CONFIGS_H__

#include <Arduino.h>
#include <Scheduletimer.h>



#define RelayPin 05
#define Relay2Pin 16
#define ConfigInputPin 13
#define SwitchButtonPin 12 // revert to 12 when done testing
#define SwitchButtonPin2 13 // revert to 12 when done testing
#define InputPin12 12
#define InputPin14 14

// #define MAX_RELAYS 2

const uint16_t MaxWifiTrials = 100;

#define RETAINED true
#define NOT_RETAINED false
#define QOS2  2

#define ON "on"
#define OFF "off"
#define TOG "tog"

#define TOG_MODE 0
#define BTN_MODE 1

typedef struct TConfigParams {
  String v_ssid;
  String v_pass;
  String v_PhyLoc ;
  String v_MQTT_BROKER ;
  String v_MQTT_B_PRT ;
  String v_PUB_TOPIC1 ;
  String v_FRM_IP ;
  String v_FRM_PRT ;
  String v_ACSmultiple ;
  String v_ACS_Sensor_Model;
  String v_ttl ;                // TTL VALUE
  String v_ttl_PUB_TOPIC ;      // MQTT TTL publish topic
  String v_CURR_TTL_PUB_TOPIC;  // running TTL publish topic
  String v_i_ttl_PUB_TOPIC;     // TTL set/update topic
  String v_STATE_PUB_TOPIC ;
  String v_InputPin12_STATE_PUB_TOPIC ;
  String v_InputPin14_STATE_PUB_TOPIC ;
  String v_tta ;
  String v_Max_Current ;
  String v_ACS_AMPS;
  String v_timeserver ;
  String v_PIC_Active ;
  String v_MQTT_Active ;
  //String v_myppp ;
  String v_ntptz ;
  String v_LWILL_TOPIC ;
  String v_SUB_TOPIC1 ;
  String v_GPIO12_TOG ;
  String v_Copy_IO ;
  String v_ACS_Active ;
  String v_Update_now ;
  String v_TOGGLE_BTN_PUB_TOPIC ;
  uint8_t v_IN1_INPUTMODE;
  uint8_t v_IN2_INPUTMODE;
} TConfigParams;

  extern TConfigParams MyConfParam;
  extern String  MAC;
  //extern Schedule_timer relayttatimer;

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
