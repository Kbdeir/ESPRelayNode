/* Schedule_timer library code is placed under the MIT license
 * Copyright (c) 2018 Stefan Staub
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "Scheduletimer.h"

Schedule_timer::Schedule_timer()
    : timer(0), repeat(0), resolution(MILLIS_), user_res(SECONDS_),
      counts(0), ksb_seconds(0), status(STOPPED_),
      callback(nullptr), periodcallback(nullptr),
      periodcallback_interval(1), lastTime(0),
      ksb_lastsecondcounter(0), diffTime(0), enabled(false) {}

Schedule_timer::Schedule_timer(int a, int b)
    : timer(a), repeat(b), resolution(MILLIS_), user_res(SECONDS_),
      counts(0), ksb_seconds(0), status(STOPPED_),
      callback(nullptr), periodcallback(nullptr),
      periodcallback_interval(1), lastTime(0),
      ksb_lastsecondcounter(0), diffTime(0), enabled(false) {}

Schedule_timer::Schedule_timer(fnptr_a callback, uint32_t timer, uint16_t repeat, ksb_resolution_t resolution,
			fnptr_a periodcallback, ksb_user_resolution_t user_res ) {

		this->resolution = resolution;
		if (resolution == MICROS_) timer = timer * 1000;
		periodcallback_interval = 1;
		if (user_res == MINUTES_) {
			timer = timer * 60;
			periodcallback_interval = 60;
		}
		this->user_res = user_res;
		this->timer = timer;
		this->repeat = repeat;
		this->callback = callback;
		this->periodcallback = periodcallback;
		enabled = false;
		lastTime = 0;
		ksb_lastsecondcounter = 0;
		counts = 0;
		ksb_seconds = 0;
		diffTime = 0;
		}


Schedule_timer::~Schedule_timer() {}

void Schedule_timer::start() {
	if (callback == NULL) return;
  	//if (periodcallback == NULL) return;
	if (resolution == MILLIS_) lastTime = millis();
	else lastTime = micros();
	enabled = true;
	counts = 0;
	ksb_seconds = 0;
	status = RUNNING_;
	}

void Schedule_timer::resume() {
	if (callback == NULL) return;
	//if (periodcallback == NULL) return;
	if (resolution == MILLIS_) lastTime = millis() - diffTime;
	else lastTime = micros() - diffTime;
	if (status == STOPPED_) counts = 0;
	enabled = true;
	status = RUNNING_;
	}

void Schedule_timer::stop() {
	enabled = false;
	counts = 0;
	ksb_seconds = 0;
	status = STOPPED_;
	}

void Schedule_timer::pause() {
	if (resolution == MILLIS_) diffTime = millis() - lastTime;
	else diffTime = micros() - lastTime;
	enabled = false;
	status = PAUSED_;
	}

void Schedule_timer::update(void* p_obj) {
	if (tick(p_obj)) callback(p_obj);
	//if (tick(p_obj)) periodcallback(p_obj);
	}

bool Schedule_timer::tick(void* p_obj) {
	if (!enabled)	return false;
	if (repeat > 0) {
		if (repeat - counts == 0) {
			this->stop();
			return false;
		}
	}
	if (resolution == MILLIS_) {
		unsigned long now = millis();
		if ((now - lastTime) >= timer) {
			lastTime = now;
			counts++;
			return true;
		}
		if ((now - ksb_lastsecondcounter) >= 1000 * periodcallback_interval) {
			ksb_lastsecondcounter = now;
			ksb_seconds++;
			if (periodcallback != NULL) periodcallback(p_obj);
		}
	} else {
		unsigned long now = micros();
		if ((now - lastTime) >= timer) {
			lastTime = now;
			counts++;
			return true;
		}
		if ((now - ksb_lastsecondcounter) >= 1000000 * periodcallback_interval) {
			ksb_seconds++;
			ksb_lastsecondcounter = now;
			if (periodcallback != NULL) periodcallback(p_obj);
		}
	}
	return false;
	}

void Schedule_timer::interval(uint32_t timer) {
	// IMPORTANT: pass the same unit you used at construction.
	// For MICROS_ timers, this multiplies by 1000 again — the same scaling the
	// constructor applied. Do NOT pre-scale before calling interval() on a MICROS_ timer.
	if (resolution == MICROS_) timer = timer * 1000;
	if (user_res == MINUTES_) {
		timer = timer * 60;
		periodcallback_interval = 60;
	}
	this->timer = timer;
	}

uint32_t Schedule_timer::elapsed() {
	if (resolution == MILLIS_) return millis() - lastTime;
	else return micros() - lastTime;
	}

ksb_status_t Schedule_timer::state() {
	return status;
	}

uint32_t Schedule_timer::counter() {
	return counts;
	}

uint32_t Schedule_timer::periodscounter() {
		return ksb_seconds;
		}
