
#include <driver/ledc.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <Ticker.h>


/* LEDs Constants ------------------------------------------------------------ */
const uint8_t   channelR          PROGMEM = 0;
const uint8_t   channelG          PROGMEM = 1;
const uint8_t   channelB          PROGMEM = 2;

const uint8_t   channelR_adjust   PROGMEM = 0;
const uint8_t   channelG_adjust   PROGMEM = 0;
const uint8_t   channelB_adjust   PROGMEM = 0;

const uint8_t   numberOfBits      PROGMEM = 8;
const uint32_t  frequency         PROGMEM = 200;



/* LEDs Definitions ----------------------------------------------------------- */
#define LEDS_DELAY_RESET        10
#define LEDS_STRIP_LENGTH       900 /* Total number of bytes in 300 RGB leds with 3bytes per led. */
#define SEND_LEDS_STATUS_STOP   4
#define SEND_LEDS_STATUS_START  5

#define LEDS_CLIENT_THREAD_RATE           30/*milliseconds*/
#define LEDS_CLIENT_RECONNECTION_TIMEOUT  (3000/LEDS_CLIENT_THREAD_RATE) /* 5Seconds */

typedef struct
{
    uint32_t initialised:1U;
    uint32_t work_run:1U;
    uint32_t addr_leds_drivr_ready:1U;
    uint32_t addr_leds_drivr_update:1U;
    uint32_t addr_leds_drivr_status:6U;
    uint32_t addr_leds_effect_mode:8U;
    uint32_t new_effect:8U;
    uint32_t unused:6U;
}leds_client_flags_t;


/* LEDs Global Variables ------------------------------------------------------ */
uint8_t  leds_rgb_buffer[900U];
uint8_t  leds_rgb_solid_buffer[3U];
uint8_t  leds_rgb_intensity = 100;

leds_client_flags_t leds_client_flags = {0U};
uint16_t leds_current_effect_strip_length = LEDS_STRIP_LENGTH;

/* LEDs Clients --------------------------------------------------------------- */
Ticker leds_client_thread;


/* LEDs External Functions ---------------------------------------------------- */
void LedsReset(void)
{
    Serial.println(" ");
    Serial.print("LEDs reseting... ");
    
    if(0U != leds_client_flags.initialised)
    {
        leds_client_flags.addr_leds_drivr_ready = 0U;
        //leds_client_flags.addr_leds_drivr_update = 0U;
        leds_client_flags.addr_leds_drivr_status = 0U;
    
        ledcWrite(channelR, 0);
        ledcWrite(channelG, 0);
        ledcWrite(channelB, 0);
    }   
    
    digitalWrite(io_system_pins[ADDR_LEDS_DRVR_RST],LOW);
    delay(500);
    digitalWrite(io_system_pins[ADDR_LEDS_DRVR_RST],HIGH);  
    delay(100);  
    
    Serial.println("done.");
}

void LedsStart(void)
{
    uint8_t temp_leds_rgb_format[3U] = {0U};
    uint8_t temp_var = 3U;
    
    if(0U == leds_client_flags.initialised)
    {
        Serial.println(" ");
        Serial.println("LEDs Start");
        Serial.print("    Starting serial port...");
        
        Serial2.begin(500000,SERIAL_8N1, io_system_pins[ADDR_LEDS_DRVR_RX], io_system_pins[ADDR_LEDS_DRVR_TX]);
        
        Serial.println("done.");
        Serial.print("    Starting PWM channels...");
        ledcSetup(channelR, frequency, numberOfBits);
        ledcSetup(channelG, frequency, numberOfBits);
        ledcSetup(channelB, frequency, numberOfBits);
        
        ledcAttachPin(io_system_pins[PWM_R], channelR);
        ledcAttachPin(io_system_pins[PWM_G], channelG);
        ledcAttachPin(io_system_pins[PWM_B], channelB);
        
        ledcWrite(channelR, 0);
        ledcWrite(channelG, 0);
        ledcWrite(channelB, 0);
        
        
        EepromRead(EEPROM_RGB_FORMAT, temp_leds_rgb_format, &temp_var);
    
        if(temp_leds_rgb_format[0] != 'R' && temp_leds_rgb_format[0] != 'G' && temp_leds_rgb_format[0] != 'B')
        {
            temp_leds_rgb_format[0] = 'R';
            temp_leds_rgb_format[1] = 'G';
            temp_leds_rgb_format[2] = 'B';
            EepromWrite(EEPROM_RGB_FORMAT, temp_leds_rgb_format, 3);
        }
        Serial.println(" done.");
        Serial.print("    Starting LEDs thread...");
        leds_client_thread.attach_ms(LEDS_CLIENT_THREAD_RATE, LedsWorkRun);
        Serial.println(" done.");
        leds_client_flags.initialised = !0U;
    }    
}

