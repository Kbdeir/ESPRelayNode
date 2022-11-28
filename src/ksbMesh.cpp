#ifdef ESP_MESH
#include <Arduino.h>
#include <ksbMesh.h>
#include <defines.h>
#ifdef OLED_1306
  #include <Adafruit_GFX.h>
  #include <Adafruit_SSD1306.h>
  extern Adafruit_SSD1306 display;
  extern void DSS1306_println(String st);
#endif




Scheduler userScheduler;   // to control your personal task
painlessMesh  mesh;

bool calc_delay = false;
SimpleList<uint32_t> nodes;
uint32_t nsent=0;
char buff[512];
uint32_t nexttime=0;
uint8_t  initialized=0;
extern bool mesh_active;
uint8_t STATION_IP[4] = {192,168,30,1};

void sendMessage() ;
Task taskSendMessage( TASK_SECOND * 1 , TASK_FOREVER, &sendMessage );

void sendMessage() {
  String msg = "Hi from node: ";
  msg += mesh.getNodeId();
  mesh.sendBroadcast( msg );
  taskSendMessage.setInterval( random( TASK_SECOND * 1, TASK_SECOND * 5 ));
}


// messages received from painless mesh network
void receivedCallback( const uint32_t &from, const String &msg ) 
  {
  Serial.printf("bridge: Received from %u msg=%s\n", from, msg.c_str());
  String topic = PUBPLISHSUFFIX + String(from);
  // mqttClient.publish(topic.c_str(), msg.c_str());
  }


void newConnectionCallback(uint32_t nodeId) 
  {
  Serial.printf("--> Start: New Connection, nodeId = %u\n", nodeId);
  Serial.printf("--> Start: New Connection, %s\n", mesh.subConnectionJson(true).c_str());
  }



void changedConnectionCallback() 
  {
  Serial.printf("Changed connections\n");
  nodes = mesh.getNodeList();
  #ifdef OLED_1306
  display.print("Num nodes: ");
  display.println(String(nodes.size()));
  display.display();  
  #endif
  Serial.printf("Num nodes: %d\n", nodes.size());
  Serial.printf("Connection list:");
  SimpleList<uint32_t>::iterator node = nodes.begin();
  while (node != nodes.end()) 
    {
    Serial.printf(" %u", *node);
    node++;
    }
  Serial.println();
  calc_delay = true;

  sprintf(buff,"Nodes:%d",nodes.size());

  }


void nodeTimeAdjustedCallback(int32_t offset) 
  {
  Serial.printf("Adjusted time %u Offset = %d\n", mesh.getNodeTime(),offset);
  }


void onNodeDelayReceived(uint32_t nodeId, int32_t delay)
  {
  Serial.printf("Delay from node:%u delay = %d\n", nodeId,delay);
  }

IPAddress getlocalIP() 
  {
  return IPAddress(mesh.getStationIP());
  }


void setup_mesh() 
  {
#ifdef ESP_MESH
  //mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | MSG_TYPES | REMOTE ); // all types on except GENERAL
  //mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
  mesh.setDebugMsgTypes( ERROR | STARTUP | CONNECTION );  // set before init() so that you can see startup messages

  // Channel set to 1. Make sure to use the same channel for your mesh and for you other
  // network (STATION_SSID)
  // mesh.init ( MESH_PREFIX, MESH_PASSWORD, MESH_PORT, WIFI_AP_STA, WiFi.channel() );
  mesh.init( MESH_PREFIX, MESH_PASSWORD, &userScheduler,MESH_PORT, WIFI_AP_STA, 7 );
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
 
  userScheduler.addTask( taskSendMessage );
  taskSendMessage.enable();

  #ifdef ESP_MESH_ROOT
    Serial.println(F("[MESH   ] entering mesh root mode"));
    mesh.stationManual(STATION_SSID, STATION_PASSWORD); // ,5555,STATION_IP);
    mesh.setHostname(HOSTNAME);
    // Bridge node, should (in most cases) be a root node. See [the wiki](https://gitlab.com/painlessMesh/painlessMesh/wikis/Possible-challenges-in-mesh-formation) for some background
    mesh.setRoot(true);
  #endif
  // This node and all other nodes should ideally know the mesh contains a root, so call this on all nodes
  mesh.setContainsRoot(true);

  // if you want your node to accept OTA firmware, simply include this line
  // with whatever role you want your hardware to be. For instance, a
  // mesh network may have a thermometer, rain detector, and bridge. Each of
  // those may require different firmware, so different roles are preferrable.
  //
  // MAKE SURE YOUR UPLOADED OTA FIRMWARE INCLUDES OTA SUPPORT OR YOU WILL LOSE
  // THE ABILITY TO UPLOAD MORE FIRMWARE OVER OTA. YOU ALSO WANT TO MAKE SURE
  // THE ROLES ARE CORRECT
  // mesh.initOTAReceive("bridge");
  sprintf(buff,"Id:%d",mesh.getNodeId());
  mesh_active = true;

 #endif 

  }
#endif