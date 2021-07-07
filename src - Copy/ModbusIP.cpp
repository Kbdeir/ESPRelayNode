/*
    ModbusIP.cpp - Source for Modbus IP Library
    Copyright (C) 2015 André Sarmento Barbosa
*/
#include "ModbusIP.h"

ModbusIP::ModbusIP():_server(MODBUSIP_PORT) {
}


void ModbusIP::config(uint8_t *mac) {
//    Ethernet.begin(mac);
    _server.begin();
}

void ModbusIP::config(uint8_t *mac, IPAddress ip) {
//    Ethernet.begin(mac, ip);
    _server.begin();
}

void ModbusIP::config(uint8_t *mac, IPAddress ip, IPAddress dns) {
//    Ethernet.begin(mac, ip, dns);
    _server.begin();
}

void ModbusIP::config(uint8_t *mac, IPAddress ip, IPAddress dns, IPAddress gateway) {
//    Ethernet.begin(mac, ip, dns, gateway);
    _server.begin();
}

void ModbusIP::config(uint8_t *mac, IPAddress ip, IPAddress dns, IPAddress gateway, IPAddress subnet) {
//    Ethernet.begin(mac, ip, dns, gateway, subnet);
    _server.begin();
}

/*void ModbusIP::task(void *data, size_t len) {
    AsyncServer client = true; //_server available();

    if (true) { //(client) {
        if (true) { //(client.connected()) {
            int i = 0;
            while (true) { //client.available()){
                _MBAP[i] = client.read();
                i++;
                if (i==7) break;  //MBAP length has 7 bytes size
            }
            _len = _MBAP[4] << 8 | _MBAP[5];
            _len--;  // Do not count with last byte from MBAP

            if (_MBAP[2] !=0 || _MBAP[3] !=0) return;   //Not a MODBUSIP packet
            if (_len > MODBUSIP_MAXFRAME) return;      //Length is over MODBUSIP_MAXFRAME

            _frame = (byte*) malloc(_len);
            i = 0;
            while (client.available()){
                _frame[i] = client.read();
                i++;
                if (i==_len) break;
            }

            this->receivePDU(_frame);
            if (_reply != MB_REPLY_OFF) {
                //MBAP
                _MBAP[4] = (_len+1) >> 8;     //_len+1 for last byte from MBAP
                _MBAP[5] = (_len+1) & 0x00FF;

                byte sendbuffer[7 + _len];

                for (i = 0 ; i < 7 ; i++) {
                    sendbuffer[i] = _MBAP[i];
                }
                //PDU Frame
                for (i = 0 ; i < _len ; i++) {
                    sendbuffer[i+7] = _frame[i];
                }
                client.write(sendbuffer, _len + 7);
            }

#ifndef TCP_KEEP_ALIVE
            client.stop();
#endif
            free(_frame);
            _len = 0;
        }
    }
}
*/

/*
void ModbusIP::task() {
    uint8_t buffer[128] = {0};
    uint8_t mux_id;

	if (prev_conn) {
		_wifi->stopTCPServer();
		_wifi->enableMUX();
		_wifi->startTCPServer(MODBUSIP_PORT);
		_wifi->setTCPServerTimeout(10);
		prev_conn = false;
	}
    uint32_t raw_len = _wifi->recv(&mux_id, buffer, sizeof(buffer), MODBUSIP_TIMEOUT);

    if (raw_len > 7) {

        for (int i = 0; i < 7; i++)	_MBAP[i] = buffer[i]; //Get MBAP

        _len = _MBAP[4] << 8 | _MBAP[5];
        _len--; // Do not count with last byte from MBAP
        if (_MBAP[2] !=0 || _MBAP[3] !=0) return;  //Not a MODBUSIP packet
        if (_len > MODBUSIP_MAXFRAME) return;      //Length is over MODBUSIP_MAXFRAME

        _frame = (byte*) malloc(_len);

        for (int i=0; i < _len; i++) _frame[i] = buffer[7+i]; //Get Modbus PDU

        this->receivePDU(_frame);

        if (_reply != MB_REPLY_OFF) {
            //MBAP
            _MBAP[4] = (_len+1) >> 8;     //_len+1 for last byte from MBAP
            _MBAP[5] = (_len+1) & 0x00FF;

            buffer[4] = _MBAP[4];
            buffer[5] = _MBAP[5];

            for (int i=0; i<_len; i++)	buffer[i+7] = _frame[i];

            _wifi->send(mux_id, buffer, _len + 7);
        }

		prev_conn = true;

        free(_frame);
        _len = 0;
    }
}
*/



uint8_t ModbusIP::task(AsyncClient* _KSBClient,void *data, size_t len, boolean respond) {
 //Serial.write((uint8_t*)data, len);

 uint8_t buffer[128] = {0};
 memcpy(buffer,data,len);
 //uint8_t mux_id;

 //_KSBClient->add((char*)buffer, len );
 //_KSBClient->send();
 //exit;

 //uint32_t raw_len = _wifi->recv(&mux_id, buffer, sizeof(buffer), MODBUSIP_TIMEOUT);

 uint32_t raw_len  = len;

 if (raw_len > 7) {

     for (int i = 0; i < 7; i++)	_MBAP[i] = buffer[i]; //Get MBAP

     _len = _MBAP[4] << 8 | _MBAP[5];
     _len--; // Do not count with last byte from MBAP
     if (_MBAP[2] !=0 || _MBAP[3] !=0) return 0;  //Not a MODBUSIP packet
     if (_len > MODBUSIP_MAXFRAME) return 0;      //Length is over MODBUSIP_MAXFRAME

     _frame = (byte*) malloc(_len);

     for (int i=0; i < _len; i++) _frame[i] = buffer[7+i]; //Get Modbus PDU

     uint8_t res = this->receivePDU(_frame);

     if (_reply != MB_REPLY_OFF) {
         //MBAP
         _MBAP[4] = (_len+1) >> 8;     //_len+1 for last byte from MBAP
         _MBAP[5] = (_len+1) & 0x00FF;

         buffer[4] = _MBAP[4];
         buffer[5] = _MBAP[5];

         for (int i=0; i<_len; i++)	buffer[i+7] = _frame[i];

        // _wifi->send(mux_id, buffer, _len + 7);
        if (respond) {
        if (_KSBClient->canSend()) {
          _KSBClient->add((char*)buffer,  _len + 7);
       	 _KSBClient->send();
       }
        }
     }

 //prev_conn = true;
    return res;
     free(_frame);
     _len = 0;
 }

}
