#include <KSBNTP.h>


byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets
static const char ntpServerName[] = "us.pool.ntp.org";
uint8_t timeZone = 1;     // Central European Time

unsigned int localPort = 8888;  // local port to listen for UDP packets
WiFiUDP Udp;


time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  if (WiFi.status() == WL_CONNECTED) {


  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println(F("[NTP ] Transmit NTP Request"));
  // get a random server from the pool
  // WiFi.hostByName(ntpServerName, ntpServerIP);
  //ntpServerIP.fromString(MyConfParam.v_timeserver);
  ntpServerIP=MyConfParam.v_timeserver;
  timeZone = MyConfParam.v_ntptz;

  Serial.print(ntpServerName);
  Serial.print(F(": "));
  Serial.print(F("timezone"));
  Serial.print(F(": "));
  Serial.print(String(timeZone));
  Serial.print(F(" - NTP server: "));
  Serial.println(ntpServerIP);
  ftimesynced = false;
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while ((millis() - beginWait < 3000) && !ftimesynced) {

    #ifdef ESP32
    #else
        ESP.wdtFeed();
    #endif    

    
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println(F("[NTP ] Received NTP Response"));
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      setSyncInterval(300); // got time, now sync ahgain every 300 seconds

      ftimesynced = true;

      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println(F("[NTP ] No NTP Response :-("));
  setSyncInterval(10); // if failed to get time, try after 10 seconds
  return 0; // return 0 if unable to get the time
  }
  return 0;
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
