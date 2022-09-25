
/*
  The only purpose of this simplistic file system is to provide a flash wear leveling.
  Simple methodology of clusterised distribution that provides enough information
  to have a cold system start.
  Simgle file support
  
  Future improvements:
  Account for bad sectors.
  Support Multiple Files.
*/

/* Definitions ----------------------------------------------------------- */
#define KBt  *1024U
#define MBt  *1024U*1024U
#define FILE_CLUSTER_SIZE 1024
#define FILE_CLUSTER_HEAD_SIZE  sizeof(lite_file_system_cluster_header_t)

#define FILE_MAX_NUMBER_CLUSTERS  (lite_file_system_flash_size/FILE_CLUSTER_SIZE)
#define FILE_MAX_DATA_SIZE (lite_file_system_flash_size-(FILE_CLUSTER_HEAD_SIZE*FILE_MAX_NUMBER_CLUSTERS))

#define FS_UNINITIALISED  0
#define FS_READY          1


#define FS_FILE_CLOSED  0
#define FS_FILE_READ    1
#define FS_FILE_WRITE   2
#define FS_FILE_LOCKED  3
typedef struct
{
    uint32_t initialised : 1U;
    uint32_t operation : 2U;
    uint32_t unused : 29U;
}lite_file_system_flags_t;

typedef struct
{
    uint32_t free_cluster : 1U;
    uint32_t valid_cluster : 1U;
    uint32_t unused : 6U;
    uint32_t cluster_seq_num : 12U;
    uint32_t cluster_length : 12U;
    //uint32_t next_cluster : 32U;
}lite_file_system_cluster_header_t;

typedef struct
{
    lite_file_system_cluster_header_t cluster_header;
    uint8_t cluster_data[FILE_CLUSTER_SIZE-FILE_CLUSTER_HEAD_SIZE];
}lite_file_system_cluster_t;


typedef struct
{
    uint32_t address;
    uint32_t file_data_index;
    uint32_t cluster_index : 16U;
    uint32_t cluster_data_index : 16U;
    lite_file_system_cluster_t cluster;
}lite_file_system_stream_t;


/* Global Variables ------------------------------------------------------ */
lite_file_system_flags_t lite_file_system_flags = {0U};

uint32_t lite_file_system_flash_size = 0U;

uint32_t lite_file_system_file_address = 0U;

uint32_t lite_file_system_file_size = 0U;

uint32_t lite_file_system_file_length = 0U;

lite_file_system_stream_t lite_file_system_file_stream = {0U};


/* External Functions ---------------------------------------------------- */
bool LiteFileSystemInit(void)
{
    if(FS_UNINITIALISED == lite_file_system_flags.initialised)
    {
        FlashStart();
        
        Serial.println(" ");
        Serial.println("File System Start ");
        
        lite_file_system_flash_size = FlashSize();

        LiteFileSystemFindFile();
        
        lite_file_system_flags.initialised = FS_READY;          
        Serial.println("done.");
    }
}

uint32_t LiteFileSystemFileOpen(bool write_)
{  
    Serial.println(" ");
    Serial.println("File Open");
    Serial.print("    File ");
    if(write_)
    {
        Serial.print("write started at ");    
        uint32_t new_ddress = (lite_file_system_file_address+(FILE_CLUSTER_SIZE*lite_file_system_file_size));

        if(new_ddress > (lite_file_system_flash_size-FILE_CLUSTER_SIZE))
        {
            new_ddress = 0U;
        }
        
        /* Erase new address sector if needed. */
        if(FILE_CLUSTER_HEAD_SIZE == FlashRead(new_ddress, (uint8_t *const)&lite_file_system_file_stream.cluster.cluster_header, FILE_CLUSTER_HEAD_SIZE))
        {
            if(0U == lite_file_system_file_stream.cluster.cluster_header.free_cluster)
            {
                FlashEraseSectors(new_ddress);
                FlashRead(new_ddress, (uint8_t *const)&lite_file_system_file_stream.cluster.cluster_header, FILE_CLUSTER_HEAD_SIZE);
            }
        }

        Serial.println(new_ddress);
        
        lite_file_system_file_stream.cluster.cluster_header.free_cluster = 0U;
        lite_file_system_file_stream.cluster.cluster_header.cluster_seq_num = 0U;
        lite_file_system_file_stream.cluster.cluster_header.cluster_length = 0U;
    
        LiteFileSystemInvalidateFile();
    
        lite_file_system_file_address = new_ddress;
        lite_file_system_file_size = 0U;
        lite_file_system_file_length = 0U;
        lite_file_system_flags.operation = FS_FILE_WRITE;
        
    }
    else
    {
        Serial.println("read started.");    
        lite_file_system_flags.operation = FS_FILE_READ;
        FlashRead(lite_file_system_file_address, (uint8_t *const)&lite_file_system_file_stream.cluster, FILE_CLUSTER_SIZE);
    }
    
    lite_file_system_file_stream.address = lite_file_system_file_address;
    lite_file_system_file_stream.cluster_index = 0U;
    lite_file_system_file_stream.file_data_index = 0U;
    lite_file_system_file_stream.cluster_data_index = 0U;

    return lite_file_system_file_length;
    
}



