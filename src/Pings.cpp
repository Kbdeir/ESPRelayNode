#include "Pings.h"
int restartRequired_counter = 0;
bool restartRequired = false;  // Set this flag in the callbacks to restart ESP in the main loop
extern bool started_in_confMode;

#ifndef ESP32

AsyncPing Pings; 

void definePingsCallbacks() {

          Pings.on(true,[](const AsyncPingResponse& response){
            IPAddress addr(response.addr); //to prevent with no const toString() in 2.3.0
            if (response.answer)
              Serial.printf("\n[PING   ] %d bytes from %s: icmp_seq=%d ttl=%d time=%d ms", response.size, addr.toString().c_str(), response.icmp_seq, response.ttl, response.time);
            else
              Serial.printf("\n[PING   ] no answer yet for %s icmp_seq=%d\n", addr.toString().c_str(), response.icmp_seq);
            return false; //do not stop
          });
          Pings.on(false,[](const AsyncPingResponse& response){
            IPAddress addr(response.addr); //to prevent with no const toString() in 2.3.0
            Serial.printf("\n[PING   ] total answer from %s sent %d recevied %d time %d ms",addr.toString().c_str(),response.total_sent,response.total_recv,response.total_time);
            if (response.total_recv > 0) {
              restartRequired_counter = 0;
            }  else
            {
                  if (MyConfParam.v_Reboot_on_WIFI_Disconnection > 0) {
                    restartRequired_counter++;
                    //Serial.printf("\n\nPinging failure count: %i \n\n", restartRequired_counter);
                    Serial.print(F("\n[PING   ] Pinging failed count: "));
                    Serial.print(restartRequired_counter);
                    Serial.print("\n");
                    if (restartRequired_counter > MyConfParam.v_Reboot_on_WIFI_Disconnection)  {
                      restartRequired = true;
                    }     
                  }
            }      
            if (response.mac) {
              // Serial.printf("detected eth address " MACSTR "\n",MAC2STR(response.mac->addr));
            }
            return true;
          });
}

#endif

void servicePings(){
  if (MyConfParam.v_Reboot_on_WIFI_Disconnection > 0) {
      #ifndef ESP32
        Pings.begin(MyConfParam.v_Pingserver,1);
      #else
          bool success = Ping.ping(MyConfParam.v_Pingserver, 1);
          if(!success){
          Serial.println(F("\n[PING   ] Ping failed"));
                if (MyConfParam.v_Reboot_on_WIFI_Disconnection > 0) {
                  restartRequired_counter++;
                  //Serial.printf("\n\nPinging failure count: %i \n\n", restartRequired_counter);
                  Serial.print(F("[PING   ] Pinging failed count: "));
                  Serial.println(restartRequired_counter);
                  if (restartRequired_counter > MyConfParam.v_Reboot_on_WIFI_Disconnection)  {
                    restartRequired = true;
                  }     
                }        
          } 
          else {
          Serial.println(F("\n[PING   ] Ping successful"));
          restartRequired_counter = 0;
          } 
      #endif
  }
}
