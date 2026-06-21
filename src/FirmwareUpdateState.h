#pragma once

extern volatile bool firmwareUpdateInProgress;

void firmwareUpdateBegin();
void schedulerStopForFirmwareUpdate();