void LiteFileSystemFileClose(void)
{
    /* Flush current cluster to flash. */
    if(FS_FILE_CLOSED != lite_file_system_flags.operation)
    {
        if(0U != lite_file_system_file_stream.cluster_data_index)
        {
            //Serial.print("Write cluster at: ");Serial.println(lite_file_system_file_stream.address);
            FlashWrite(lite_file_system_file_stream.address , (uint8_t *const)&lite_file_system_file_stream.cluster, FILE_CLUSTER_SIZE);
            lite_file_system_file_size++;
        }
        Serial.println("    File Closed.");
        lite_file_system_flags.operation = FS_FILE_CLOSED;
    }
}

uint32_t LiteFileSystemFileRead(uint8_t *const data_bytes, uint32_t data_length)
{
    uint32_t data_count = data_length;
    uint8_t *destination_data = data_bytes;
    
    if(FS_FILE_READ == lite_file_system_flags.operation)
    {
        if(data_count > (lite_file_system_file_length-lite_file_system_file_stream.file_data_index))
        {
            data_count = (lite_file_system_file_length-lite_file_system_file_stream.file_data_index);
        }
        
        while(data_count)
        {
            /* Read from current cluster. */
            uint32_t current_cluster_data_available = lite_file_system_file_stream.cluster.cluster_header.cluster_length - lite_file_system_file_stream.cluster_data_index;
            if(data_count < current_cluster_data_available)
            {
                current_cluster_data_available = data_count;
            }
            LiteFileSystemMemCpy(destination_data, (const uint8_t*)&lite_file_system_file_stream.cluster.cluster_data[lite_file_system_file_stream.cluster_data_index], current_cluster_data_available);
            lite_file_system_file_stream.file_data_index += current_cluster_data_available;
            lite_file_system_file_stream.cluster_data_index += current_cluster_data_available;
            data_count -= current_cluster_data_available;
            destination_data += current_cluster_data_available;
            
            /* Load in the next cluster, if needed. */
            if(data_count)
            {
                lite_file_system_file_stream.address += FILE_CLUSTER_SIZE;
                if(lite_file_system_file_stream.address > lite_file_system_flash_size)
                {
                    lite_file_system_file_stream.address = 0U;
                }
                lite_file_system_file_stream.cluster_index++;
                //Serial.print("    File read: ");Serial.println(lite_file_system_file_stream.address);
                lite_file_system_file_stream.cluster_data_index = 0U;
                FlashRead(lite_file_system_file_stream.address, (uint8_t *const)&lite_file_system_file_stream.cluster, FILE_CLUSTER_SIZE);
            }
        }

        //Serial.println(" done.");   
    }
    
    return data_length - data_count;
}



