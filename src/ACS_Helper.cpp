#include "ACS712.h"
#include <ACS_Helper.h>

void ACS_Calibrate_Start(Relay &arelay, ACS712 &asensor){
	Serial.println(F("[INFO   ] Calibrating sensor... Ensure that no current flows through the sensor at this moment"));
	int type = arelay.RelayConfParam->v_ACS_Sensor_Model.toInt();
	float sensitivity = 0.100;
	switch (type) {
		case 5:
			sensitivity = 0.185;
			break;
		case 20:
			sensitivity = 0.100;
			break;
		case 30:
			sensitivity = 0.066;
			break;
	}
	Serial.println("[INFO   ] Sensitivity = " + String(sensitivity));
	asensor.setSensitivity(sensitivity);
	int zero = asensor.calibrate();
	Serial.print(F("[INFO   ] Done! Zero point for this sensor = "));
	Serial.println(zero);
	asensor.setZeroPoint(zero);
	//ticker_ACS712.interval(1000);

	if (arelay.RelayConfParam->v_ACS_Active) {
		arelay.start_ACS712();
		arelay.start_ACS712_mqtt();
	}

}
