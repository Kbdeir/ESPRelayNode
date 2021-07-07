
//#ifndef Schedule_timer_H
//#define Schedule_timer_H
#pragma once

#include "Arduino.h"

enum ksb_resolution_t {MICROS_, MILLIS_, MICROS_MICROS_};
enum ksb_user_resolution_t {SECONDS_, MINUTES_};
enum ksb_status_t {STOPPED_, RUNNING_, PAUSED_};

typedef void (*fptr)();
typedef void (*fnptr_a)(void* t);

class Schedule_timer {

private:

  bool tick(void* p_obj);
  bool enabled;
  uint32_t timer;
  uint16_t repeat;
  ksb_resolution_t resolution = MICROS_;
  ksb_user_resolution_t user_res = SECONDS_;
  uint32_t counts;
  uint32_t ksb_seconds;
  ksb_status_t status;
  fnptr_a callback;
  fnptr_a periodcallback;
  uint8_t periodcallback_interval;
  uint32_t lastTime;
  uint32_t ksb_lastsecondcounter;
  uint32_t diffTime;

public:
  Schedule_timer();
  Schedule_timer(int a, int b);
  Schedule_timer(fnptr_a callback, uint32_t timer, uint16_t repeat = 0, ksb_resolution_t resolution = MICROS_,fnptr_a periodcallback = NULL, ksb_user_resolution_t user_res = SECONDS_);

	~Schedule_timer();
	void start();
	void resume();
	void pause();
	void stop();
	void update(void* p_obj);
	void interval(uint32_t timer);
	uint32_t elapsed();
	ksb_status_t state();
	uint32_t counter();
	uint32_t periodscounter();


};



//#endif