bool  LiteFileSystemFilePointerSet(const uint32_t data_new_pointer)
{
    uint32_t new_position = data_new_pointer;
    
    if(FS_FILE_READ == lite_file_system_flags.operation)
    {

        if(new_position == 0U)
        {
            if(lite_file_system_file_stream.address != lite_file_system_file_address)
            {
                lite_file_system_file_stream.address = lite_file_system_file_address;
                FlashRead(lite_file_system_file_stream.address, (uint8_t *const)&lite_file_system_file_stream.cluster, FILE_CLUSTER_SIZE);
            }
            
            lite_file_system_file_stream.cluster_index = 0U;
            lite_file_system_file_stream.file_data_index = 0U;
            lite_file_system_file_stream.cluster_data_index = 0U;
            
        }
        else
        {
          if(new_position >= lite_file_system_file_length)
          {
              /* Just jump to last cluster. */
              new_position = lite_file_system_file_length-1;      
          }
          
          if(new_position > lite_file_system_file_stream.file_data_index)
          {
              /* Move forward. */
              while(lite_file_system_file_stream.file_data_index != new_position)
              {
                  if(new_position < (lite_file_system_file_stream.file_data_index+(lite_file_system_file_stream.cluster.cluster_header.cluster_length - lite_file_system_file_stream.cluster_data_index)))
                  {
                      /* Cluster where the file pointer must be*/  
                      lite_file_system_file_stream.cluster_data_index = (new_position - lite_file_system_file_stream.file_data_index);
                      lite_file_system_file_stream.file_data_index = new_position;
                      FlashRead(lite_file_system_file_stream.address+FILE_CLUSTER_HEAD_SIZE, (uint8_t *const)&lite_file_system_file_stream.cluster+FILE_CLUSTER_HEAD_SIZE, FILE_CLUSTER_SIZE-FILE_CLUSTER_HEAD_SIZE);
  
                      /* Finished. */
                  }
                  else
                  {
                      /* Move to next cluster. */
                      lite_file_system_file_stream.file_data_index += (lite_file_system_file_stream.cluster.cluster_header.cluster_length - lite_file_system_file_stream.cluster_data_index);
                      
                      lite_file_system_file_stream.address += FILE_CLUSTER_SIZE;
                      if(lite_file_system_file_stream.address > (lite_file_system_flash_size-FILE_CLUSTER_SIZE))
                      {
                          lite_file_system_file_stream.address = 0U;
                      }
                      
                      FlashRead(lite_file_system_file_stream.address, (uint8_t *const)&lite_file_system_file_stream.cluster, FILE_CLUSTER_HEAD_SIZE);
                      lite_file_system_file_stream.cluster_data_index = 0U;
                      lite_file_system_file_stream.cluster_index++;
                  }
              }
              
          }
          else 
          if(new_position < lite_file_system_file_stream.file_data_index)
          {
              /* Move Backwards. */
              while(lite_file_system_file_stream.file_data_index != new_position)
              {
                  if(new_position > (lite_file_system_file_stream.file_data_index - lite_file_system_file_stream.cluster_data_index))
                  {
                      /* Cluster where the file pointer must be. */  
                      lite_file_system_file_stream.cluster_data_index = (lite_file_system_file_stream.cluster_data_index - (lite_file_system_file_stream.file_data_index-new_position));
                      lite_file_system_file_stream.file_data_index = new_position;
                      FlashRead(lite_file_system_file_stream.address+FILE_CLUSTER_HEAD_SIZE, (uint8_t *const)&lite_file_system_file_stream.cluster+FILE_CLUSTER_HEAD_SIZE, FILE_CLUSTER_SIZE-FILE_CLUSTER_HEAD_SIZE);
                      
                      /* Finished. */
                  }
                  else
                  {
                      /* Move to next cluster. */
                      lite_file_system_file_stream.file_data_index -= (lite_file_system_file_stream.cluster_data_index);
                      lite_file_system_file_stream.address -= FILE_CLUSTER_SIZE;
                      if(lite_file_system_file_stream.address > lite_file_system_flash_size)
                      {
                          lite_file_system_file_stream.address = lite_file_system_flash_size-FILE_CLUSTER_SIZE;
                      }
                      FlashRead(lite_file_system_file_stream.address, (uint8_t *const)&lite_file_system_file_stream.cluster, FILE_CLUSTER_HEAD_SIZE);
                      
                      lite_file_system_file_stream.cluster_data_index = lite_file_system_file_stream.cluster.cluster_header.cluster_length;
                      
                      lite_file_system_file_stream.cluster_index--;
                  }
              }
          }
        }

        //Serial.println(" done.");   
        return true;
    }
    
    return false;
}




