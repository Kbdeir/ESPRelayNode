#ifndef _TIMERCONG_H__
#define _TIMERCONG_H__

#include "Arduino.h"
#include <ConfigParams.h>
#include <JSONConfig.h>
#include <Scheduletimer.h>
#include <OneButton.h>
#include <Bounce2.h>

typedef void (*fnptr)();
typedef void (*fnptr_a)(void* t);
typedef void (*fnptr_b)(int, void* t);

#define MAX_NUMBER_OF_TIMERS 4
#define buffer_size  1500 // json buffer size
typedef enum { TM_FAILURE, TM_FULL_SPAN, TM_DAILY_SPAN, TM_WEEKDAY_SPAN, TM_MONTHLY_SPAN, TM_YEARLY_SPAN } TimerType;

typedef struct TWeekdays {
  boolean Monday ;
  boolean Tuesday ;
  boolean Wednesday ;
  boolean Thursday;
  boolean Friday ;                // TTL VALUE
  boolean Saturday ;      // MQTT TTL publish topic
  boolean Sunday;  // running TTL publish topic
  boolean AllWeek;     // TTL set/update topic
  boolean clear() {
            Monday    = false;
            Tuesday   = false;
            Wednesday = false;
            Thursday  = false;
            Friday    = false;
            Saturday  = false;
            Sunday    = false;
            AllWeek   = false;
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
    boolean enabled;

    char* Testchar;

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
              boolean para_enabled
            );

    ~NodeTimer();
    void watch();
};

bool saveNodeTimer(AsyncWebServerRequest *request);
config_read_error_t loadNodeTimer(char* filename, NodeTimer &para_NodeTimer);
NodeTimer * gettimerbypin(uint8_t pn);

#endif
