//#ifndef _RELAYSCONG_H__
//#define _RELAYSCONG_H__

#pragma once
//#include <InputClass.h>
#include "Arduino.h"
#include <ConfigParams.h>
#include <JSONConfig.h>
//#include <OneButton.h>
//#include <Bounce2.h>
// #include <InputClass.h>
#ifdef ESP32
extern "C"
{
  #include <lwip/icmp.h> // needed for icmp packet definitions
	 #include "freertos/FreeRTOS.h"
	 #include "freertos/timers.h"  
}
#endif

typedef void (*fnptr)();
typedef void (*fnptr_a)(void* t);
typedef void (*fnptr_b)(int, void* t);

const int DEFLEN_ = 20;
#define buffer_size 1200

class Relay
{
  private:
   uint8_t pin;
   char RelayTag[DEFLEN_];
   int IDRelayTag;
   const char* filename = "/Relayconfig.json";

   uint8_t fswitchbutton;

   #ifdef ESP32
    fnptr_a_xtimer xtfttlcallback;      
    fnptr_a_xtimer xtfttacallback;
   #endif

   Schedule_timer *ticker_ACS712;
   fnptr_a fticker_ACS712_func;
   Schedule_timer *ticker_ACS_MQTT;
   fnptr_a fticker_ACS712_mqtt_func;

   fnptr_a fonchangeInterruptService;
   fnptr_a fgeneralinLoopFunc;

   void freelockfunc(void);
   void freelockreset() ;

   unsigned long pmillis;
   unsigned long freeinterval;

  public:
    fnptr_a fttlcallback;
    fnptr_a fttlupdatecallback; 
    fnptr_a fttacallback;      

    #ifdef ESP32
      TimerHandle_t ttltimer;
      TimerHandle_t ttatimer;
      portMUX_TYPE  timerFlagsMux;
    #endif
    
    Schedule_timer *ticker_relay_tta;
    Schedule_timer *ticker_relay_ttl;
    Schedule_timer *freelock;    

    volatile uint32_t running_TTA = 0;
    volatile uint32_t running_TTL = 0;

    // Set by FreeRTOS timer callbacks; consumed by watch() on the Arduino task.
    volatile bool ttl_expired = false;
    volatile bool ttl_update  = false;
    volatile bool tta_expired = false;

    Trelayconf *RelayConfParam;
    volatile boolean lockupdate;
    volatile boolean rchangedflag;
    boolean timerpaused;
    boolean hastimerrunning;
    uint8_t r_in_mode;
    uint8_t relay_previous_state = 0;
    uint8_t       persistLastVal = 0xFF; // last value written to flash (0xFF = never written)

    // Last ON/OFF value published to v_PUB_TOPIC1 (-1 = not yet published).
    // v_PUB_TOPIC1 is also the subscribed command topic, so publishing to it
    // causes the broker to echo the message back. Used to avoid republishing
    // an unchanged value.
    int8_t lastPub1State = -1;
    // Number of v_PUB_TOPIC1 publishes made by us that are still awaiting
    // their broker echo. onMqttMessage swallows that many ON/OFF messages on
    // v_PUB_TOPIC1 (regardless of value/order) instead of re-applying them to
    // the relay - otherwise an echo would re-trigger the state-change handler
    // and the relay would cycle on/off forever, especially under rapid
    // consecutive toggles where echoes can arrive out of order.
    volatile uint8_t pendingEchoCount = 0;


    // Constructor
    Relay(uint8_t p,
          fnptr_a ttlcallback,
          fnptr_a ttlupdatecallback,
          fnptr_a ACSfunc,
          fnptr_a ACSfunc_mqtt,
          fnptr_a onchangeInterruptService,
          fnptr_a ttacallback
          /*
          #ifdef ESP32
            ,fnptr_a_xtimer TTL_CallBack             
            ,fnptr_a_xtimer TTA_CallBack
          #endif
          */
        );


    ~Relay();

    String getRelayPubTopic();
    void setRelayConfig(Trelayconf * RelayConf);
    Trelayconf * getRelayConfig();
    void setRelayTag(char *Relayt);
    void setIdNumber(int id);
    char *getName();
    int getIdNumber();
    //boolean loadrelayparams();
    boolean loadrelayparams(uint8_t rnb);//;

    void watch();
    void start_ttl_timer();
    void stop_ttl_timer();

    void start_tta_timer();
    void stop_tta_timer();    

    void start_ACS712();
    void stop_ACS712();
    void start_ACS712_mqtt();
    void stop_ACS712_mqtt();

    uint32_t getRelayTTLperiodscounter();
    void setRelayTTL_Timer_Interval(uint32_t interval);
    void setRelayTTA_Timer_Interval(uint32_t interval);    
    ksb_status_t TTLstate();
    int readrelay ();
    bool readPersistedrelay ();
    bool savePersistedrelay ();    
    uint8_t getRelayPin();
    void attachLoopfunc(fnptr_a GeneralLoopFunc);
    void mdigitalWrite(uint8_t pn, uint8_t v);

};

  Relay * getrelaybypin(uint8_t pn);
  Relay * getrelaybynumber(uint8_t pn);

  void mkRelayConfigName(char name[], uint8_t rnb);
//#endif
