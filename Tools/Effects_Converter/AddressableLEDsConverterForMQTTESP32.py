import subprocess
import os
import sys
import time
import queue
from subprocess import PIPE, Popen
from threading  import Thread
import io
import serial
import argparse
import os.path
from pathlib import Path
import sys


def effects_start(file):
	
	success = False
	
	run = True	
	while(run == True):
		data = file.readline()
		
		if (not data):
			run = False
		else:
			if(data[:5]	== "{anim"):
				success = True
				run = False
				
	return success	

	
def effects_get_next_array(file):
	
	number_of_leds = 0
	array = []	
	
	run = True	
	while(run == True):
		data = file.readline()
		
		if (not data):
			run = False
		else:
			array_ = data.split(':')	
	
			if(array_[0] == 'w'):
				number_of_leds = int(array_[1])
			elif(array_[0] == 'r'):
				array = array_[1].split(' ')
				if(number_of_leds == 0):
					number_of_leds = 1
				run = False
				
	return number_of_leds, array
	
def effects_get_info_from_array_item(item):
	
	r = 0
	g = 0
	b = 0
	try:
		b = int(item[:2], 16)
		r = int(item[2:4], 16)
		g = int(item[4:], 16)
	except:
		pass
		
	return g,r,b

def main ():

	parser = argparse.ArgumentParser(description='Matrix Studio Converter For MQTT Magic Lights')
	
	parser.add_argument('-f', metavar='path', required=True, help='file of effects')
	
	args = parser.parse_args()

	effects_file_name = args.f
	effects_file = Path(effects_file_name)
	
	
	
	if(effects_file.is_file() == False):
		print("Wrong file or path.")
		return
	
	output_file_name = effects_file_name + ".esp32"
	output_file = open(output_file_name, "wb")
	
	
	effects_file = open(effects_file_name, "r")
	if(effects_start(effects_file) == False ):
		print("Invalid file.")
		return 
	
	total_number_leds, array = effects_get_next_array(effects_file)
	
	#Save the number of Leds
	
	total_leds_data_len = total_number_leds * 3
	total_data_size  = total_leds_data_len.to_bytes(4, byteorder = 'little')			
	
	# To be filled with the total data length of the effect.
	total_effect_length = 0
	temp_data = bytearray([0, 0, 0, 0]) 
	output_file.write(temp_data)
	
	temp_data = bytearray([total_data_size[0], total_data_size[1]])
	output_file.write(temp_data)
		
	run = True
	while (run == True):
		
		counter = 0
		while(counter < total_number_leds):
			r,g,b = effects_get_info_from_array_item(array[counter])
			red = r&0x00FF
			green = g&0x00FF
			blue = b&0x00FF				
			temp_data=bytearray([red,green,blue])
			output_file.write(temp_data)
			counter = counter + 1
			total_effect_length += 3
		
		num_items, array = effects_get_next_array(effects_file)
			
		if(not array):
			bytes_ = total_effect_length.to_bytes(4, byteorder = 'little')			
			output_file.seek(0)
			output_file.write(bytearray([bytes_[0],bytes_[1],bytes_[2],bytes_[3]]))
			effects_file.close()
			output_file.close()
			run = False
		



if __name__ == "__main__":
    main()
