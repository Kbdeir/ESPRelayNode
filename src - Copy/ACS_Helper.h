#ifndef _ACSHELPER_H__
#define _ACSHELPER_H__
#include "Arduino.h"
#include "ACS712.h"
#include "RelayClass.h"

void ACS_Calibrate_Start(Relay &arelay, ACS712 &asensor);

#endif
