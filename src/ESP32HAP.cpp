#ifdef ESP32
#include <Arduino.h>
#ifdef ESP32
#include <Wifi.h>
#else
#include <ESP8266WiFi.h>
#endif

#include <ESP32HomeKit.h>
#include <ESP32HAP.h>
#include <RelayClass.h>
#include <ConfigParams.h>


    hap_char_t *hcx;
    hap_char_t *hcx_temp;    

/* Mandatory identify routine for the accessory.
 * In a real accessory, something like LED blink should be implemented
 * got visual identification
 */
#define NUM_BRIDGED_ACCESSORIES 3     // ****************  This is important ! set it to the correct number of accessories, otherwise system will not be reliable !!!!
static const char* kSetupID = "GRGR"; // This must be unique
static int identify(hap_acc_t *ha)
{
	ESP_LOGI(TAG, "Accessory identified");
	return HAP_SUCCESS;
}



//Called when the switch value is changed by iOS Home APP
void switchSetter(int value) {
	bool on = (value == 1);
    Relay * rtemp = nullptr;
    rtemp = getrelaybypin(RelayPin);
    if (rtemp) {
        if (on) {
        rtemp->mdigitalWrite(RelayPin,HIGH);
        } else {
            rtemp->mdigitalWrite(RelayPin,LOW);
        }
    }
 }



/* A dummy callback for handling a read on the "Direction" characteristic of Fan.
 * In an actual accessory, this should read from hardware.
 * Read routines are generally not required as the value is available with th HAP core
 * when it is updated from write routines. For external triggers (like fan switched on/off
 * using physical button), accessories should explicitly call hap_char_update_val()
 * instead of waiting for a read request.
 */
static int fan_read(hap_char_t *hc, hap_status_t *status_code, void *serv_priv, void *read_priv)
{
	if (hap_req_get_ctrl_id(read_priv))
	{
		Serial.printf("Received read from %s \n", hap_req_get_ctrl_id(read_priv));
	}
	if (!strcmp(hap_char_get_type_uuid(hc), HAP_CHAR_UUID_ROTATION_DIRECTION))
	{
		/* Read the current value, toggle it and set the new value.
        * A separate variable should be used for the new value, as the hap_char_get_val()
        * API returns a const pointer
        */
		const hap_val_t *cur_val = hap_char_get_val(hc);

		hap_val_t new_val;
		if (cur_val->i == 1)
		{
			new_val.i = 0;
		}
		else
		{
			new_val.i = 1;
		}
		hap_char_update_val(hc, &new_val);
		*status_code = HAP_STATUS_SUCCESS;
	}
	return HAP_SUCCESS;
}

/* A dummy callback for handling a write on the "On" characteristic of Fan.
 * In an actual accessory, this should control the hardware
 */
static int fan_write(hap_write_data_t write_data[], int count, void *serv_priv, void *write_priv)
{

	Serial.printf("Received write from %s \n", hap_req_get_ctrl_id(write_priv));
	if (hap_req_get_ctrl_id(write_priv))
	{
		ESP_LOGI(TAG, "Received write from %s", hap_req_get_ctrl_id(write_priv));
	}

	Serial.printf( "Fan Write called with %d chars \n", count);
	int i, ret = HAP_SUCCESS;
	hap_write_data_t *write;
	for (i = 0; i < count; i++)
	{
		write = &write_data[i];
		if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_ON))
		{
			//ESP_LOGI(TAG, "Received Write. Fan %s", write->val.b ? "On" : "Off");
			Serial.printf( "Received Write. Fan %s \n", write->val.b ? "On" : "Off");

			/* TODO: Control Actual Hardware */
            write->val.b ? switchSetter(1) : switchSetter(0);
			hap_char_update_val(write->hc, &(write->val));
			*(write->status) = HAP_STATUS_SUCCESS;
		}
		else if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_ROTATION_DIRECTION))
		{
			if (write->val.i > 1)
			{
				*(write->status) = HAP_STATUS_VAL_INVALID;
				ret = HAP_FAIL;
			}
			else
			{
				Serial.printf("Received Write. Fan %s \n", write->val.i ? "AntiClockwise" : "Clockwise");
				hap_char_update_val(write->hc, &(write->val));
				*(write->status) = HAP_STATUS_SUCCESS;
			}
		}
		else
		{
			*(write->status) = HAP_STATUS_RES_ABSENT;
		}
	}
	return ret;
}


