#include <KSBWiFiHelper.h>
#include <TimerClass.h>

extern void thingsTODO_on_WIFI_Connected();
extern Schedule_timer Wifireconnecttimer;
extern void startsoftAP();
extern long blinkInterval; // blinkInterval at which to blink (milliseconds)
extern unsigned long lastMillis_2;
extern unsigned long previousMillis;
extern int ledState;             					// ledState used to set the LED

#ifdef ESP32
void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info)
{
    Serial.println("[WIFI   ] Event: WiFi connected");
    Serial.print("[WIFI   ] IP address: ");
    Serial.println(IPAddress(info.got_ip.ip_info.ip.addr));
    thingsTODO_on_WIFI_Connected();
    blinkInterval = 1000;
    Wifireconnecttimer.stop();
    lastMillis_2 = 0;
}
void WiFiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
    Serial.println("[WIFI   ] * Event: WiFi disconnected * ");
    if (blinkInterval > 50)
    {
        Wifireconnecttimer.start();
        Serial.println(F("[WIFI   ] Starting reconnection timer\n"));
    }
    blinkInterval = 50;
    if (digitalRead(ConfigInputPin) == LOW)
    {
        Wifireconnecttimer.stop();
        Serial.println(F("[WIFI   ] Starting AP_STA mode\n"));
        WiFi.mode(WIFI_AP_STA);
        if ((WiFi.status() != WL_CONNECTED))
        {
            startsoftAP();
        }
    }
}
#endif

#ifndef ESP32
    WiFiEventHandler gotIpEventHandler = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP &event)
    {
        Serial.print(F("\n[WIFI   ] Station connected, IP: "));
        Serial.println(WiFi.localIP());
        thingsTODO_on_WIFI_Connected();
        blinkInterval = 1000;
        Wifireconnecttimer.stop();
        lastMillis_2 = 0; 
    });
    WiFiEventHandler disconnectedEventHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected &event)
    {
        Serial.println(F("\n[WIFI   ] Station disconnected\n"));
            if (blinkInterval > 50) {
            Wifireconnecttimer.start();
            Serial.println(F("[WIFI   ] Starting reconnection timer\n"));
        }
        blinkInterval = 50;
        if (digitalRead(ConfigInputPin) == LOW){
            Wifireconnecttimer.stop();
            Serial.println(F("[WIFI   ] Starting AP_STA mode\n"));
            WiFi.mode(WIFI_AP_STA);
            if ((WiFi.status() != WL_CONNECTED))	{
                startsoftAP();
            }
        } 
    });
#endif

void blinkled(){
	unsigned long currentMillis = millis();
	if (currentMillis - previousMillis >= blinkInterval) {
		previousMillis = currentMillis;
		if (ledState == LOW) {
			ledState = HIGH;
		} else {
			ledState = LOW;
		}
		digitalWrite(led, ledState);
	}
}

void blinkledtimed(void* obj){
		if (ledState == LOW) {
			ledState = HIGH;
		} else {
			ledState = LOW;
		}
		digitalWrite(led, ledState);
      #ifdef HWver03_4R
    		// digitalWrite(led2, ledState);
      #endif  
}



/*
void WiFiEvent(WiFiEvent_t event)
{
    Serial.printf("[WiFi-event] event: %d\n", event);

    switch (event) {
        case ARDUINO_EVENT_WIFI_READY: 
            Serial.println("WiFi interface ready");
            break;
        case ARDUINO_EVENT_WIFI_SCAN_DONE:
            Serial.println("Completed scan for access points");
            break;
        case ARDUINO_EVENT_WIFI_STA_START:
            Serial.println("WiFi client started");
            break;
        case ARDUINO_EVENT_WIFI_STA_STOP:
            Serial.println("WiFi clients stopped");
            break;
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Serial.println("Connected to access point");
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.println("Disconnected from WiFi access point");
            break;
        case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
            Serial.println("Authentication mode of access point has changed");
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.print("Obtained IP address: ");
            Serial.println(WiFi.localIP());
            break;
        case ARDUINO_EVENT_WIFI_STA_LOST_IP:
            Serial.println("Lost IP address and IP address is reset to 0");
            break;
        case ARDUINO_EVENT_WPS_ER_SUCCESS:
            Serial.println("WiFi Protected Setup (WPS): succeeded in enrollee mode");
            break;
        case ARDUINO_EVENT_WPS_ER_FAILED:
            Serial.println("WiFi Protected Setup (WPS): failed in enrollee mode");
            break;
        case ARDUINO_EVENT_WPS_ER_TIMEOUT:
            Serial.println("WiFi Protected Setup (WPS): timeout in enrollee mode");
            break;
        case ARDUINO_EVENT_WPS_ER_PIN:
            Serial.println("WiFi Protected Setup (WPS): pin code in enrollee mode");
            break;
        case ARDUINO_EVENT_WIFI_AP_START:
            Serial.println("WiFi access point started");
            break;
        case ARDUINO_EVENT_WIFI_AP_STOP:
            Serial.println("WiFi access point  stopped");
            break;
        case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
            Serial.println("Client connected");
            break;
        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
            Serial.println("Client disconnected");
            break;
        case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
            Serial.println("Assigned IP address to client");
            break;
        case ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED:
            Serial.println("Received probe request");
            break;
        case ARDUINO_EVENT_WIFI_AP_GOT_IP6:
            Serial.println("AP IPv6 is preferred");
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
            Serial.println("STA IPv6 is preferred");
            break;
        case ARDUINO_EVENT_ETH_GOT_IP6:
            Serial.println("Ethernet IPv6 is preferred");
            break;
        case ARDUINO_EVENT_ETH_START:
            Serial.println("Ethernet started");
            break;
        case ARDUINO_EVENT_ETH_STOP:
            Serial.println("Ethernet stopped");
            break;
        case ARDUINO_EVENT_ETH_CONNECTED:
            Serial.println("Ethernet connected");
            break;
        case ARDUINO_EVENT_ETH_DISCONNECTED:
            Serial.println("Ethernet disconnected");
            break;
        case ARDUINO_EVENT_ETH_GOT_IP:
            Serial.println("Obtained IP address");
            break;
        default: break;
    }}


void setup()
{
    Serial.begin(115200);

    // delete old config
    WiFi.disconnect(true);

    delay(1000);

    // Examples of different ways to register wifi events
    WiFi.onEvent(WiFiEvent);
    WiFi.onEvent(WiFiGotIP, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFiEventId_t eventID = WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
        Serial.print("WiFi lost connection. Reason: ");
        Serial.println(info.wifi_sta_disconnected.reason);
    }, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    // Remove WiFi event
    Serial.print("WiFi Event ID: ");
    Serial.println(eventID);
    // WiFi.removeEvent(eventID);

    WiFi.begin(ssid, password);

    Serial.println();
    Serial.println();
    Serial.println("Wait for WiFi... ");
}


 */   