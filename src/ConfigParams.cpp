#include <ConfigParams.h>

boolean ftimesynced = false;
boolean CalendarNotInitiated = true;

  bool webing = false;

TConfigParams MyConfParam;
TIRMap myIRMap;

String CID(){
  String res;
  #ifdef ESP32
    /*
    char buf[20];
    uint64_t chipid = ESP.getEfuseMac();
    ltoa(chipid,buf,20);
    String tmp = String(buf);
    return tmp;
    */
    uint64_t chipid = ESP.getEfuseMac();
    uint16_t chip = (uint16_t)(chipid >> 32);
    chip   = __builtin_bswap16(chip); 
    chipid = __builtin_bswap32((uint32_t)chipid); 
    char temp[23];
    snprintf(temp, 23, "%08X%04X", (uint32_t)chipid, chip );

    /*
    uint32_t chipId2 = 0;
    for(int i=0; i<17; i=i+8) {
      chipId2 |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
    }
    Serial.printf ("\n[Chip   ] >>>>>>>>>>> ESP32 Chip model = %s Rev %d\n", ESP.getChipModel(), ESP.getChipRevision());
    Serial.printf ("[Chip   ] >>>>>>>>>>> This chip has %d cores\n", ESP.getChipCores());
    Serial.print ("[Chip   ] >>>>>>>>>>> Chip ID: "); Serial.println(chipId2);
    char temp2[23];
    snprintf(temp2, 23, "%08X", chipId2 );
    Serial.print("[Chip   ] >>>>>>>>>>> Chip ID HEX: ");
    Serial.println(temp2);
    */
  return temp;
  #else
    res = String(ESP.getChipId());//  + softwareVersion;
    return res;
  #endif
}

void relayon(void* obj){
  if (obj != NULL) {
    Relay * rly = static_cast<Relay *>(obj);
    rly->mdigitalWrite(rly->getRelayPin(), HIGH);
    rly->ticker_relay_tta->stop();
  }
}



/*
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
*/

//Schedule_timer relayttatimer(relayon,0,0,MILLIS_);