static int temp_write(hap_write_data_t write_data[], int count, void *serv_priv, void *write_priv)
{

	int i, ret = HAP_SUCCESS;
	hap_write_data_t *write;
	for (i = 0; i < count; i++)
	{
		write = &write_data[i];
		if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_ON))
		{
			//ESP_LOGI(TAG, "Received Write. Fan %s", write->val.b ? "On" : "Off");
			Serial.printf( "Received Write. Fan %s \n", write->val.b ? "On" : "Off");

			/* TODO: Control Actual Hardware */
            write->val.b ? switchSetter(1) : switchSetter(0);
			hap_char_update_val(write->hc, &(write->val));
			*(write->status) = HAP_STATUS_SUCCESS;
		}
		else if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_ROTATION_DIRECTION))
		{
			if (write->val.i > 1)
			{
				*(write->status) = HAP_STATUS_VAL_INVALID;
				ret = HAP_FAIL;
			}
			else
			{
				Serial.printf("Received Write. Fan %s \n", write->val.i ? "AntiClockwise" : "Clockwise");
				hap_char_update_val(write->hc, &(write->val));
				*(write->status) = HAP_STATUS_SUCCESS;
			}
		}
		else
		{
			*(write->status) = HAP_STATUS_RES_ABSENT;
		}
	}
	return ret;
}


void resetHap()
{
	hap_reset_to_factory();
	hap_reset_homekit_data();
	hap_reset_pairings();
}