void LedsSetRGB(uint8_t r, uint8_t g, uint8_t b)
{
  uint16_t temp_var = 0U;
  
  if(0U != leds_client_flags.initialised)
  {
      Serial.println("LEDs new setting: ");

      Serial.print("    RED: ");
      Serial.println(r);
      Serial.print("    GREEN: ");
      Serial.println(g);
      Serial.print("    BLUE: ");
      Serial.println(b);
      
      leds_rgb_solid_buffer[0U] = r;
      leds_rgb_solid_buffer[1U] = g;
      leds_rgb_solid_buffer[2U] = b;
      //LedsRGBReformat(leds_rgb_solid_buffer);

      temp_var = leds_rgb_intensity; temp_var *= leds_rgb_solid_buffer[0U]; temp_var /= 100;
      ledcWrite(channelR, temp_var);
      temp_var = leds_rgb_intensity; temp_var *= leds_rgb_solid_buffer[1U]; temp_var /= 100;
      ledcWrite(channelG, temp_var);
      temp_var = leds_rgb_intensity; temp_var *= leds_rgb_solid_buffer[2U]; temp_var /= 100;
      ledcWrite(channelB, temp_var); 
  
      if(0U != leds_client_flags.new_effect)
      {
          LedsSetEffectMode(0U);
      }
            
      leds_client_flags.addr_leds_drivr_update = !0U;      
  }
}

void LedsSetIntensity(uint8_t intensity)
{
  uint16_t temp_var = 0;
  if(0U != leds_client_flags.initialised)
  {
      if(intensity > 100)
      {
        intensity = 100;
      }
      else
      if(intensity < 1) 
      {
        intensity = 1;
      }
      
      Serial.println("LEDs new intensity ");
      
      leds_rgb_intensity = intensity;
      
      Serial.print("    Intensity: ");
      Serial.println(leds_rgb_intensity);
      
      if(0U == leds_client_flags.new_effect)
      {
        //Serial.print("    RED: ");
        temp_var = leds_rgb_intensity; temp_var *= leds_rgb_solid_buffer[0U]; temp_var /= 100;
        //Serial.println(temp_var);
        ledcWrite(channelR, temp_var);
        //Serial.print("    GREEN: ");
        temp_var = leds_rgb_intensity; temp_var *= leds_rgb_solid_buffer[1U]; temp_var /= 100;
        //Serial.println(temp_var);
        ledcWrite(channelG, temp_var);
        //Serial.print("    BLUE: ");
        temp_var = leds_rgb_intensity; temp_var *= leds_rgb_solid_buffer[2U]; temp_var /= 100;
        //Serial.println(temp_var);
        ledcWrite(channelB, temp_var);
        leds_client_flags.addr_leds_drivr_update = !0U;      
      }
      
      
  }
}

