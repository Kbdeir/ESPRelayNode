#include <FirmwareUpdateState.h>
#include <defines.h>

#ifdef OLED_1306
extern void oledWake();
#endif

volatile bool firmwareUpdateInProgress = false;

void firmwareUpdateBegin()
{
  firmwareUpdateInProgress = true;
#ifdef OLED_1306
  oledWake();
#endif
}
