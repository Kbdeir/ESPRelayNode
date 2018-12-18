#ifndef _RELAYSCONG_H__
#define _RELAYSCONG_H__

#include "Arduino.h"
#include <ConfigParams.h>
#include <JSONConfig.h>
#include <Scheduletimer.h>
#include <OneButton.h>
#include <Bounce2.h>

typedef void (*fnptr)();
typedef void (*fnptr_a)(void* t);
typedef void (*fnptr_b)(int, void* t);

const int DEFLEN = 20;
#define buffer_size  1500 // json buffer size



typedef struct TRelayConfigParams {
  String v_PhyLoc ;
  String v_PUB_TOPIC1 ;
  String v_ACSmultiple ;
  String v_ACS_Sensor_Model;
  String v_ttl ;                // TTL VALUE
  String v_ttl_PUB_TOPIC ;      // MQTT TTL publish topic
  String v_CURR_TTL_PUB_TOPIC;  // running TTL publish topic
  String v_i_ttl_PUB_TOPIC;     // TTL set/update topic
  String v_STATE_PUB_TOPIC ;
  String v_tta ;
  String v_Max_Current ;
  String v_ACS_AMPS;
  String v_MQTT_Active ;
  String v_LWILL_TOPIC ;
  String v_SUB_TOPIC1 ;
  String v_GPIO12_TOG ;
  String v_Copy_IO ;
  boolean v_ACS_Active ;
} TRelayConfigParams; // this is in preparation for separate relay configuations

 class Relay
{
  private:
   uint8_t pin;
   char RelayTag[DEFLEN];
   int IDRelayTag;
   const char* filename = "/Relayconfig.json";

   uint8_t fswitchbutton;

   fnptr_a fttlcallback;
   fnptr_a fttlupdatecallback;
   fnptr_a fttacallback;

   Schedule_timer *ticker_ACS712;
   fnptr_a fticker_ACS712_func;

   Schedule_timer *ticker_ACS_MQTT;
   fnptr_a fticker_ACS712_mqtt_func;

   fnptr_a fonchangeInterruptService;
   fnptr_a fon_associatedbtn_change;
   fnptr_a fonclick;
   fnptr_a fgeneralinLoopFunc;

   void freelockfunc(void);
   void freelockreset() ;

   //unsigned long cmillis;
   unsigned long pmillis;
   unsigned long freeinterval;


  public:
    boolean lockupdate;
    TConfigParams *RelayConfParam;
    OneButton *fbutton;
    Bounce *btn_debouncer;
    Schedule_timer *ticker_relay_tta;
    Schedule_timer *ticker_relay_ttl;
    Schedule_timer *freelock;
    boolean rchangedflag;
    boolean timerpaused;

  //  Relay(uint8_t p);
    Relay(uint8_t p,
          fnptr_a ttlcallback,
          fnptr_a ttlupdatecallback,
          fnptr_a ACSfunc,
          fnptr_a ACSfunc_mqtt,
          fnptr_a onchangeInterruptService,
          fnptr_a ttacallback );


    ~Relay();

    String getRelayPubTopic();
    void setRelayConfig(TConfigParams * RelayConf);
    TConfigParams * getRelayConfig();
    void setRelayTag(char *Relayt);
    void setIdNumber(int id);
    char *getName();
    int getIdNumber();
    boolean SaveDefautRelayParams();
    boolean loadrelayparams();
    void watch();
    void start_ttl_timer();
    void stop_ttl_timer();
    void start_ACS712();
    void stop_ACS712();
    void start_ACS712_mqtt();
    void stop_ACS712_mqtt();
    uint32_t getRelayTTLperiodscounter();
    void setRelayTTT_Timer_Interval(uint32_t interval);
    ksb_status_t TTLstate();
    int readrelay ();
    void attachSwithchButton(uint8_t switchbutton,
                            fnptr_a intSvcfunc,
                            fnptr_a OnebtnSvcfunc);

    uint8_t getRelaySwithbtn();
    uint8_t getRelayPin();
    uint8_t getRelaySwithbtnState();
    void attachLoopfunc(fnptr_a GeneralLoopFunc);
    void mdigitalWrite(uint8_t pn, uint8_t v);
  //  Relay * relayofpin(uint8_t pn);
};

  Relay * getrelaybypin(uint8_t pn);

#endif
