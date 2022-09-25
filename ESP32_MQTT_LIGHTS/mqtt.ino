
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <Ticker.h>
#include <WiFi.h>
#include <ArduinoMqttClient.h>

#define MQTT_CLIENT_IP_LEN  16U
#define MQTT_CLIENT_THREAD_RATE           50/*milliseconds*/
#define MQTT_CLIENT_RECONNECTION_TIMEOUT  (3000/MQTT_CLIENT_THREAD_RATE) /* 5Seconds */

typedef struct
{
    uint32_t reconnecting:1U;
    uint32_t initialised:1U;
    uint32_t work_run:1U;
    uint32_t file_transfer_index:16U;
    uint32_t unused:13U;
}mqtt_client_flags_t;


/* MQTT Global Constants ------------------------------------------------------ */
const String COMMAND_RGB_TOPIC_ PROGMEM = "_rgb_set";
const String COMMAND_EFFECT_SEL_TOPIC_ PROGMEM = "_effect_selection";
const String COMMAND_FILE_TRANSFER_TOPIC_ PROGMEM = "_file_transfer";
const String COMMAND_FILE_TRANSFER_REP_TOPIC_ PROGMEM = "_file_transfer_reply";


/* MQTT Global Variables ------------------------------------------------------ */
mqtt_client_flags_t mqtt_client_flags = {.reconnecting = !0U};
uint32_t mqtt_client_reconnection_timeout = 0U;

String COMMAND_RGB_TOPIC = "";
String COMMAND_EFFECT_SEL_TOPIC = "";
String COMMAND_FILE_TRANSFER_TOPIC = "";
String COMMAND_FILE_TRANSFER_REP_TOPIC = "";

uint32_t mqtt_transfered_file_length = 0;

/* MQTT Clients --------------------------------------------------------------- */
WiFiClient mqtt_wifi_client;
MqttClient mqtt_client(&mqtt_wifi_client);
Ticker mqtt_client_thread;


/* MQTT External Functions ---------------------------------------------------- */
void MQTTConnect(void)
{
    uint8_t counter = 0;
    
    if(0U == mqtt_client_flags.initialised)
    {
        Serial.print("MQTT Thread starting... ");
        mqtt_client_thread.attach_ms(MQTT_CLIENT_THREAD_RATE, &MQTTRunWork);
        mqtt_client_flags.initialised = !0U;
        mqtt_client_flags.reconnecting = !0U;

        COMMAND_RGB_TOPIC  = userID_ + COMMAND_RGB_TOPIC_;
        COMMAND_EFFECT_SEL_TOPIC  = userID_ + COMMAND_EFFECT_SEL_TOPIC_;
        COMMAND_FILE_TRANSFER_TOPIC  = userID_ + COMMAND_FILE_TRANSFER_TOPIC_;
        COMMAND_FILE_TRANSFER_REP_TOPIC = userID_ + COMMAND_FILE_TRANSFER_REP_TOPIC_;
        
        Serial.println("Done.");
    }

    Serial.println("MQTT New Connection request. "); 
    if(0U == mqtt_client_flags.reconnecting)
    {
        Serial.println("MQTT Disconnect from Broker. ");
        mqtt_client.stop();
        mqtt_client_flags.reconnecting = 1U;
    }
}


bool MQTTIsConnected(void)
{
    return (0U == mqtt_client_flags.reconnecting);
}


/* MQTT Internal Functions ---------------------------------------------------- */

void MQTTRunWork(void)
{
  mqtt_client_flags.work_run = !0U;  
}

void MQTTPerformSubscriptions(void)
{
    Serial.println(" ");
    Serial.println("MQTT Subscribing: ");
    Serial.print("    Subscribe: ");Serial.println(COMMAND_RGB_TOPIC);
    mqtt_client.subscribe(COMMAND_RGB_TOPIC);
    Serial.print("    Subscribe: ");Serial.println(COMMAND_EFFECT_SEL_TOPIC);
    mqtt_client.subscribe(COMMAND_EFFECT_SEL_TOPIC);
    Serial.print("    MQTT Subscribe: ");Serial.println(COMMAND_FILE_TRANSFER_TOPIC);
    mqtt_client.subscribe(COMMAND_FILE_TRANSFER_TOPIC);

}


void MQTTWork(void)
{
    if(0U != mqtt_client_flags.initialised && mqtt_client_flags.work_run != 0U)
    {
        mqtt_client_flags.work_run = 0U;
        if(0U == mqtt_client_flags.reconnecting)
        {
            if(mqtt_client.connected())
            {
                mqtt_client.poll();
            }
            else
            {
                if(0U == mqtt_client_flags.reconnecting)
                {
                    mqtt_client_flags.reconnecting = !0U;
                    MQTTReConnect();
                }
            }
        }
        else
        {
            MQTTReConnect();
        }
    }
   
}


