#ifndef _MESH_H_
#define _MESH_H_
#ifdef ESP_MESH
    #include "painlessMesh.h"

    #define   MESH_PREFIX     "ksbMesh"
    #define   MESH_PASSWORD   "samsam12"
    #define   MESH_PORT       5555

    // WiFi credentials
    #define   STATION_SSID     "ksba"
    #define   STATION_PASSWORD "samsam12"
    #define   HOSTNAME         "ksbMeshBridge"

    #define   PUBPLISHSUFFIX             "painlessMesh/from/"
    #define   SUBSCRIBESUFFIX            "painlessMesh/to/"
    #define   PUBPLISHFROMGATEWAYSUFFIX  PUBPLISHSUFFIX "gateway"

void receivedCallback( const uint32_t &from, const String &msg ) ;
void newConnectionCallback(uint32_t nodeId);
void changedConnectionCallback() ;
void nodeTimeAdjustedCallback(int32_t offset) ;
void onNodeDelayReceived(uint32_t nodeId, int32_t delay);
IPAddress getlocalIP() ;
void setup_mesh() ;    
#endif
#endif