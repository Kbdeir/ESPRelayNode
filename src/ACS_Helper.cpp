#include "ACS712.h"
#include <ACS_Helper.h>

void ACS_Calibrate_Start(Relay &arelay, ACS712 &asensor){
	Serial.println(F("Calibrating sensor... Ensure that no current flows through the sensor at this moment"));
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
	Serial.println("Sensitivity = " + String(sensitivity));
	asensor.setSensitivity(sensitivity);
	int zero = asensor.calibrate();
	Serial.println(F("Done!"));
	Serial.println("Zero point for this sensor = " + zero);
	asensor.setZeroPoint(zero);
	//ticker_ACS712.interval(1000);

	if (arelay.RelayConfParam->v_ACS_Active == "1") {
		arelay.start_ACS712();
		arelay.start_ACS712_mqtt();
	}

}
