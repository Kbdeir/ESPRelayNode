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

 class NodeTimer
{
  private:
    uint8_t id;
  public:
    NodeTimer(uint8_t id);
    ~NodeTimer();
    void watch();


};

NodeTimer * gettimerbypin(uint8_t pn);

#endif
