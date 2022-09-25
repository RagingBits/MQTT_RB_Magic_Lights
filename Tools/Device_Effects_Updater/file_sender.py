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
import paho.mqtt.client as mqtt_client

generic_topic_head = "rb_wifi_mqtt_lights_"
output_topic_termination = "_file_transfer"
input_topic_termination = "_file_transfer_reply"
output_topic = ""
input_topic = ""
CHUNK_LEN = 2048
message_received = 10*[0]
mqtt_engine = mqtt_client
mqtt_engine_chunk_counter = 0
sending_timeout = 200


def on_message(client, userdata, msg):
	global message_received
	message_received.put(msg)

def send_start():
	global mqtt_engine
	global mqtt_engine_chunk_counter
	global sending_timeout
	
	mqtt_engine_chunk_counter = -1
	
	return True

def send_finish():
	global mqtt_engine
	global mqtt_engine_chunk_counter
	global sending_timeout
	
	mqtt_engine_chunk_counter = 0xffff
	
	data_to_be_sent_len = mqtt_engine_chunk_counter.to_bytes(2, byteorder = 'little')
	
	mqtt_engine.publish(output_topic,data_to_be_sent_len)

	timeout = sending_timeout
	
	while(timeout != 0):
		if(not message_received.empty()):
			message = message_received.get()
			if(message.topic == input_topic):
				last_chunk_sent = message.payload[1]
				last_chunk_sent *= 256
				last_chunk_sent += message.payload[0]
				success = message.payload[3]
				if((mqtt_engine_chunk_counter == last_chunk_sent) and (1 == success)):
					return True
				else:
					return False
		else:
			timeout -= 1
			time.sleep(0.01)
		
	return False
	
def send_chunk(data):
	global mqtt_engine
	global mqtt_engine_chunk_counter
	global message_received
	global sending_timeout
	
	data_to_be_sent_len = mqtt_engine_chunk_counter.to_bytes(2, byteorder = 'little')
	data_to_be_sent = data_to_be_sent_len + data
	mqtt_engine.publish(output_topic,data_to_be_sent)

	timeout = sending_timeout
	
	while(timeout != 0):
		if(not message_received.empty()):
			message = message_received.get()
			if(message.topic == input_topic):
				last_chunk_sent = message.payload[1]
				last_chunk_sent *= 256
				last_chunk_sent += message.payload[0]
				success = message.payload[3]
				if((mqtt_engine_chunk_counter == last_chunk_sent) and (1 == success)):
					return True
				else:
					return False
		else:
			timeout -= 1
			time.sleep(0.01)
		
	return False
	

def send_file(file):
	global mqtt_engine
	global mqtt_engine_chunk_counter
	
	chunk_len = 0
	
	print("Validate file: ", file)
	print(os.path.exists(file))
	
	if(True == os.path.exists(file)):
		if(True == os.path.isfile(file)):
			print("File found. Start sending...")
			effects_file = open(file, "rb")
			effects_file_length = os.path.getsize(file)
			effects_file_index = 0
			
			send_result = True
			while(effects_file_index < effects_file_length):
			
				if(True == send_result):
					effects_file_index += chunk_len
					mqtt_engine_chunk_counter += 1
					chunk_len = (effects_file_length-effects_file_index)
					if(chunk_len > CHUNK_LEN):
						chunk_len = CHUNK_LEN
						
					f_bytes = effects_file.read(chunk_len)					
				else:
					send_retry -= 1
					if(0 == send_retry):
						effects_file.close()
						return False
				if(effects_file_index < effects_file_length):
					print("Send Chunk: ", mqtt_engine_chunk_counter, " - ", effects_file_index , " of ", effects_file_length)
					send_result = send_chunk(f_bytes)
				
			effects_file.close()
			print("File send.")
			return True
	else:
		print("File not found.")
		return False


def main ():
	global message_received
	global CHUNK_LEN
	global output_topic
	global input_topic
	global generic_topic_head
	global output_topic_termination
	global input_topic_termination
	global mqtt_engine
	global mqtt_engine_chunk_counter
	
	message_received = queue.Queue()
	
	parser = argparse.ArgumentParser(description='MQTT_Lights_Effects_Sender')
	
	parser.add_argument('-f', metavar='path', required=True, help='File of effects *.esp32')
	parser.add_argument('-d', metavar='mqtt_server', required=True, help='Ip of the mqtt server.')
	parser.add_argument('-s', metavar='mqtt_device_serial', required=True, help='Serial number of the mqtt device.')
	
	args = parser.parse_args()
	
	output_topic = generic_topic_head+args.s+output_topic_termination
	input_topic = generic_topic_head+args.s+input_topic_termination
	
	mqtt_server = args.d

	
	effects_list_file_name = args.f
	
	print("\n\rFile list: ", effects_list_file_name)
	
	effects_list_file = Path(effects_list_file_name)
	
	
	
	if(effects_list_file.is_file() == False):
		print("Wrong file or path.")
		return
	
	effects_list_file = open(effects_list_file_name, "r")
	
	#file_length = os.path.getsize(effects_list_file_name)

	mqtt_engine = mqtt_client.Client()	
	mqtt_engine.on_message = on_message
	mqtt_engine.connect(mqtt_server, port=1883, keepalive=60, bind_address="")
	mqtt_engine.loop_start()
	
	print("Subscribe: " + input_topic)
	mqtt_engine.subscribe(input_topic)
				
	
	run = True
	
	#print("Working on: ",os.getcwd())
	
	print("Start sending:")

	success = send_start()
	run = success
	
	while (run == True):
			
		file_to_send = effects_list_file.readline()
		try:
			while(file_to_send[len(file_to_send)-1] == ' ' or file_to_send[len(file_to_send)-1] == '\n' or file_to_send[len(file_to_send)-1] == '\r'):
				if(3 > len(file_to_send)):
					file_to_send = ""
					break
				else:
					file_to_send = file_to_send[0:len(file_to_send)-1]
		except:
			file_to_send = ""
			
		
		if( file_to_send == ""):
			print("All files sent.")
			success = send_finish()
			run = False
		else:
			success = send_file(file_to_send)
			run = success

	
	if(success == True ):
		print("File transfer finished with success.")
	else:
		print("File transfer failed.")
	
	effects_list_file.close()
	mqtt_engine.disconnect()
	mqtt_engine.loop_stop()

if __name__ == "__main__":
    main()
