#pragma once

//#ifndef _TIMERCONG_H__
//#define _TIMERCONG_H__

#include "Arduino.h"
#include <ConfigParams.h>
//#include <JSONConfig.h>
//#include <Scheduletimer.h>
//#include <OneButton.h>
//#include <Bounce2.h>

typedef void (*fnptr)();
typedef void (*fnptr_a)(void* t);
typedef void (*fnptr_b)(int, void* t);

#define MAX_NUMBER_OF_TIMERS 4

// Sentinel `relay` value meaning "no relay attached" — lets a timer exist purely
// as a reusable schedule window (e.g. for Automation Rules' time-gating via
// isNodeTimerActiveNow()) without actuating any hardware. Chosen as 255 so it
// can never collide with a real relay index (relays.size() is at most a handful)
// and is skipped before it ever reaches the Chronos calendar / EventNames[] —
// see chronosInit() in main.cpp.
#define TIMER_NULL_RELAY 255
// #define buffer_size  1500 // json buffer size
typedef enum { TM_FAILURE, TM_FULL_SPAN, TM_DAILY_SPAN, TM_WEEKDAY_SPAN, TM_MONTHLY_SPAN, TM_YEARLY_SPAN } TimerType;

typedef struct TWeekdays {
  boolean Monday ;
  boolean Tuesday ;
  boolean Wednesday ;
  boolean Thursday;
  boolean Friday ;
  boolean Saturday ;
  boolean Sunday;
  boolean AllWeek;
  void clear() {
            this->Monday    = false;
            this->Tuesday   = false;
            this->Wednesday = false;
            this->Thursday  = false;
            this->Friday    = false;
            this->Saturday  = false;
            this->Sunday    = false;
            this->AllWeek   = false;
          }

} TWeekdays; // this is in preparation for separate relay configuations


 class NodeTimer
{
  private:

  public:
    uint8_t id;
    TimerType TM_type;
    String spanDatefrom;
    String spanDateto;
    String spantimefrom;
    String spantimeto;
    uint32_t secondsspan;
    TWeekdays * weekdays;
    uint16_t Mark_Hours;
    uint16_t Mark_Minutes;
    uint8_t  monthDay;
    boolean enabled;
    uint8_t relay;

    //char* Testchar;

    NodeTimer(uint8_t para_id
      //,unsigned int para_mark,
      //uint8_t para_marktype
      );

    NodeTimer(uint8_t para_id,
              String para_spanDatefrom,
              String para_spanDateto,
              String para_spantimefrom,
              String para_spantimeto,
              TWeekdays * para_weekdays,
              boolean para_enabled,
              uint8_t para_relay
            );

    ~NodeTimer();
    void watch();
};

// Flat (no-pointer) snapshot of a timer's schedule — safe to copy and store
// in a global array.  Populated once at boot and refreshed after every save.
struct TimerScheduleCache {
  bool      loaded;       // false = slot not configured (file absent)
  bool      enabled;
  uint8_t   relay;
  uint8_t   id;
  TimerType TM_type;
  char      spanDatefrom[12];   // "DD-MM-YYYY\0"
  char      spanDateto[12];
  char      spantimefrom[6];    // "HH:MM\0"
  char      spantimeto[6];
  uint16_t  Mark_Hours;
  uint16_t  Mark_Minutes;
  uint8_t   monthDay;
  TWeekdays weekdays;           // value copy — no heap pointer
};
extern TimerScheduleCache g_timerScheduleCache[MAX_NUMBER_OF_TIMERS];

// Populate one cache slot (1-based index) by reading the flash file once.
void refreshTimerCache(int timerIdx);
// Populate all slots — call once at boot.
void refreshAllTimerCache();

bool saveNodeTimer(AsyncWebServerRequest *request);
config_read_error_t loadNodeTimer(char* filename, NodeTimer &para_NodeTimer);
NodeTimer * gettimerbypin(uint8_t pn);

// Independently re-checks timer `timerId`'s own stored schedule (date range,
// weekday flags, time-of-day window) against "now" — used to gate Automation
// Rules by an existing timer's schedule window without touching the Chronos
// calendar (whose events are keyed by *target relay number*, not timer id —
// see main.cpp:1466 `Chronos::Event(NTmr.relay, ...)`).
bool isNodeTimerActiveNow(uint8_t timerId);

//#endif
