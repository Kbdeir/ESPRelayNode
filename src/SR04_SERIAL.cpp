#ifndef _SR04_SERIAL_
#define _SR04_SERIAL_

#include "defines.h"
#include "SR04_SERIAL.h"

#define MYPORT_TX 14
#define MYPORT_RX 12

byte serialinbytes[4];
int bytesin = 0;
SoftwareSerial myPort;

double readSerialUltrasoundSensor(void) {
double finalresult = 0 ;
      while (myPort.available()) {
        byte ch = (byte)myPort.read();
        serialinbytes[bytesin] = ch;
        //Serial.print(PSTR("\nResult:"));
        //Serial.print(ch < 0x10 ? PSTR(" 0") : PSTR(" "));
        //Serial.print(ch, HEX);
        yield();
        bytesin++;
        if (bytesin == 4) {
          finalresult = (serialinbytes[1] * (16 * 16)) + (serialinbytes[2] * 1) ;
          // Serial.print(PSTR("\nResult SR04 Serial :"));
          // Serial.println(finalresult/10);    
          bytesin = 0;      
        }
      }
      return finalresult/10;
}


void beginSerialSR04(void) {
  #ifdef SR04_SERIAL
  myPort.begin(9600,SWSERIAL_8N1, MYPORT_RX, MYPORT_TX,false);
  myPort.enableIntTx(false);
    if (!myPort) { // If the object did not initialize, then its configuration is invalid
      Serial.println(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>Invalid SoftwareSerial pin configuration, check config"); 
    }   
  #endif
}  

#endif