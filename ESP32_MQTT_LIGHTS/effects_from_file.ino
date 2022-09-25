


/* Effects from file Definitions --------------------------------------------- */
#define EFFECTS_FROM_FILE__MAX_EFFECTS  100

typedef struct __attribute__((packed))
{
    uint32_t effect_len;/* Number of strips. */
    uint16_t strip_len; /* Number of bytes per strip. */
} effects_descriptor_t;

typedef struct __attribute__((packed))
{
    effects_descriptor_t descriptor;
    uint32_t data_address;
} effect_t; 


/* Effects from file Global Variables ----------------------------------------- */
uint8_t effects_from_file_total_effects = 0U;

effect_t effects_from_file_list[EFFECTS_FROM_FILE__MAX_EFFECTS];

uint32_t effect_current_address = 0U;
uint32_t effect_current_index = 0U;


/* Effects from file External Functions --------------------------------------- */
void EffectsFromFileStart(void)
{    
    Serial.println(" ");
    Serial.println("Effects from file."); 
    Serial.print("    Mapping Effects...");
    
    EffectsFromFileListAll();
    
    Serial.println("Done."); 
}


uint16_t EffectsFromFileSetEffect(uint8_t effect)
{
    Serial.println(" ");
    Serial.println("Effects from file."); 
    
    if(effect > 0 && effects_from_file_total_effects > 0)
    {
        Serial.print("    Effect requested: ");  Serial.print(effect);
        effect_current_index = effect - 1;
        if(effect_current_index >= effects_from_file_total_effects)
        {
            effect_current_index = effects_from_file_total_effects-1;
        }
        Serial.print(" - Effect selected: ");  Serial.println(effect_current_index+1);    
        effect_current_address = effects_from_file_list[effect_current_index].data_address;
    
        LiteFileSystemFilePointerSet(effect_current_address);
        
        return effects_from_file_list[effect_current_index].descriptor.strip_len;
    }
    else
    {
        Serial.println("    Not able to select.");  
    }
}



void EffectsFromFileReadEffect(uint8_t* data_destination)
{
    uint16_t counter = 0;
    uint32_t temp = 0;
    //Serial.print("    File read: ");Serial.print(effects_from_file_list[effect_current_index].descriptor.strip_len);Serial.print(" from ");Serial.println(effect_current_address);
    LiteFileSystemFileRead((uint8_t *const) data_destination, effects_from_file_list[effect_current_index].descriptor.strip_len);

    effect_current_address += effects_from_file_list[effect_current_index].descriptor.strip_len;

    temp = effects_from_file_list[effect_current_index].descriptor.effect_len+effects_from_file_list[effect_current_index].data_address;
    
    temp -= effects_from_file_list[effect_current_index].descriptor.strip_len;
    
    if(effect_current_address > temp)
    {
          effect_current_address = effects_from_file_list[effect_current_index].data_address;
          LiteFileSystemFilePointerSet(effect_current_address);
    }
    
}


/* Effects from file Internal Functions --------------------------------------- */

void EffectsFromFileListAll(void)
{
    uint32_t file_index = 0U;
    uint32_t total_file_length = LiteFileSystemFileOpen(false);
    effects_from_file_total_effects = 0U;
    
    while(file_index < total_file_length)
    {
        LiteFileSystemFileRead((uint8_t *const)&effects_from_file_list[effects_from_file_total_effects].descriptor, sizeof(effects_descriptor_t));
        file_index += sizeof(effects_descriptor_t);
        effects_from_file_list[effects_from_file_total_effects].data_address = file_index;
        Serial.print("    Effect "); Serial.print(effects_from_file_total_effects); 
        Serial.print(" found at "); Serial.print(file_index); 
        Serial.print(" with ");Serial.print(effects_from_file_list[effects_from_file_total_effects].descriptor.effect_len); 
        Serial.println(" bytes.");
        file_index += effects_from_file_list[effects_from_file_total_effects].descriptor.effect_len;
        LiteFileSystemFilePointerSet(file_index);
        effects_from_file_total_effects++;
    }
}
