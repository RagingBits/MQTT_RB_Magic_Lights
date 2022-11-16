

/* Main Device Unique Information ---------------------------------------- */

/* Device serial number. This is associated to every MQTT Topic and to Hotspot SSID name. 
   Several units may have the same Serial number in the same network, such they all receive the 
   same Topic data and respond with the same Topics too. 
   The length of the number MUST be respected. 10 digits EXACTLY. 
   */
   
/* 
  FW  v2.1 - Base project ready for work.
      v2.2 - Added support for intensity control.
      v2.3 - Added option for Device ID (serial number) setting on setup Webpage.
             Also changed the serial port setup. Both support any chars up to 10 chars.

  HW  v1   - Base project ready to work.  
 */


/* FIXED definitions. Do not change!!! ------------------------------------*/

String userID_   = "rb_wifi_mqtt_lights_0000000000";

/* Main Device HW and SW Information ------------------------------------- */
#define HW_V_1  1
#define HW_V_2  2
#define HW_V_3  3

/* Current Hardware version defines pinouts. */
#define HARDWARE_VERSION  HW_V_1
#define FIRMWARE_VERSION  FW_V_2.3 
