#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "pins_arduino.h"
#include <Ticker.h>


/* IO_Pins Definitions --------------------------------------------------------- */

#define IO_PINS_CLIENT_THREAD_RATE           100/*milliseconds*/
#define IO_PINS_IO_BUTTON_HOLD_TIMEOUT       (3000/IO_PINS_CLIENT_THREAD_RATE) /* 1Seconds */

#if (HARDWARE_VERSION == HW_V_1)
const uint8_t io_system_pins[] PROGMEM = {22,21,23, 25,26,27, 5, 0, 1,3, 17,16, 18,15,2,4};
#else
#error  Hardware version is invalid 
#endif

typedef struct
{
    uint32_t initialised:1U;
    uint32_t button_hold:1U;
    uint32_t button_press:1U;
    uint32_t work_run:1U;
    uint32_t unused:28U;
}io_pins_flags_t;

/* IO_Pins Constants ----------------------------------------------------------- */



/* IO_Pins Variables ----------------------------------------------------------- */
static uint8_t input_status = 0;
static uint32_t input_check = 0;
static uint32_t io_pins_check_inputs_timeout = 0;
static uint32_t io_pins_check_button_hold_timeout = 0;
static io_pins_flags_t io_pins_flags = {0U};

/* IO_Pins Clients ------------------------------------------------------------- */
Ticker io_pins_client_thread;

/* IO_Pins External Functions -------------------------------------------------- */
void IOPinsStart(void)
{
  
    Serial.println(" ");
    Serial.print("IO Pins Start"); 
    if(0U == io_pins_flags.initialised)
    {
        IOPinsInitialise();
        io_pins_client_thread.attach_ms(IO_PINS_CLIENT_THREAD_RATE, &IOPinsRunWork);
        io_pins_flags.initialised = !0U;
    }
    Serial.println("done.");
}


void IOPinsToggleLEDs(uint8_t led)
{
    if(digitalRead(io_system_pins[led]) != 0)
    {
        IOPinsSetLEDs(led, false);
    }
    else
    {
        IOPinsSetLEDs(led, true);
    }
}

void IOPinsSetLEDs(uint8_t led, bool enabled)
{
    if(WIFI_LED == led)
    {
        //Serial.print("WiFi Led ");
        if(enabled)
        {
            //Serial.println("ON");
            digitalWrite(io_system_pins[led], LOW);
        }
        else
        {
            //Serial.println("OFF");
            digitalWrite(io_system_pins[led], HIGH);
        }
    }  
    else
    if(ERROR_LED == led)
    {
        //Serial.print("Error Led ");
        if(enabled)
        {
            //Serial.println("ON");
            digitalWrite(io_system_pins[led], LOW);
        }
        else
        {
            //Serial.println("OFF");
            digitalWrite(io_system_pins[led], HIGH);
        }
    }
    else
    if(WORK_LED == led)
    {
        //Serial.print("Work Led ");
        if(enabled)
        {
            //Serial.println("ON");
            digitalWrite(io_system_pins[led], LOW);
        }
        else
        {
            //Serial.println("OFF");
            digitalWrite(io_system_pins[led], HIGH);
        }
    }
}

void IOPinsWork(void)
{
    uint8_t temp_read = 0U;
    
    if(io_pins_flags.work_run)  
    {
        if(0U == digitalRead(io_system_pins[MEM_ERASE_REQ]))
        {
            io_pins_flags.button_press = !0U;
            if(io_pins_check_button_hold_timeout == IO_PINS_IO_BUTTON_HOLD_TIMEOUT)
            {
                io_pins_flags.button_hold = !0U;
            }
            else
            {
                io_pins_check_button_hold_timeout++;
            }
        }
        else
        {
            io_pins_flags.button_press = 0U;
            io_pins_flags.button_hold = 0U;
            io_pins_check_button_hold_timeout = 0;
        }
        
      
        io_pins_flags.work_run = 0U;  
    }
}


bool IOPinsButtonPress(void)
{
    return (0U != io_pins_flags.button_press);
}


bool IOPinsButtonHold(void)
{
    return (0U != io_pins_flags.button_hold);
}


/* IO_Pins Internal Functions -------------------------------------------------- */

void IOPinsRunWork(void)
{
    io_pins_flags.work_run = !0U;
}


void digitalToggle(uint8_t io)
{
    if(digitalRead(io) != 0)
    {
        digitalWrite(io,LOW);
    }
    else
    {
        digitalWrite(io,HIGH);
    }  
}



void IOPinsInitialise(void)
{
  uint8_t counter = 0;
  Serial.println(" ");
  Serial.println("IO Pins Initialisation");
  
  /* Adressable leds driver. */    
  Serial.print("    Initialise RGB LED pins...");
  
  pinMode(io_system_pins[PWM_R], OUTPUT);
  digitalWrite(io_system_pins[PWM_R],LOW);
  pinMode(io_system_pins[PWM_G], OUTPUT);
  digitalWrite(io_system_pins[PWM_G],LOW);  
  pinMode(io_system_pins[PWM_B], OUTPUT);
  digitalWrite(io_system_pins[PWM_B],LOW);

  pinMode(io_system_pins[ADDR_LEDS_DRVR_RST], OUTPUT);
  digitalWrite(io_system_pins[ADDR_LEDS_DRVR_RST],LOW);
  Serial.println(" done.");

  Serial.print("    Initialise Flash SPI pins...");
  pinMode(io_system_pins[FLASH_SPI_CLK], OUTPUT);
  //pinMode(io_system_pins[FLASH_SPI_MISO], FUNCTION_3);
  pinMode(io_system_pins[FLASH_SPI_MOSI], OUTPUT);
  pinMode(io_system_pins[FLASH_SPI_CS], OUTPUT);
  
  digitalWrite(io_system_pins[FLASH_SPI_CS],HIGH);
  Serial.println(" done.");

  /* System UI leds. */
  Serial.print("    Initialise UI LED pins...");
  pinMode(io_system_pins[WIFI_LED], OUTPUT);
  digitalWrite(io_system_pins[WIFI_LED],HIGH);
  pinMode(io_system_pins[WORK_LED], OUTPUT);
  digitalWrite(io_system_pins[WORK_LED],HIGH);
  pinMode(io_system_pins[ERROR_LED], OUTPUT);  
  digitalWrite(io_system_pins[ERROR_LED],HIGH);
  Serial.println(" done.");
  
  /* System UI inputs. */
  Serial.print("    Initialise UI button pin...");
  pinMode(io_system_pins[MEM_ERASE_REQ], INPUT_PULLUP);
  Serial.println(" done.");

}
