#include <ConfigParams.h>

boolean ftimesynced = false;
boolean CalendarNotInitiated = true;

TConfigParams MyConfParam;

String CID(){
  #ifdef ESP32
    char buf[16];
    uint64_t chipid = ESP.getEfuseMac();
    ltoa(chipid,buf,10);
    return String(buf);
  #else
    return String(ESP.getChipId());
  #endif
}

void relayon(void* obj){
  if (obj != NULL) {
    Relay * rly;
    rly = static_cast<Relay *>(obj);
    rly->mdigitalWrite(rly->getRelayPin(), HIGH);
    rly->ticker_relay_tta->stop();
  }
}





//Schedule_timer relayttatimer(relayon,0,0,MILLIS_);
