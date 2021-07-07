/*
    ModbusIP.h - Header for Modbus IP Library
    Copyright (C) 2015 André Sarmento Barbosa
*/

#ifndef MODBUSIP_H
#define MODBUSIP_H

#include <Arduino.h>
#include <Modbus.h>
#ifdef ESP32
#include <AsyncTCP.h>
#else
#include <ESPAsyncTCP.h>
#endif



#define MODBUSIP_PORT 	  502
#define MODBUSIP_MAXFRAME 200

//#define TCP_KEEP_ALIVE

class ModbusIP : public Modbus {
    private:
        AsyncServer _server;
        byte _MBAP[7];

    public:
        ModbusIP();
        void config(uint8_t *mac);
        void config(uint8_t *mac, IPAddress ip);
        void config(uint8_t *mac, IPAddress ip, IPAddress dns);
        void config(uint8_t *mac, IPAddress ip, IPAddress dns, IPAddress gateway);
        void config(uint8_t *mac, IPAddress ip, IPAddress dns, IPAddress gateway, IPAddress subnet);
        uint8_t task(AsyncClient* _KSBClient,void *data, size_t len,boolean respond);
};

#endif //MODBUSIP_H
