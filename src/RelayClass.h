#ifndef _RELAYSCONG_H__
#define _RELAYSCONG_H__
//#include <InputClass.h>
#include "Arduino.h"
#include <ConfigParams.h>
#include <JSONConfig.h>
#include <OneButton.h>
#include <Bounce2.h>
// #include <InputClass.h>

typedef void (*fnptr)();
typedef void (*fnptr_a)(void* t);
typedef void (*fnptr_b)(int, void* t);

const int DEFLEN = 20;
#define buffer_size  1800 // json buffer size

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
   fnptr_a fgeneralinLoopFunc;

   void freelockfunc(void);
   void freelockreset() ;

   unsigned long pmillis;
   unsigned long freeinterval;

  public:
    boolean lockupdate;
    //TConfigParams *RelayConfParam;
    Trelayconf *RelayConfParam;
    //Trelayconf *RelayxxConfParam;
    Schedule_timer *ticker_relay_tta;
    Schedule_timer *ticker_relay_ttl;
    Schedule_timer *freelock;
    boolean rchangedflag;
    boolean timerpaused;
    boolean hastimerrunning;
    uint8_t r_in_mode;
    Relay(uint8_t p,
          fnptr_a ttlcallback,
          fnptr_a ttlupdatecallback,
          fnptr_a ACSfunc,
          fnptr_a ACSfunc_mqtt,
          fnptr_a onchangeInterruptService,
          fnptr_a ttacallback
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
    boolean loadrelayparams2(uint8_t rnb);
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
    uint8_t getRelayPin();
    void attachLoopfunc(fnptr_a GeneralLoopFunc);
    void mdigitalWrite(uint8_t pn, uint8_t v);
};

  Relay * getrelaybypin(uint8_t pn);
  Relay * getrelaybynumber(uint8_t pn);

  void mkRelayConfigName(char name[], uint8_t rnb);
#endif
