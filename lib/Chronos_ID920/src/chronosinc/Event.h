/*
 * Event.h
 * Base class for various time marks -- sets of 0-dimensional calendar events (points in time,
 * e.g. "every Sunday at 15h00").
 * See the PointEvents and Calendar examples for usage.
 *
 *  http://flyingcarsandstuff.com/projects/chronos
 *  Created on: Dec 18, 2015
 *      Author: Pat Deegan
 *      Part of the Chronos library project
 *      Copyright (C) 2015 Pat Deegan, http://psychogenic.com
 *
 *  This file is part of the Chronos embedded datetime/calendar library.
 *
 *     Chronos is free software: you can redistribute it and/or modify
 *     it under the terms of the GNU Lesser Public License as published by
 *     the Free Software Foundation, either version 3 of the License, or
 *     (at your option) any later version.
 *
 *     Chronos is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU Lesser Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser Public License
 *    along with Chronos.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CHRONOS_INTINCLUDES_EVENT_H_
#define CHRONOS_INTINCLUDES_EVENT_H_


#include "../chronosinc/timeExtInc.h"
#include "../chronosinc/timeTypes.h"

namespace Chronos {

class DateTime; // forward decl

namespace Mark {

class Event {
public:
	Event();
	virtual ~Event();

	virtual DateTime next(const DateTime & dt) const = 0;
	virtual DateTime previous(const DateTime & dt) const = 0 ;

	virtual Event * clone() const = 0;


	void listNext(uint8_t number, DateTime into[], const DateTime & dt) const ;
	void listPrevious(uint8_t number, DateTime into[], const DateTime & dt) const;



protected:
	typedef enum {Previous=0, Next} Direction;
private:
	Event(const Event & other);




};

} /* namespace Event */

} /* namespace Chronos */

#endif /* CHRONOS_INTINCLUDES_EVENT_H_ */