void LedsWork(void)
{
    if((0U != leds_client_flags.initialised) && (0U != leds_client_flags.work_run))
    {
        if((0U == leds_client_flags.addr_leds_drivr_ready) || (0U != leds_client_flags.addr_leds_drivr_update) || (0U != leds_client_flags.addr_leds_effect_mode) || (leds_client_flags.new_effect != leds_client_flags.addr_leds_effect_mode))
        {
            if(leds_client_flags.new_effect != leds_client_flags.addr_leds_effect_mode)
            {
                LedsSelectEffectMode(leds_client_flags.new_effect);
            }
            
            LedsAddressbleDriver();
        }
        
        leds_client_flags.work_run = 0U;
    }
}

bool LedsReady(void)
{
    return (0U != leds_client_flags.addr_leds_drivr_ready);  
}


void LedsSetEffectMode(uint8_t effect_mode)
{
    leds_client_flags.new_effect = effect_mode;
}


/* LEDs Internal Functions ---------------------------------------------------- */
void LedsSelectEffectMode(uint8_t effect_mode)
{
    leds_client_flags.addr_leds_effect_mode = effect_mode;  
    Serial.println(" ");
    Serial.println("LEDs");
    Serial.print("    New mode: ");Serial.println(effect_mode);
    uint32_t last_len = leds_current_effect_strip_length;
    if(leds_client_flags.addr_leds_effect_mode > 0)
    {
        leds_current_effect_strip_length = EffectsFromFileSetEffect(leds_client_flags.addr_leds_effect_mode);
    }
    else
    {
        leds_current_effect_strip_length = LEDS_STRIP_LENGTH;
    }
    if(last_len != leds_current_effect_strip_length)
    {
      last_len = leds_current_effect_strip_length;
      LedsReset();
    }
    Serial.println("LEDs");
    Serial.print("    Effect length: ");Serial.print(leds_current_effect_strip_length);Serial.println(" bytes.");
    leds_client_flags.new_effect = leds_client_flags.addr_leds_effect_mode;
}


void LedsRGBReformat(uint8_t *rgb)
{
  uint8_t temp_leds_rgb_format[3];
  
  uint8_t temp_rgb[3] = {rgb[0],rgb[1],rgb[2]};
  uint8_t rgb_data_len = 3;
  
  EepromRead(EEPROM_RGB_FORMAT, temp_leds_rgb_format, &rgb_data_len);

  switch(temp_leds_rgb_format[0])
  {
    case 'G':
    {
      rgb[0] = temp_rgb[1];
    }
    break; 
    case 'B':
    {
      rgb[0] = temp_rgb[2];
    }
    break; 
    default:
    break;
  }
  
  switch(temp_leds_rgb_format [1])
  {
    case 'R':
    {
      rgb[1] = temp_rgb[0];
    }
    break; 
    case 'B':
    {
      rgb[1] = temp_rgb[2];
    }
    break; 
    default:
    break;
  }

  switch(temp_leds_rgb_format[2])
  {
    case 'R':
    {
      rgb[2] = temp_rgb[0];
    }
    break; 
    case 'G':
    {
      rgb[2] = temp_rgb[1];
    }
    break; 
    default:
    break;
  }
}


void LedsWorkRun(void)
{
    leds_client_flags.work_run = !0U;
}