uint32_t LiteFileSystemFileWrite(const uint8_t *const data_bytes, uint32_t data_length_)
{
    uint32_t data_length = data_length_;
    if(FS_FILE_WRITE == lite_file_system_flags.operation)
    {
        const uint8_t *origin_data = (const uint8_t*)data_bytes;
        if((FILE_MAX_DATA_SIZE-lite_file_system_file_length) < data_length)
        {
            data_length = (FILE_MAX_DATA_SIZE-lite_file_system_file_length);
        }

        //Serial.print("    Writting ");Serial.print(data_length);Serial.print(" of ");Serial.print(data_length_);Serial.println(" bytes to buffer cluster.");  

        while(0U != data_length)
        {

            uint32_t current_cluster_data_available = (FILE_CLUSTER_SIZE-FILE_CLUSTER_HEAD_SIZE)-lite_file_system_file_stream.cluster_data_index;

            if(current_cluster_data_available > data_length)
            {
                current_cluster_data_available = data_length;
            }
            LiteFileSystemMemCpy(&lite_file_system_file_stream.cluster.cluster_data[lite_file_system_file_stream.cluster_data_index], (const uint8_t*)origin_data, current_cluster_data_available);
            
            lite_file_system_file_stream.cluster_data_index += current_cluster_data_available;
            lite_file_system_file_stream.cluster.cluster_header.cluster_length += current_cluster_data_available;
            lite_file_system_file_stream.file_data_index += current_cluster_data_available;
            lite_file_system_file_length += current_cluster_data_available;
            origin_data += current_cluster_data_available;
            data_length -= current_cluster_data_available;

            if((0U != data_length) || (lite_file_system_file_stream.cluster.cluster_header.cluster_length == (FILE_CLUSTER_SIZE-FILE_CLUSTER_HEAD_SIZE)))
            {
                //Serial.println("    Write cluster. "); 
                FlashWrite(lite_file_system_file_stream.address , (uint8_t *const)&lite_file_system_file_stream.cluster, FILE_CLUSTER_SIZE);
                
                lite_file_system_file_stream.address += FILE_CLUSTER_SIZE;
                if(lite_file_system_file_stream.address > (lite_file_system_flash_size-FILE_CLUSTER_SIZE))
                {
                    lite_file_system_file_stream.address = 0U;
                }
                
                lite_file_system_file_stream.cluster_data_index = 0U;
                lite_file_system_file_stream.cluster_index++;
                lite_file_system_file_size++;

                FlashRead(lite_file_system_file_stream.address , (uint8_t *const)&lite_file_system_file_stream.cluster, FILE_CLUSTER_HEAD_SIZE);
                if(0xFFFFFFFFU != *((uint32_t*)&lite_file_system_file_stream.cluster))
                {
                    FlashEraseSectors(lite_file_system_file_stream.address);
                    FlashRead(lite_file_system_file_stream.address , (uint8_t *const)&lite_file_system_file_stream.cluster, FILE_CLUSTER_HEAD_SIZE);
                }

                FlashRead(lite_file_system_file_stream.address+FILE_CLUSTER_HEAD_SIZE , ((uint8_t *const)&lite_file_system_file_stream.cluster)+FILE_CLUSTER_HEAD_SIZE, (FILE_CLUSTER_SIZE-FILE_CLUSTER_HEAD_SIZE));
                    
                lite_file_system_file_stream.cluster.cluster_header.free_cluster = 0U;
                lite_file_system_file_stream.cluster.cluster_header.cluster_seq_num = lite_file_system_file_stream.cluster_index;
                lite_file_system_file_stream.cluster.cluster_header.cluster_length = 0U;
            }
        }
    }

    return data_length_ - data_length;
}



/* Internal Functions ---------------------------------------------------- */

void LiteFileSystemMemCpy(uint8_t * dest, const uint8_t * orig, uint32_t len)
{

    while(len--)
    {
        *dest++ = *orig++;
    }

  /*
    uint32_t length_of_data = len;
    uint8_t  len_of_switch = length_of_data%4;
    uint32_t *orig32_p = (uint32_t *)orig;
    uint32_t *dest32_p = (uint32_t *)dest;
    while(length_of_data)
    {
        switch(len_of_switch)
        {
            case 3:
              *dest++ = *orig++;
            case 2:
              *dest++ = *orig++;
            case 1:
              *dest++ = *orig++;
              length_of_data -= len_of_switch;
              len_of_switch = length_of_data%16;
            break;
  
            case 0:
              *dest32_p++ = *orig32_p++;
              length_of_data -= 4U;
            case 12:
              *dest32_p++ = *orig32_p++;
              length_of_data -= 4U;
            case 8:
              *dest32_p++ = *orig32_p++;
              length_of_data -= 4U;
            case 4:
              *dest32_p++ = *orig32_p++;
              length_of_data -= 4U;
              len_of_switch = 0;
        }
    }
    */
}

