/*
 * my_accessory.c
 * Define the accessory in C language using the Macro in characteristics.h
 *
 *  Created on: 2020-05-15
 *      Author: Mixiaoxiao (Wang Bin)
 */

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <defines.h>

extern char HAName_Bridge[HK_name_len];
extern char HAName_SW[HK_name_len];
extern char HAName_SW1[HK_name_len];
extern char HAName_SW2[HK_name_len];
extern char HAName_SW3[HK_name_len];

void my_accessory_identify(homekit_value_t _value) {
	printf("accessory identify\n");
}

// Switch (HAP section 8.38)
// required: ON
// optional: NAME

// format: bool; HAP section 9.70; write the .setter function to get the switch-event sent from iOS Home APP.
homekit_characteristic_t cha_switch_on = HOMEKIT_CHARACTERISTIC_(ON, false);
homekit_characteristic_t cha_name = HOMEKIT_CHARACTERISTIC_(NAME, HAName_SW);

#ifdef HWver03_4R
    // format: bool; HAP section 9.70; write the .setter function to get the switch-event sent from iOS Home APP.
    homekit_characteristic_t cha_switch_on1 = HOMEKIT_CHARACTERISTIC_(ON, false);
    homekit_characteristic_t cha_name1 = HOMEKIT_CHARACTERISTIC_(NAME, HAName_SW1);

    homekit_characteristic_t cha_switch_on2 = HOMEKIT_CHARACTERISTIC_(ON, false);
    homekit_characteristic_t cha_name2 = HOMEKIT_CHARACTERISTIC_(NAME, HAName_SW2);

    homekit_characteristic_t cha_switch_on3 = HOMEKIT_CHARACTERISTIC_(ON, false);
    homekit_characteristic_t cha_name3 = HOMEKIT_CHARACTERISTIC_(NAME, HAName_SW3);
#endif 

#if defined (HWver02)  || defined (HWver03)
// format: float; min 0, max 100, step 0.1, unit celsius
homekit_characteristic_t cha_temperature_name   = HOMEKIT_CHARACTERISTIC_(NAME, "Temperature");
homekit_characteristic_t cha_temperature        = HOMEKIT_CHARACTERISTIC_(CURRENT_TEMPERATURE, 1);
#endif 
/*
// single accessory relay mode
homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_switch, .services=(homekit_service_t*[]) {
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, HAName_SW), //"Switch"),
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "KSB HomeKit"),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "0123456"),
            HOMEKIT_CHARACTERISTIC(MODEL, "ESP8266/ESP32"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1.0"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
            NULL
        }),
		HOMEKIT_SERVICE(SWITCH, .primary=true, .characteristics=(homekit_characteristic_t*[]){
			&cha_switch_on,
			&cha_name,
			NULL
		}),
        NULL
    }),
    NULL
};
*/



homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_bridge, .services=(homekit_service_t*[]) {
    	// HAP section 8.17:
    	// For a bridge accessory, only the primary HAP accessory object must contain this(INFORMATION) service.
    	// But in my test,
    	// the bridged accessories must contain an INFORMATION service,
    	// otherwise the HomeKit will reject to pair.
    	HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, HAName_Bridge),
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "KSB HomeKit"),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "0123456"),
            HOMEKIT_CHARACTERISTIC(MODEL, "ESP8266/ESP32"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1.0"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
            NULL
        }),
        NULL
    }),

    HOMEKIT_ACCESSORY(.id=2, .category=homekit_accessory_category_switch, .services=(homekit_service_t*[]) {
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, HAName_SW),
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "KSB HomeKit"),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "0123456"),
            HOMEKIT_CHARACTERISTIC(MODEL, "ESP8266/ESP32"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1.0"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
            NULL
        }),
		HOMEKIT_SERVICE(SWITCH, .primary=true, .characteristics=(homekit_characteristic_t*[]){
			&cha_switch_on,
			&cha_name,
			NULL
		}),
        NULL
    }),    

    #if defined (HWver02)  || defined (HWver03)
        HOMEKIT_ACCESSORY(.id=3, .category=homekit_accessory_category_switch, .services=(homekit_service_t*[]) {
            HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
                HOMEKIT_CHARACTERISTIC(NAME, "Temperature"), //"Switch"),
                HOMEKIT_CHARACTERISTIC(MANUFACTURER, "KSB HomeKit"),
                HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "012345678"),
                HOMEKIT_CHARACTERISTIC(MODEL, "ESP8266/ESP32"),
                HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1.0"),
                HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
                NULL
            }),
            HOMEKIT_SERVICE(TEMPERATURE_SENSOR, .primary=true, .characteristics=(homekit_characteristic_t*[]){
                &cha_temperature,
                &cha_temperature_name,
                NULL
            }),
            NULL
        }),   
    #endif

    #ifdef HWver03_4R
    // ksb: you still have to implement the setter and updater functions
        HOMEKIT_ACCESSORY(.id=3, .category=homekit_accessory_category_switch, .services=(homekit_service_t*[]) {
            HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
                HOMEKIT_CHARACTERISTIC(NAME, HAName_SW1), //"Switch"),
                HOMEKIT_CHARACTERISTIC(MANUFACTURER, "KSB HomeKit"),
                HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "01234567"),
                HOMEKIT_CHARACTERISTIC(MODEL, "ESP8266/ESP32"),
                HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1.0"),
                HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
                NULL
            }),
            HOMEKIT_SERVICE(SWITCH, .primary=true, .characteristics=(homekit_characteristic_t*[]){
                &cha_switch_on1,
                &cha_name1,
                NULL
            }),
            NULL
        }),    

        HOMEKIT_ACCESSORY(.id=4, .category=homekit_accessory_category_switch, .services=(homekit_service_t*[]) {
            HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
                HOMEKIT_CHARACTERISTIC(NAME, HAName_SW2), //"Switch"),
                HOMEKIT_CHARACTERISTIC(MANUFACTURER, "KSB HomeKit"),
                HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "01234567"),
                HOMEKIT_CHARACTERISTIC(MODEL, "ESP8266/ESP32"),
                HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1.0"),
                HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
                NULL
            }),
            HOMEKIT_SERVICE(SWITCH, .primary=true, .characteristics=(homekit_characteristic_t*[]){
                &cha_switch_on2,
                &cha_name2,
                NULL
            }),
            NULL
        }),  

        HOMEKIT_ACCESSORY(.id=5, .category=homekit_accessory_category_switch, .services=(homekit_service_t*[]) {
            HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
                HOMEKIT_CHARACTERISTIC(NAME, HAName_SW3), //"Switch"),
                HOMEKIT_CHARACTERISTIC(MANUFACTURER, "KSB HomeKit"),
                HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "01234567"),
                HOMEKIT_CHARACTERISTIC(MODEL, "ESP8266/ESP32"),
                HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1.0"),
                HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
                NULL
            }),
            HOMEKIT_SERVICE(SWITCH, .primary=true, .characteristics=(homekit_characteristic_t*[]){
                &cha_switch_on3,
                &cha_name3,
                NULL
            }),
            NULL
        }),  

    #endif   

   NULL
};



homekit_server_config_t config = {
		.accessories = accessories,
		.password = "111-11-111",
		//.on_event = on_homekit_event, //optional
		.setupId = "ABCD" //optional        
};

