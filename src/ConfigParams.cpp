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


  String IPAdrtoStr(IPAdr& IP_){
      char szRet[16];
      sprintf(szRet,"%u.%u.%u.%u", IP_.bytes[0],  IP_.bytes[1],  IP_.bytes[2],
        IP_.bytes[3]);
      return String(szRet);
  };

  void IPAdrfromStr(const char* strIP_, IPAdr& IP_){
    int a, b, c, d;
    //sscanf( json["FRM_IP"].as<String>().c_str(), "%hhu.%hhu.%hhu.%hhu", ConfParam.v_FRM_IP.bytes[1], ConfParam.v_FRM_IP.bytes[2], ConfParam.v_FRM_IP.bytes[3], ConfParam.v_FRM_IP.bytes[4] );
    sscanf( strIP_ , "%d.%d.%d.%d", &a, &b, &c, &d );
      IP_.bytes[0] = a;
      IP_.bytes[1] = b;
      IP_.bytes[2] = c;
      IP_.bytes[3] = d;
  };





//Schedule_timer relayttatimer(relayon,0,0,MILLIS_);