void LiteFileSystemInvalidateFile(void)
{  
    /* Run all the file clusters and invlidates them. */
    
    uint32_t counter = lite_file_system_file_size;
    uint32_t address_interator = lite_file_system_file_address;
    lite_file_system_cluster_header_t header = {0U};

    //Serial.print("    Invalidating file with ");Serial.print(counter);Serial.println(" clusters.");
    while(counter)
    {
        //Serial.print("    Invalidating cluster ");Serial.println(counter);
        FlashWrite(address_interator, (uint8_t *const)&header, FILE_CLUSTER_HEAD_SIZE);
        address_interator += FILE_CLUSTER_SIZE;
        if(address_interator >= lite_file_system_flash_size)
        {
            address_interator = 0U;
        }
        counter--;
    }
}


bool LiteFileSystemFindFile(void)
{  
    uint32_t lite_file_system_file_end_address_last = 0U;
    uint32_t lite_file_system_file_end_address = 0U;
    lite_file_system_cluster_header_t header = {0U};
    bool success = true;
    bool file_found = false;
    
    lite_file_system_file_address = 0U;
    lite_file_system_file_size = 0U;
    lite_file_system_file_length = 0U;

    Serial.println("    Searching for a file ...");
    while((lite_file_system_file_address+FILE_CLUSTER_HEAD_SIZE) < lite_file_system_flash_size)
    {
        
        if(FILE_CLUSTER_HEAD_SIZE == FlashRead(lite_file_system_file_address, (uint8_t *const)&header, FILE_CLUSTER_HEAD_SIZE))
        {
            if((0U != header.valid_cluster) && (0U == header.free_cluster) && (0U == header.cluster_seq_num))
            {
                lite_file_system_file_size += 1;
                lite_file_system_file_length += header.cluster_length;
                lite_file_system_file_end_address = lite_file_system_file_address + FILE_CLUSTER_SIZE;
                
                lite_file_system_file_end_address_last = lite_file_system_file_address;
                
                if(lite_file_system_file_end_address > (lite_file_system_flash_size-FILE_CLUSTER_SIZE))
                {
                    lite_file_system_file_end_address = 0U;
                }
                /*Serial.println(" ");
                Serial.print("    Start Found at address: ");
                Serial.println(lite_file_system_file_address);*/
                file_found = true;
                break;
            }
            else
            {
                //Serial.print(".");
                lite_file_system_file_address += FILE_CLUSTER_SIZE;
            }
        }
        else
        {
            success = false;
        }
    }

    if(success)
    {
        if(file_found)
        {
            //file_found = false;
                        
            while(lite_file_system_file_address != lite_file_system_file_end_address)
            {
                if(FILE_CLUSTER_HEAD_SIZE == FlashRead(lite_file_system_file_end_address, (uint8_t *const)&header, FILE_CLUSTER_HEAD_SIZE))
                {
                    //Serial.print("    Check end: ");Serial.println(lite_file_system_file_end_address);
                    if((0U == header.valid_cluster) || (0U != header.free_cluster))
                    {
                        break;
                    }
                    else
                    {
                        //Serial.print(".");
                        lite_file_system_file_end_address_last = lite_file_system_file_end_address;
                        lite_file_system_file_end_address += FILE_CLUSTER_SIZE;
                        if(lite_file_system_file_end_address > (lite_file_system_flash_size-FILE_CLUSTER_SIZE))
                        {
                            lite_file_system_file_end_address = 0U;
                        }
                        
                        lite_file_system_file_size += 1;
                        lite_file_system_file_length += header.cluster_length;
                    }
                }
                else
                {
                    success = false;
                }
            }

            if(file_found)
            {
                Serial.print("    Found from address: ");
                Serial.print(lite_file_system_file_address);
                Serial.print(" to ");
                Serial.print(lite_file_system_file_end_address);
                Serial.print(", with ");
                Serial.print(lite_file_system_file_length);
                Serial.println(" bytes.");
            }
            else
            {
                Serial.println("    File System error. ");
                FlashErase();
                lite_file_system_file_address = 0U;
                lite_file_system_file_length = 0U;
                Serial.print("    Next File at: ");
                Serial.println(lite_file_system_file_address);
            }
        }
        else
        {
            lite_file_system_file_address = 0U;
            lite_file_system_file_length = 0U;
            lite_file_system_file_size = 0U;
            Serial.println("    Not found.");
            Serial.print("    Next File at: ");
            Serial.println(lite_file_system_file_address);
        }
    }
    
    return success;
}
