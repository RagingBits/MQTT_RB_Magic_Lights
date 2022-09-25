
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <SPI.h>
#include "SPIFlash.h"
#include <SPIMemory.h>

#define FLASH_MAN_ID  

#define FLASH_SECTOR_SIZE 4096

extern const uint8_t io_system_pins[];

SPIClass flash_spi(VSPI);
SPIFlash flash(io_system_pins[FLASH_SPI_CS], &flash_spi);


bool FlashStart(void)
{
    uint64_t generic_var = 0U;
    Serial.println(" ");
    Serial.println("Flash Start ");
    Serial.print("    Start SPI... ");
    flash_spi.begin(io_system_pins[FLASH_SPI_CLK], io_system_pins[FLASH_SPI_MISO], io_system_pins[FLASH_SPI_MOSI], io_system_pins[FLASH_SPI_CS]);
    Serial.println("done.");
    Serial.print("    Start Flash IC... ");
    flash.begin(/*MB(32)*/);
    Serial.println("done.");
    generic_var = flash.getManID();
    Serial.print("    Flash Manufacturer ID: ");Serial.println(generic_var);
    generic_var = flash.getJEDECID();
    Serial.print("    Flash JEDEC ID: ");Serial.println(generic_var);
    generic_var = flash.getCapacity();
    Serial.print("    Flash Size: ");
    if(generic_var > (1024U*1024U))
    {
      Serial.print(" ");Serial.print(generic_var/(1024U*1024U));Serial.println("MB");
      generic_var -= ((generic_var/(1024U*1024U))*(1024U*1024U));
    }
    
    if(generic_var > (1024U))
    {
      Serial.print(" "); Serial.print(generic_var/(1024U));Serial.println("KB");
      generic_var -= ((generic_var/(1024U))*(1024U));
    }
    
    if(generic_var > (0))
    {
      Serial.print(" "); Serial.print(generic_var);Serial.println("B");
    }
    
    Serial.println("done.");
    
    return true;
}

uint32_t FlashSize(void)
{
    return flash.getCapacity();
}


uint32_t FlashEraseSectors(const uint32_t address)
{
    uint32_t len = 0;
    uint32_t addr = address;
    if(address < flash.getCapacity())
    {
        addr &= 0xFFFFF000;
        //Serial.println(" ");
        //Serial.print("Flash Erase Sector: "); Serial.print(addr/4096); Serial.print(" address request: "); Serial.print(addr);
        if(flash.eraseSector(addr))
        {
            len++;
            //Serial.println(" Done.");
        }
        else
        {
            //Serial.println(" Failed.");
        }
    }

    return len;
}

uint32_t FlashErase(void)
{
    uint32_t len = 0U;

    //Serial.println(" ");
    //Serial.println("Flash Chip Erase.");
    //Serial.print("    Erasing...");
    if(flash.eraseChip())
    {
        len++;
        //Serial.println(" Done.");
    }
    else
    {
        //Serial.println(" Failed.");
    }
    return len;
}

uint32_t FlashWrite(const uint32_t address, const uint8_t *const data_array, const uint32_t length_of_array)
{
    size_t len = length_of_array;
    uint32_t written_len = len;
    uint32_t address_of_data = address;
    uint8_t *data_p = (uint8_t *)data_array;
    if(address < flash.getCapacity())
    {
        //Serial.println(" ");
        if((flash.getCapacity()-address) < len)
        {
            written_len = len = (flash.getCapacity()-address);
        }
        
        //Serial.print("    Flash write: "); Serial.print(len); Serial.print(" at "); Serial.println(address);
        /*if(!flash.writeByteArray(address, (uint8_t*)data_array, len))
        {
            written_len = 0U;
        } */

        while(written_len--)
        {
            flash.writeByte(address_of_data++,*data_p++, false);  
        }
             
    }

    return written_len;
}


uint32_t FlashRead(const uint32_t address, uint8_t *const data_array, const uint32_t length_of_array)
{
    uint32_t len = length_of_array;
    if(address < flash.getCapacity())
    {
        if((flash.getCapacity()-address) < len)
        {
            len = (flash.getCapacity()-address);
        }
        //Serial.print("    Flash read: "); Serial.print(len); Serial.print(" at "); Serial.println(address);
        if(!flash.readByteArray(address, data_array, len))
        {
            len = 0U;
        }
    }

    return len;
}