void MQTTReConnect(void)
{
    if(0U != mqtt_client_flags.reconnecting)
    {
        if(0U == mqtt_client_reconnection_timeout)
        {
            if(0U != mqtt_client.connected())
            {
                Serial.println(" ");
                Serial.println("    MQTT Connected. ");
                MQTTPerformSubscriptions();
                mqtt_client_flags.reconnecting = 0U;
                mqtt_client_reconnection_timeout = 0U;
            }
            else
            {
                Serial.println("MQTT Connecting:");

                uint8_t temp_len = MQTT_BROKER_PORT_MAX_LEN;
                char temp_data[MQTT_BROKER_PORT_MAX_LEN+MQTT_BROKER_IP_MAX_LEN] = {0U};
                EepromRead(EEPROM_MQTT_BROKER_PORT, (uint8_t*)temp_data, &temp_len);
                uint16_t mqtt_broker_port_int = atoi(temp_data);

                temp_len = MQTT_BROKER_IP_MAX_LEN;
                EepromRead(EEPROM_MQTT_BROKER_IP, (uint8_t*)temp_data, &temp_len);

                Serial.print("    Broker IP: "); Serial.println(temp_data);
                Serial.print("    Broker port: "); Serial.println(mqtt_broker_port_int);
                Serial.print("    ");
                
                mqtt_client.connect(temp_data, mqtt_broker_port_int);
                
                mqtt_client.onMessage(MQTTHandle);
                mqtt_client_reconnection_timeout = MQTT_CLIENT_RECONNECTION_TIMEOUT;
            }
        }
        else
        {
            Serial.print(".");
            mqtt_client_reconnection_timeout--;
        }
       
    }
}


void MQTTHandle(int message_len)
{
    //Serial.println(" ");
    //Serial.print("MQTT new message: ");
    String received_topic = mqtt_client.messageTopic();
    
    
    char message_data[2100]={0};
    uint16_t message_data_len = 0;
    while (mqtt_client.available() && message_data_len<2100 && message_data_len<message_len) 
    {
      message_data[message_data_len++] = (char)mqtt_client.read();
    }
    //Serial.println((char*)message_data);

    if(COMMAND_RGB_TOPIC == received_topic)
    {
        Serial.println("RGB Color.");
        MQTTHandleRGBMessage(message_data,message_data_len);
    }
    else
    if(COMMAND_EFFECT_SEL_TOPIC == received_topic)
    {
        Serial.println("Work Mode.");
        MQTTHandleEffectSelectionMessage(message_data,message_data_len);
    }
    else
    if(COMMAND_FILE_TRANSFER_TOPIC == received_topic)
    {
        //Serial.println("File Transfer Data.");
        MQTTHandleFileTransferMessage(message_data,message_data_len);
    }
    else
    {
      ;/* Mistake?... */  
    } 
}

void MQTTHandleEffectSelectionMessage(char *message_data, uint16_t message_len)
{
    /* Only permits changes if there is no file being updated. */
    if(0U == mqtt_client_flags.file_transfer_index)
    {
        uint32_t effect_value = strtoul((const char*)message_data, NULL, 16);;
    
        if(0U == effect_value)
        {
            effect_value = strtoul((const char*)message_data, NULL, 10);
        }
       
        LedsSelectEffectMode(effect_value);
    }
}


void MQTTHandleFileTransferMessageReply(uint16_t chunk_number, uint8_t result)
{
    uint8_t message_data[4] = {(uint8_t)(chunk_number&0x00FF), (uint8_t)((chunk_number/256)&0x00FF), 0, ((0U==result)?0U:1U)};

    mqtt_client.beginMessage(COMMAND_FILE_TRANSFER_REP_TOPIC);

    mqtt_client.write(message_data, 4);            

    mqtt_client.endMessage();  
}