void LedsAddressbleDriver(void)
{  
    static uint32_t leds_delay = 0U;
    uint16_t temp_var;
  
    switch(leds_client_flags.addr_leds_drivr_status)
    {
        case 0:
        {
            Serial.println(" ");
            Serial.println("Start Handshake with Addressable LEDs driver.");
            Serial.print("    Starting auto baud...");
            leds_client_flags.addr_leds_drivr_status++;
        }
        break;
      
        case 1:
        {
          
          if(Serial2.read() == 'X')
          {
            /* Empty buffer. */
            while(Serial2.read() != -1){;}
            Serial.println(" done.");
            Serial.print("    Setting LEDs strip length...");
            
            Serial2.print("L");
            uint8_t temp[2] = {(leds_current_effect_strip_length)/256,(leds_current_effect_strip_length)%256}; /* value '900' in 2 bytes 3 132, msb first. */
            Serial2.write(temp,2);
            Serial2.print("\n");
            
            leds_client_flags.addr_leds_drivr_status++;
          }
          else
          {
            Serial.print(".");
            Serial2.write("XXX",3);
          }
        }
        break;
  
        case 2:
        {
          if(Serial2.read() == 'Y')
          {
              Serial.println(" done.");
              Serial.println("    Addressable LEDs driver is ready.");
              leds_client_flags.addr_leds_drivr_ready = !0U;
              leds_client_flags.addr_leds_drivr_status++;
              leds_client_flags.addr_leds_drivr_update = !0U;  
          }
          else
          {
              Serial.print(".");
          }
        }
        break;

        
        case 3:
        {
            if(Serial2.read() == 'R')
            {        
                uint32_t counter = 0;
                if(0U == leds_client_flags.addr_leds_effect_mode)
                {
                    while(counter++ < LEDS_STRIP_LENGTH/3)
                    {
                        uint8_t colr[3] = {0,0,0};
                        Serial2.write(colr,3);
                    }
                }
                leds_client_flags.addr_leds_drivr_status = 5;
                leds_delay = LEDS_DELAY_RESET;
            }
            else
            {
                if(leds_delay > 0)
                {
                    leds_delay--;
                }
                else
                {
                    /* Fill in with nulls. */
                    Serial2.write("\0",1);
                }
            }
        }
        break;
        
        case 4:
        {
            if(Serial2.read() == 'R')
            {        
                uint32_t counter = 0;
                if(0U == leds_client_flags.addr_leds_effect_mode)
                {
                    uint8_t colr[3] = {0};
                    temp_var = leds_rgb_intensity; temp_var *= leds_rgb_solid_buffer[0U]; temp_var /= 100;
                    colr[0U] = temp_var;
                    temp_var = leds_rgb_intensity; temp_var *= leds_rgb_solid_buffer[1U]; temp_var /= 100;
                    colr[1U] = temp_var;
                    temp_var = leds_rgb_intensity; temp_var *= leds_rgb_solid_buffer[2U]; temp_var /= 100;
                    colr[2U] = temp_var;
                    LedsRGBReformat(colr);
                    while(counter++ < LEDS_STRIP_LENGTH/3)
                    {
                        Serial2.write(colr,3);
                    }
                    
                    leds_client_flags.addr_leds_drivr_update = 0U;  
                }
                else
                {
                    while(counter < leds_current_effect_strip_length)
                    {
                        uint8_t colr[3] = {0};
                        temp_var = leds_rgb_intensity; temp_var *= leds_rgb_buffer[counter++]; temp_var /= 100;
                        colr[0U] = temp_var;
                        temp_var = leds_rgb_intensity; temp_var *= leds_rgb_buffer[counter++]; temp_var /= 100;
                        colr[1U] = temp_var;
                        temp_var = leds_rgb_intensity; temp_var *= leds_rgb_buffer[counter++]; temp_var /= 100;
                        colr[2U] = temp_var;
                        LedsRGBReformat(colr);
                        Serial2.write(colr,3);
                    }
                }
                
                leds_client_flags.addr_leds_drivr_status++;
                leds_delay = LEDS_DELAY_RESET;
            }
            else
            {
                if(leds_delay > 0)
                {
                    leds_delay--;
                }
                else
                {
                    /* Fill in with nulls. */
                    Serial2.write("\0",1);
                }
            }
        }
        break;
      
        case 5:
        {
            /* All data sent, wait for Yes signal from driver. */
            if(Serial2.read() == 'Y')
            {
                if(0U != leds_client_flags.addr_leds_effect_mode)
                {
                    EffectsFromFileReadEffect(leds_rgb_buffer);
                }
                
                leds_client_flags.addr_leds_drivr_status--;
                leds_delay = LEDS_DELAY_RESET;
            }
            else
            {
                if(leds_delay > 0)
                {
                    leds_delay--;
                }
                else
                {
                    /* Fill in with nulls. */
                    Serial2.write("\0\0\0\0",4);
                }
            }
          
        }
        break;
  
        default:
        {}
        break;
    }
}
