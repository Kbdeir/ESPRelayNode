
#include <KSBNTP.h>
#include <ConfigParams.h>

byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets
static const char ntpServerName[] = "us.pool.ntp.org";
uint8_t timeZone = 1;     // Central European Time

unsigned int localPort = 8888;  // local port to listen for UDP packets
WiFiUDP Udp;

String digitalClockDisplay()
{
String t =  String(hour())+":"+printDigits(minute()) +":"+printDigits(second())+" "+String(day())+"-"+String(month())+"-"+String(year());
//Serial.print(" printing time ");
//Serial.println(t);
return t;

}

String printDigits(int digits)
{
 //  String t;
  // utility for digital clock display: prints preceding colon and leading 0
  return digits < 10 ? "0"  + String(digits) : String(digits);
  /*if (digits < 10) {
    t = t + "0";
  }
  t = t + String(digits);
return t;*/

}

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println(F("Transmit NTP Request"));
  // get a random server from the pool
  // WiFi.hostByName(ntpServerName, ntpServerIP);
  ntpServerIP.fromString(MyConfParam.v_timeserver);
  timeZone = MyConfParam.v_ntptz.toInt();

  Serial.print(ntpServerName);
  Serial.print(F(": "));
  Serial.print(F("timezone"));
  Serial.print(F(": "));
  Serial.print(String(timeZone));
    Serial.print(F(" - NTP server: "));
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 3000) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println(F("Receive NTP Response"));
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      setSyncInterval(300); // got time, now sync ahgain every 300 seconds
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println(F("No NTP Response :-("));
  setSyncInterval(10); // if failed to get time, try after 10 seconds
  return 0; // return 0 if unable to get the time
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
