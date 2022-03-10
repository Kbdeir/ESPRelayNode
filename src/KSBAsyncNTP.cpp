#include <KSBAsyncNTP.h>


// IPAddress timeWindowsCom = IPAddress(192,168, 20, 1);
// char timeServer[]         = "192.168.20.1";   // NTP server
// const int NTP_PACKET_SIZE_ = 48;           // NTP timestamp is in the first 48 bytes of the message
byte packetBuffer_[NTP_PACKET_SIZE_];         // buffer to hold incoming and outgoing packets

time_t epoch_t_;

// send an NTP request to the time server at the given address
void createNTPpacket(void)
{
   Serial.println(F("\n[NTP    ] Updating NTP time"));

  // set all bytes in the buffer to 0
  memset(packetBuffer_, 0, NTP_PACKET_SIZE_);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)

  packetBuffer_[0]   = 0b11100011;   // LI, Version, Mode
  packetBuffer_[1]   = 0;     // Stratum, or type of clock
  packetBuffer_[2]   = 6;     // Polling Interval
  packetBuffer_[3]   = 0xEC;  // Peer Clock Precision
  
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer_[12]  = 49;
  packetBuffer_[13]  = 0x4E;
  packetBuffer_[14]  = 49;
  packetBuffer_[15]  = 52;
}


void parsePacket(AsyncUDPPacket packet)
{
  struct tm  ts;
  char       buf[80];

    memcpy(packetBuffer_, packet.data(), sizeof(packetBuffer_));

    
    Serial.print("Received UDP Packet Type: ");
    Serial.println(packet.isBroadcast() ? "Broadcast" : packet.isMulticast() ? "Multicast" : "Unicast");
    Serial.print("From: ");
    Serial.print(packet.remoteIP());
    Serial.print(":");
    Serial.print(packet.remotePort());
    Serial.print(", To: ");
    Serial.print(packet.localIP());
    Serial.print(":");
    Serial.print(packet.localPort());
    Serial.print(", Length: ");
    Serial.print(packet.length());
    Serial.println();
    

    unsigned long highWord = word(packetBuffer_[40], packetBuffer_[41]);
    unsigned long lowWord = word(packetBuffer_[42], packetBuffer_[43]);

    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    
    //Serial.print(F("Seconds since Jan 1 1900 = "));
    //Serial.println(secsSince1900);

    // now convert NTP time into )everyday time:
    Serial.print(F("[NTP    ] Epoch/Unix time = "));
    
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    
    // subtract seventy years:
    // unsigned long epoch = secsSince1900 - seventyYears;
    // epoch_t_ = epoch + MyConfParam.v_ntptz * 3600;   //secsSince1900 - seventyYears;
    epoch_t_ = (secsSince1900 - seventyYears) + MyConfParam.v_ntptz * 3600;   //secsSince1900 - seventyYears;
    ftimesynced = true;
    
    // print Unix time:
    // Serial.println(epoch);

    // print the hour, minute and second:
    Serial.print(F("The UTC/GMT time is "));       // UTC is the time at Greenwich Meridian (GMT)

    ts = *localtime(&epoch_t_);
    strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S %Z", &ts);
    Serial.println(buf);

}



time_t timeprovider(void) {
    sendAsyncNTPPacket();
    if (ftimesynced) { 
        setSyncInterval(30);        
        return epoch_t_;

    } else {
        // setSyncInterval(10);
        return 0;
    }
}

void sendAsyncNTPPacket(void) {
  // ftimesynced = false;
  createNTPpacket();
  
  //Send unicast
  Audp.write(packetBuffer_, sizeof(packetBuffer_));
}








