Howto:
	- Use the python converted

		CMDLINE:> python AddressableLEDsConverterForESP32.py -f "<file_path>\<file_name>.leds"
		output:
		"<file_path>\<file_name>.leds.esp32"

	- Use the batch file to convert all of the files at once
		Edit the batch file on a text editor.
		Add one converter call per every file to be converted.
		Ex.:

		...
		python AddressableLEDsConverterForMQTTESP32.py -f "50LED_FALLING_STARS.leds"
		python AddressableLEDsConverterForMQTTESP32.py -f "50LED_FIREWORKS.leds"
		python AddressableLEDsConverterForMQTTESP32.py -f "50LED_WAVES.leds"
		python AddressableLEDsConverterForMQTTESP32.py -f "50LED_XTMAS.leds"
		...