void setupHAP()
{
    hap_acc_t *accessory;
    hap_serv_t *service;

    hap_acc_t *accessoryt;
    hap_serv_t *servicet;

    /* Initialize the HAP core */
    hap_init(HAP_TRANSPORT_WIFI);

    /* Initialise the mandatory parameters for Accessory which will be added as
     * the mandatory services internally
     */
    hap_acc_cfg_t cfg = {
        .name = "Esp-Bridge",
        .model = "EspBridge01",
        .manufacturer = "Espressif",		
        .serial_num = "001122334455",
        .fw_rev = "0.9.0",
        .hw_rev = NULL,
        .pv = "1.1.0",
        .cid = HAP_CID_BRIDGE,		
        .identify_routine = identify,
	};
    /* Create accessory object */
    accessory = hap_acc_create(&cfg);

    /* Add a dummy Product Data */
    uint8_t product_data[] = {'E','S','P','3','2','H','A','P'};
    hap_acc_add_product_data(accessory, product_data, sizeof(product_data));

    /* Add the Accessory to the HomeKit Database */
    hap_add_accessory(accessory);



#ifdef A1
    for (uint8_t i = 0; i < NUM_BRIDGED_ACCESSORIES; i++) {
        char accessory_name[12] = {0};
        sprintf(accessory_name, "ESP-Fan-%d", i);

        hap_acc_cfg_t bridge_cfg = {
            .name = accessory_name,
            .model = "EspFan01",			
            .manufacturer = "Espressif",
            .serial_num = "abcdefg",
            .fw_rev = "0.9.0",
            .hw_rev = NULL,
            .pv = "1.1.0",
            .cid = HAP_CID_BRIDGE,			
            .identify_routine = identify,

        };
        /* Create accessory object */
        accessory = hap_acc_create(&bridge_cfg);

        /* Create the Fan Service. Include the "name" since this is a user visible service  */
        service = hap_serv_fan_create(false);
        hap_serv_add_char(service, hap_char_name_create(accessory_name));

        /* Set the Accessory name as the Private data for the service,
         * so that the correct accessory can be identified in the
         * write callback
         */
        hap_serv_set_priv(service, strdup(accessory_name));

        /* Set the write callback for the service */
        hap_serv_set_write_cb(service, fan_write);
 
        /* Add the Fan Service to the Accessory Object */
        hap_acc_add_serv(accessory, service);

        /* Add the Accessory to the HomeKit Database */
        hap_add_bridged_accessory(accessory, hap_get_unique_aid(accessory_name));
    }
#endif

/*

		static String accessory_name1 = "ESP-SW1";
        hap_acc_cfg_t bridge_cfg1 = {
            .name = "ESP_SW1",
            .model = "EspFan01",			
            .manufacturer = "Espressif",
            .serial_num = "abcdefg",
            .fw_rev = "0.9.0",
            .hw_rev = NULL,
            .pv = "1.1.0",
            .cid = HAP_CID_BRIDGE,			
            .identify_routine = identify,

        };
        // Create accessory object 
        accessory = hap_acc_create(&bridge_cfg1);

        // Create the Fan Service. Include the "name" since this is a user visible service  
        service = hap_serv_fan_create(false);
        hap_serv_add_char(service, hap_char_name_create("ESP_SW1"));

        // Set the Accessory name as the Private data for the service,
        // so that the correct accessory can be identified in the
        // write callback
        //
        hap_serv_set_priv(service, strdup(accessory_name1.c_str()));

        // Set the write callback for the service 
        hap_serv_set_write_cb(service, fan_write);
 
        // Add the Fan Service to the Accessory Object 
        hap_acc_add_serv(accessory, service);

        // Add the Accessory to the HomeKit Database 
        hap_add_bridged_accessory(accessory, hap_get_unique_aid(accessory_name1.c_str()));

*/
#define SW
#ifdef SW
		static String accessory_name = "ESP-SW";
        hap_acc_cfg_t bridge_cfg = {
            .name = "Switch",
            .model = "Switch1",			
            .manufacturer = "Espressif",
            .serial_num = "abcdefg",
            .fw_rev = "0.9.0",
            .hw_rev = NULL,
            .pv = "1.1.0",
            .cid = HAP_CID_BRIDGE,			
            .identify_routine = identify,

        };
        // Create accessory object 
        accessory = hap_acc_create(&bridge_cfg);

        // Create the Fan Service. Include the "name" since this is a user visible service  , or hap_serv_temperature_sensor_create
        service = hap_serv_switch_create(false);
        hap_serv_add_char(service, hap_char_name_create("Switch"));

        // Set the Accessory name as the Private data for the service,
        // so that the correct accessory can be identified in the
        // write callback
        //
        hap_serv_set_priv(service, strdup(accessory_name.c_str()));

        // Set the write callback for the service 
        hap_serv_set_write_cb(service, fan_write);
 
        // Add the Fan Service to the Accessory Object 
        hap_acc_add_serv(accessory, service);

        // Add the Accessory to the HomeKit Database 
        hap_add_bridged_accessory(accessory, hap_get_unique_aid(accessory_name.c_str()));

        hcx = hap_serv_get_char_by_uuid(service, HAP_CHAR_UUID_ON);
#endif        

#define TEMPSNSR_
#ifdef TEMPSNSR_
		static String accessory_name1 = "ESP-Temp";
        hap_acc_cfg_t bridge_cfg1 = {
            .name = "TempSensor",
            .model = "TempSensor",			
            .manufacturer = "Espressif",
            .serial_num = "abcdefg",
            .fw_rev = "0.9.0",
            .hw_rev = NULL,
            .pv = "1.1.0",
            .cid = HAP_CID_BRIDGE,			
            .identify_routine = identify,

        };
        // Create accessory object 
        accessoryt = hap_acc_create(&bridge_cfg1);

        // Create the Fan Service. Include the "name" since this is a user visible service  , or hap_serv_temperature_sensor_create
        servicet = hap_serv_temperature_sensor_create(10);
        hap_serv_add_char(servicet, hap_char_name_create("TempSensor"));

        // Set the Accessory name as the Private data for the service,
        // so that the correct accessory can be identified in the
        // write callback
        //
        hap_serv_set_priv(servicet, strdup(accessory_name1.c_str()));

        // Set the write callback for the service 
         hap_serv_set_write_cb(servicet, fan_write);
 
        // Add the Fan Service to the Accessory Object 
         hap_acc_add_serv(accessoryt, servicet);

        // Add the Accessory to the HomeKit Database 
        hap_add_bridged_accessory(accessoryt, hap_get_unique_aid(accessory_name1.c_str()));

        hcx_temp = hap_serv_get_char_by_uuid(servicet, HAP_CHAR_UUID_CURRENT_TEMPERATURE); 
        // Serial.println () hap_serv_get_uuid(servicet));
#endif                


	/* Query the controller count (just for information) */
	ESP_LOGI(TAG, "Accessory is paired with %d controllers",
			 hap_get_paired_controller_count());

	/* TODO: Do the actual hardware initialization here */

	/* Unique Setup code of the format xxx-xx-xxx. Default: 111-22-333 */
	hap_set_setup_code("123-45-678");
	/* Unique four character Setup Id. Default: ES32 */
	hap_set_setup_id("ES32");

	/* After all the initializations are done, start the HAP core */
	hap_start();
    
    /*
    hap_val_t new_val;
    Relay * rtemp = nullptr;
    rtemp = getrelaybypin(RelayPin);
    if (rtemp) {
      new_val.i = digitalRead(rtemp->getRelayPin()) == HIGH ? 1 : 0;
      hap_char_update_val(hcx, &new_val);
    }
    */


}
#endif
