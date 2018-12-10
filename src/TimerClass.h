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


#define buffer_size  1500 // json buffer size

typedef enum { TM_FULL_SPAN, TM_WEEKDAY_SPAN, TM_DAILY_SPAN } TimerType;

typedef struct TWeekdays {
  boolean Monday ;
  boolean Tuesday ;
  boolean Wednesday ;
  boolean Thursday;
  boolean Friday ;                // TTL VALUE
  boolean Saturday ;      // MQTT TTL publish topic
  boolean Sunday;  // running TTL publish topic
  boolean AllWeek;     // TTL set/update topic

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
    unsigned int mark;
    uint8_t marktype;
    boolean enabled;
    uint16_t fyear;

    NodeTimer(uint8_t para_id,
      unsigned int para_mark,
      uint8_t para_marktype
      );

    NodeTimer(uint8_t para_id,
              String para_spanDatefrom,
              String para_spanDateto,
              String para_spantimefrom,
              String para_spantimeto,

              TWeekdays * para_weekdays,
              unsigned int para_mark,
              uint8_t para_marktype,
              boolean para_enabled
            );

    ~NodeTimer();
    void watch();



};

bool saveNodeTimer(AsyncWebServerRequest *request);
config_read_error_t loadNodeTimer(char* filename, NodeTimer &para_NodeTimer);
NodeTimer * gettimerbypin(uint8_t pn);

#endif
