# FlexTrak STM32 

This is the firmware for the STM32 FlexTrak board.

## **Firmware Update**

To program the device you need:

- ​	ST-LINK V2 programmer
- ​	Connector wires between programmer and extension connector on FlexTrak board

You need to connect the SWDIO, SWCLK and GND pins.  **Do not connect** **the** **power (Vcc) pin**. During programming, the tracker board can be connected to the Pi or on its own, with power supplied from the Pi or via the battery connector.

The simplest method is to use the STM firmware programmer software, load the .bin file from our repository, and load onto the FlexTrak board.

## **Firmware Development**

If you wish to modify the firmware yourself, then you can do so using the popular Arduino IDE.  There are some steps you must go through within that IDE:



- ​	Go to File → Preferences, and add the following URL to the “Additional Boards Manager URLs” box, separated from any other entries with a semi-colon: https://adafruit.github.io/arduino-board-index/package_adafruit_index.json
- ​	Under Tools → Board, choose “Generic STM32F4 Series”
- ​	Under Tools → Board Part Number, choose “Generic F401CCUx”
- ​	Under Tools → U(S)ART Support, choose “Enabled (No Generic “Serial”)
- ​	Under Tools → C Runtime Library, choose “Newlib Nano + Float Printf/Scanf”
- ​	Under Tools → Upload Method, choose “STM32CubeProgrammer (SWD)”
- ​	Under Tools → Manage Libraries, search for and install “FlashStorage_STM32”

Download the firmware from https://github.com/daveake/FlexSTM32, and open in Arduino.  With the tracker board powered and connected to the programmer just click the Arduino IDE program button to program the tracker.

For debugging, purchase a “USB-TTL” serial adapter, connected to the Tx/Rx/GND pins on the programming connector.  Then you can watch the serial communications from the tracker using a terminal program or the terminal in the Arduino IDE.  Just set the correct COM port and set the baud rate to 38,400.

## Releases

V1.05	-	Fixed bug in landing prediction
					Tidy serial output and make it more consistent				

V1.04	-	Fix to landing_altitude acceptable range

V1.03	-	Default field list includes external temperature

V1.02	-	Change APRS pins to suit latest board

V1.01	-	Send APRS settings

V1.00	-	Prototype software
		Small mods to suit first prototype board
		Added GPS speed and direction
		Added sending of current settings to host
		Add uplink enable flag
		Added product and description to serial protocol
		Send temperatures in standard serial format