void MQTTHandleFileTransferMessage(char *message_data, uint16_t message_len)
{
    uint16_t chunk_number = message_data[1U];
    chunk_number *= 256U;
    chunk_number += message_data[0U];
    
    mqtt_client_flags.file_transfer_index = chunk_number;

    if(0U == chunk_number)
    {
        Serial.println("    New effects file start.");
        LedsSelectEffectMode(0);
        LiteFileSystemFileClose();
        LiteFileSystemFileOpen(true);
        LiteFileSystemFileWrite(((uint8_t*)message_data)+2U, message_len-2U);
        mqtt_transfered_file_length =  message_len-2U;
        /* Ack start of transfer. */
        MQTTHandleFileTransferMessageReply(chunk_number, 1);
        mqtt_client_flags.file_transfer_index = 1;
    }
    else
    if(0xFFFFU == chunk_number)
    {
        Serial.println("    ");
        Serial.println("    Finished.");
        LiteFileSystemFileClose();
        /* Ack end of transfer. */
        MQTTHandleFileTransferMessageReply(chunk_number, 1);
        mqtt_client_flags.file_transfer_index = 0;
        EffectsFromFileStart();
    }
    else
    if(mqtt_client_flags.file_transfer_index == chunk_number)
    {
        mqtt_transfered_file_length +=  message_len-2U;
        Serial.print("    Chunk: ");Serial.print(chunk_number);Serial.print(" +");Serial.print(message_len-2);Serial.print(" - "); Serial.print(mqtt_transfered_file_length);Serial.print(" bytes \r\n");
        LiteFileSystemFileWrite(((uint8_t*)message_data)+2U, message_len-2U);
        /* Ack received packet. */
        MQTTHandleFileTransferMessageReply(chunk_number, 1);
        mqtt_client_flags.file_transfer_index++;
    }
    else
    if(mqtt_client_flags.file_transfer_index == (chunk_number+1))
    {
        /* Ack last repeated packet. */
        Serial.print("    Chunk: ");Serial.print(chunk_number);Serial.print(" +");Serial.print(message_len-2);Serial.print(" - "); Serial.print(mqtt_transfered_file_length);Serial.print(" bytes - repeated\r\n");
        MQTTHandleFileTransferMessageReply(mqtt_client_flags.file_transfer_index, 1);
    }
    else
    {
        Serial.print("    Chunk: ");Serial.print(chunk_number);Serial.println(" - Sequence Error. ");
        MQTTHandleFileTransferMessageReply(mqtt_client_flags.file_transfer_index, 0);
    }
}

void MQTTHandleRGBMessage(char *message_data, uint16_t message_len)
{
  uint8_t led_buffer[3] = {0U};
  
  if(message_data[0] == '#' && message_len >= 7)
  {
        //Serial.println("MQTT rgb format. ");  

        char temp_num[3] = {0};
  
        temp_num[0] = message_data[1];
        temp_num[1] = message_data[2];
        
        led_buffer[0] = strtoul((const char*)temp_num, NULL, 16)%256;      
        temp_num[0] = message_data[3];
        temp_num[1] = message_data[4];
        led_buffer[1] = strtoul((const char*)temp_num, NULL, 16)%256;      
        
        temp_num[0] = message_data[5];
        temp_num[1] = message_data[6];
        led_buffer[2] = strtoul((const char*)temp_num, NULL, 16)%256;      

        LedsSetRGB(led_buffer[0],led_buffer[1],led_buffer[2]);

    }
    else
    {

      uint8_t total_counter = 0;
      uint8_t temp_counter = 0;
      uint8_t temp_data[6] = {0};
      uint8_t total_numbers = 0;
      bool run = true;
      bool fail = false;
      while(total_numbers < 3 && !fail)
      {
        temp_counter = 0;
        run = true;
        while(temp_counter <= 5 && run)
        {
          temp_data[temp_counter] = message_data[total_counter++];
          if(' ' == temp_data[temp_counter])
          {
            while(' ' == temp_data[temp_counter])
            {
              temp_data[temp_counter] = message_data[total_counter++];
            }
            temp_data[temp_counter] = 0;
            total_counter--;
            run = false;
          }
          else
          if(temp_data[temp_counter] == 0 && total_numbers < 2)
          {
            Serial.println("End string.");
            run = false;
            fail = true;
          }
          else
          if(temp_data[temp_counter] == 0)
          {
            run = false;
          }
          else
          {
            ;
          }
          temp_counter++;
        }
        
        if(!run)
        {
          //Serial.print("New number ");
          if(temp_data[1] == 'x')
          {
              //Serial.println((char*)temp_data);
              led_buffer[total_numbers++] = strtoul((const char*)temp_data, NULL, 16)%256;                  
              //Serial.println("in hex.");  
            
          }
          else
          {
            /* Decimal number */
            //Serial.println((char*)temp_data);
            led_buffer[total_numbers++] = atoi((const char*)temp_data)%256;
            //Serial.println("in decimal.");  
          }
        }
        else
        {
          fail = true;  
        }
        //Serial.println(total_numbers);  
      }
      //Serial.println("Finished.");  
      
      if(!fail)
      {
        LedsSetRGB(led_buffer[0],led_buffer[1],led_buffer[2]);
      }
    }
}
